// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "valve_base.h"

#include <string.h>
#include <supla/channels/channel_state.h>
#include <supla/storage/canonical_config.h>
#include <supla/actions.h>
#include <supla/log_wrapper.h>
#include <supla/network/network.h>
#include <supla/protocol/protocol_layer.h>
#include <supla/storage/config_tags.h>
#include <supla/storage/storage.h>
#include <supla/time.h>

using Supla::Control::ValveBase;

Supla::Control::ValveConfig::ValveConfig() {
  memset(sensorData, 255, sizeof(sensorData));
  closeValveOnFloodType = 0;
  memset(reserved, 0, sizeof(reserved));
}

bool Supla::Control::ValveConfiguration::valid() const {
  if (closeValveOnFloodType > SUPLA_VALVE_CLOSE_ON_FLOOD_TYPE_ON_CHANGE)
    return false;
  for (const auto &sensor : sensorData)
    if (!sensor.reference().valid()) return false;
  return true;
}

ValveBase::~ValveBase() {
  for (uint8_t i = 0; i < SUPLA_VALVE_SENSOR_MAX; ++i)
    releaseChannelConsumer(0x30000 + (getChannelNumber() + 1) * 32 + i);
}

Supla::ChannelReference ValveBase::sensorReference(uint8_t slot) const {
  return slot < SUPLA_VALVE_SENSOR_MAX ? config.sensorData[slot].reference()
                                      : ChannelReference{};
}

void ValveBase::reconcileChannelDependencies() {
  for (uint8_t i = 0; i < SUPLA_VALVE_SENSOR_MAX; ++i)
    consumeChannelState(sensorReference(i),
                        0x30000 + (getChannelNumber() + 1) * 32 + i);
}

void ValveBase::cleanupLegacy() {
  lastCleanupMs = millis();
  char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
  generateKey(key, Supla::ConfigTag::ValveCfgTag);
  cleanupPending =
      !Supla::StorageDetail::cleanupLegacyConfig(key, cleanupPending);
}

bool ValveBase::persistConfig(const ValveConfiguration &candidate,
                              uint32_t function) {
  if (!candidate.valid()) return false;
  ValveStoredConfigV2 record;
  record.function = function;
  record.config = candidate;
  char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
  generateKey(key, "valve_cfg2");
  return Supla::StorageDetail::persistCanonicalConfig(
      key, record, &persistenceUncertain, getChannelNumber());
}

ValveBase::ValveBase(bool openClose) {
  channel.setFlag(SUPLA_CHANNEL_FLAG_RUNTIME_CHANNEL_CONFIG_UPDATE);
  channel.setFlag(SUPLA_CHANNEL_FLAG_FLOOD_SENSORS_SUPPORTED);
  if (openClose) {
    channel.setType(SUPLA_CHANNELTYPE_VALVE_OPENCLOSE);
    channel.setDefaultFunction(SUPLA_CHANNELFNC_VALVE_OPENCLOSE);
  } else {
    channel.setType(SUPLA_CHANNELTYPE_VALVE_PERCENTAGE);
    channel.setDefaultFunction(SUPLA_CHANNELFNC_VALVE_PERCENTAGE);
  }
  usedConfigTypes.set(SUPLA_CONFIG_TYPE_DEFAULT);
}

void ValveBase::onInit() {
  lastSensorsCheckTimestamp = millis();
}

void ValveBase::iterateAlways() {
  reconcileChannelDependencies();
  if (cleanupPending && millis() - lastCleanupMs >= 60000) cleanupLegacy();
  if (millis() - lastUpdateTimestamp > 200) {
    lastUpdateTimestamp = millis();
    auto currentState = getValueOpenStateFromDevice();
    if (currentState != lastOpenLevelState) {
      if (lastCmdTimestamp == 0 || millis() - lastCmdTimestamp > 30000) {
        if (currentState == 0) {
          SUPLA_LOG_DEBUG("Valve[%d]: manually closed", getChannelNumber());
          channel.setValveManuallyClosedFlag(true);
        }
        if (currentState != 0) {
          SUPLA_LOG_DEBUG("Valve[%d]: manually opened", getChannelNumber());
          channel.setValveManuallyClosedFlag(false);
        }
      }
    }
    lastOpenLevelState = currentState;
    channel.setValveOpenState(currentState);
  }

  if (millis() - lastSensorsCheckTimestamp > 1000) {
    lastSensorsCheckTimestamp = millis();

    bool floodDetected = isFloodDetected();

    if (channel.isValveOpen() || !channel.isValveFloodingFlagActive()) {
      if (floodDetected) {
        closeValve();
        channel.setValveFloodingFlag(true);
      }
    }
  }
}

bool ValveBase::isFloodDetected() {
  bool floodDetected = false;
  for (uint8_t i = 0; i < SUPLA_VALVE_SENSOR_MAX; ++i) {
    const auto state = consumeChannelState(sensorReference(i),
        0x30000 + (getChannelNumber() + 1) * 32 + i);
    bool value;
    if (!state.availableFor(ChannelCapability::FloodDetection) ||
        !state.binary(&value)) {
      previousSensorState[i] = false;
      continue;
    }
    if (value && (config.closeValveOnFloodType <= 1 ||
                  !previousSensorState[i])) floodDetected = true;
    previousSensorState[i] = value;
  }
  return floodDetected;
}

int32_t ValveBase::handleNewValueFromServer(
    TSD_SuplaChannelNewValue *newValue) {
  // TODO(klew): update below logic for percentage variant
  SUPLA_LOG_DEBUG("Valve[%d]: handleNewValueFromServer, value[0] %d",
                  getChannelNumber(),
                  newValue->value[0]);
  switch (newValue->value[0]) {
    case 0: {  // close
      closeValve();
      break;
    }
    case 1: {  // open
      openValve();
      break;
    }
    default: {
      SUPLA_LOG_WARNING("Valve[%d]: value[0] not supported",
                        getChannelNumber());
      return SUPLA_RESULT_FALSE;
    }
  }
  return SUPLA_RESULT_TRUE;
}

void ValveBase::onLoadConfig(SuplaDeviceClass *) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (!cfg) return;
  const bool wasUncertain = persistenceUncertain;
  const int32_t storedFunction = cfg->getChannelFunction(getChannelNumber());
  if ((storedFunction == 0 ||
      storedFunction == SUPLA_CHANNELFNC_VALVE_OPENCLOSE ||
      storedFunction == SUPLA_CHANNELFNC_VALVE_PERCENTAGE) &&
      channel.isFunctionValid(storedFunction)) setFunction(storedFunction);
  char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
  generateKey(key, "valve_cfg2");
  if (cfg->getBlobSize(key) >= 0) {
    ValveStoredConfigV2 record;
    if (cfg->getBlobSize(key) != sizeof(record) ||
        !cfg->getBlob(key, reinterpret_cast<char *>(&record), sizeof(record)) ||
        record.version != 2 || !record.config.valid() ||
        record.function != channel.getDefaultFunction() ||
        (record.function != 0 &&
         record.function != SUPLA_CHANNELFNC_VALVE_OPENCLOSE &&
         record.function != SUPLA_CHANNELFNC_VALVE_PERCENTAGE)) {
      config = {};
      config.closeValveOnFloodType = defaultCloseValveOnFloodType;
      configDurable = false;
      reconcileChannelDependencies();
      return;
    }
    config = record.config;
    persistenceUncertain = wasUncertain ||
        storedFunction != static_cast<int32_t>(record.function);
    configDurable = true;
    cleanupLegacy();
  } else {
    generateKey(key, Supla::ConfigTag::ValveCfgTag);
    LegacyValveConfig legacy;
    if (cfg->getBlobSize(key) == sizeof(legacy) &&
        cfg->getBlob(key, reinterpret_cast<char *>(&legacy), sizeof(legacy))) {
      ValveConfiguration candidate;
      candidate.closeValveOnFloodType = legacy.closeValveOnFloodType;
      memcpy(candidate.reserved, legacy.reserved, sizeof(candidate.reserved));
      for (uint8_t i = 0; i < SUPLA_VALVE_SENSOR_MAX; ++i) {
        if (legacy.sensorData[i] != 255)
          candidate.sensorData[i].assign(
              ChannelReference::local(legacy.sensorData[i]));
      }
      if (candidate.valid()) {
        config = candidate;
        configDurable = persistConfig(candidate, channel.getDefaultFunction());
        if (configDurable) cleanupLegacy();
      }
    }
  }
  loadConfigChangeFlag();
  if (defaultCloseValveOnFloodType && !config.closeValveOnFloodType) {
    config.closeValveOnFloodType = defaultCloseValveOnFloodType;
    triggerSetChannelConfig();
  }
  reconcileChannelDependencies();
}

void ValveBase::purgeConfig() {
  auto cfg = Supla::Storage::ConfigInstance();
  if (!cfg) return;
  char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
  generateKey(key, Supla::ConfigTag::ValveCfgTag);
  cfg->eraseKey(key);
  generateKey(key, "valve_cfg2");
  cfg->eraseKey(key);
}

void ValveBase::printConfig() const {
  SUPLA_LOG_DEBUG("Valve[%d]: close on flood type %d", getChannelNumber(),
                   config.closeValveOnFloodType);
}

void ValveBase::onLoadState() {
  uint8_t state = 0;
  Supla::Storage::ReadState(reinterpret_cast<unsigned char *>(&state),
                            sizeof(state));
  channel.setValveOpenState(state);
  lastOpenLevelState = state;

  uint8_t flags = 0;
  Supla::Storage::ReadState(reinterpret_cast<unsigned char *>(&flags),
                            sizeof(flags));
  if (flags & SUPLA_VALVE_FLAG_FLOODING) {
    channel.setValveFloodingFlag(true);
  }
  if (flags & SUPLA_VALVE_FLAG_MANUALLY_CLOSED) {
    channel.setValveManuallyClosedFlag(true);
  }

  SUPLA_LOG_DEBUG(
      "Valve[%d]: restored state %d (%s), flooding %d, manually closed %d",
      getChannelNumber(),
      state,
      (state == 0 ? "closed" : "open"),
      channel.isValveFloodingFlagActive(),
      channel.isValveManuallyClosedFlagActive());
}

void ValveBase::onSaveState() {
  uint8_t state = channel.getValveOpenState();
  Supla::Storage::WriteState(reinterpret_cast<unsigned char *>(&state),
                             sizeof(state));
  uint8_t flags = 0;
  if (channel.isValveFloodingFlagActive()) {
    flags |= SUPLA_VALVE_FLAG_FLOODING;
  }
  if (channel.isValveManuallyClosedFlagActive()) {
    flags |= SUPLA_VALVE_FLAG_MANUALLY_CLOSED;
  }
  Supla::Storage::WriteState(reinterpret_cast<unsigned char *>(&flags),
                             sizeof(flags));
}

Supla::ApplyConfigResult ValveBase::applyChannelConfig(
    TSD_ChannelConfig *result, bool local) {
  if (!result || result->ConfigType != SUPLA_CONFIG_TYPE_DEFAULT ||
      !channel.isFunctionValid(result->Func) ||
      (result->Func != 0 &&
       result->Func != SUPLA_CHANNELFNC_VALVE_OPENCLOSE &&
       result->Func != SUPLA_CHANNELFNC_VALVE_PERCENTAGE))
    return Supla::ApplyConfigResult::DataError;
  if (!result->ConfigSize || result->Func == 0) {
    if (result->Func == 0 && result->ConfigSize != 0)
      return Supla::ApplyConfigResult::DataError;
    auto candidate = config;
    if (static_cast<uint32_t>(result->Func) !=
            channel.getDefaultFunction() || result->Func == 0)
      candidate = {};
    if (defaultCloseValveOnFloodType)
      candidate.closeValveOnFloodType = defaultCloseValveOnFloodType;
    configDurable = persistConfig(candidate, result->Func);
    if (!configDurable) return Supla::ApplyConfigResult::DataError;
    config = candidate;
    channel.setDefaultFunction(result->Func);
    reconcileChannelDependencies();
    if (!result->Func) {
      markAllChannelConfigsReceived();
      return Supla::ApplyConfigResult::Success;
    }
    return Supla::ApplyConfigResult::SetChannelConfigNeeded;
  }
  if (result->ConfigType != SUPLA_CONFIG_TYPE_DEFAULT ||
      result->ConfigSize != sizeof(TChannelConfig_Valve) ||
      (result->Func != SUPLA_CHANNELFNC_VALVE_OPENCLOSE &&
       result->Func != SUPLA_CHANNELFNC_VALVE_PERCENTAGE))
    return Supla::ApplyConfigResult::DataError;
  const auto wire = reinterpret_cast<TChannelConfig_Valve *>(result->Config);
  ValveConfiguration candidate;
  candidate.closeValveOnFloodType = wire->CloseValveOnFloodType;
  const bool readonlyViolation = defaultCloseValveOnFloodType &&
                                 !candidate.closeValveOnFloodType;
  if (readonlyViolation)
    candidate.closeValveOnFloodType = config.closeValveOnFloodType
        ? config.closeValveOnFloodType : defaultCloseValveOnFloodType;
  memcpy(candidate.reserved, wire->Reserved, sizeof(candidate.reserved));
  const bool server = !local && usesServerReferences();
  for (uint8_t i = 0; i < SUPLA_VALVE_SENSOR_MAX; ++i) {
    const auto &field = wire->SensorInfo[i];
    if (server) {
      candidate.sensorData[i].assign(field.ChannelId ?
          ChannelReference::server(field.ChannelId) : ChannelReference{});
    } else {
      if (field.IsSet > 1 || (field.IsSet && field.ChannelNo == 255))
        return Supla::ApplyConfigResult::DataError;
      candidate.sensorData[i].assign(field.IsSet ?
          ChannelReference::local(field.ChannelNo) : ChannelReference{});
      const auto old = sensorReference(i);
      if (local && old.kind == ChannelReferenceKind::SERVER_CHANNEL &&
          localReferenceNumber(old) < 0) candidate.sensorData[i].assign(old);
    }
  }
  if (!candidate.valid()) return Supla::ApplyConfigResult::DataError;
  if (!persistConfig(candidate, result->Func)) {
    configDurable = false;
    return Supla::ApplyConfigResult::DataError;
  }
  for (uint8_t i = 0; i < SUPLA_VALVE_SENSOR_MAX; ++i) {
    if (sensorReference(i) != candidate.sensorData[i].reference())
      previousSensorState[i] = false;
  }
  config = candidate;
  configDurable = true;
  channel.setDefaultFunction(result->Func);
  cleanupLegacy();
  reconcileChannelDependencies();
  if (readonlyViolation) triggerSetChannelConfig();
  return readonlyViolation ? Supla::ApplyConfigResult::SetChannelConfigNeeded
                           : Supla::ApplyConfigResult::Success;
}

void ValveBase::fillChannelConfig(void *channelConfig, int *size,
                                  uint8_t configType) {
  if (!size) return;
  *size = 0;
  if (!channelConfig || configType != SUPLA_CONFIG_TYPE_DEFAULT) return;
  TChannelConfig_Valve wire = {};
  wire.CloseValveOnFloodType = config.closeValveOnFloodType;
  memcpy(wire.Reserved, config.reserved, sizeof(wire.Reserved));
  for (uint8_t i = 0; i < SUPLA_VALVE_SENSOR_MAX; ++i) {
    const auto ref = sensorReference(i);
    uint32_t id;
    if (!referenceWireId(ref, &id)) return;
    if (usesServerReferences()) {
      wire.SensorInfo[i].ChannelId = id;
    } else {
      if (ref.kind == ChannelReferenceKind::SERVER_CHANNEL) return;
      wire.SensorInfo[i].IsSet = ref.kind != ChannelReferenceKind::NONE;
      wire.SensorInfo[i].ChannelNo = id;
    }
  }
  memcpy(channelConfig, &wire, sizeof(wire));
  *size = sizeof(wire);
}

void ValveBase::closeValve() {
  setValve(0);
}

void ValveBase::openValve() {
  setValve(100);
}

void ValveBase::setValve(uint8_t openLevel) {
  SUPLA_LOG_INFO("Valve[%d]: setValve %d (%s)",
                 getChannelNumber(),
                 openLevel,
                 openLevel > 0 ? "open" : "close");

  if (openLevel > 0) {
    // when open, check sensors
    if (isFloodDetected()) {
      closeValve();
      channel.setValveFloodingFlag(true);
      return;
    } else {
      channel.setValveFloodingFlag(false);
    }
  }

  lastCmdTimestamp = millis();
  setValueOnDevice(openLevel);
}

void ValveBase::saveConfig(bool local) {
  configDurable = persistConfig(config, channel.getDefaultFunction());
  if (!configDurable) return;
  cleanupLegacy();
  if (local) triggerSetChannelConfig(SUPLA_CONFIG_TYPE_DEFAULT, true);
  else clearChannelConfigChangedFlag();
  saveConfigChangeFlag();
  for (auto proto = Supla::Protocol::ProtocolLayer::first(); proto;
       proto = proto->next()) proto->notifyConfigChange(getChannelNumber());
}

bool ValveBase::addSensor(uint8_t channelNumber) {
  if (channelNumber == 255) return false;
  auto ch = Channel::GetByChannelNumber(channelNumber);
  if (!ch || ch->getChannelType() != SUPLA_CHANNELTYPE_BINARYSENSOR)
    return false;
  for (const auto &sensor : config.sensorData) {
    if (localReferenceNumber(sensor.reference()) == channelNumber) return true;
  }
  for (uint8_t i = 0; i < SUPLA_VALVE_SENSOR_MAX; ++i) {
    if (sensorReference(i).kind == ChannelReferenceKind::NONE) {
      config.sensorData[i].assign(ChannelReference::local(channelNumber));
      previousSensorState[i] = false;
      reconcileChannelDependencies();
      saveConfig();
      return true;
    }
  }
  return false;
}

bool ValveBase::removeSensor(uint8_t channelNumber) {
  if (channelNumber == 255) return false;
  for (uint8_t i = 0; i < SUPLA_VALVE_SENSOR_MAX; ++i) {
    if (localReferenceNumber(sensorReference(i)) == channelNumber) {
      config.sensorData[i].assign(ChannelReference{});
      previousSensorState[i] = false;
      reconcileChannelDependencies();
      saveConfig();
      return true;
    }
  }
  return false;
}

uint8_t ValveBase::getValueOpenStateFromDevice() {
  SUPLA_LOG_ERROR("Valve[%d]: getValueOpenStateFromDevice not implemented",
                  getChannelNumber());
  return 0;
}

void ValveBase::setValueOnDevice(uint8_t openLevel) {
  SUPLA_LOG_WARNING("Valve::setValueOnDevice not implemented");
  SUPLA_LOG_DEBUG(
      "Valve[%d]: setValueOnDevice: %d", getChannelNumber(), openLevel);
}

void ValveBase::setIgnoreManuallyOpenedTimeMs(uint32_t timeMs) {
  ignoreManuallyOpenedTimeMs = timeMs;
}

void ValveBase::handleAction(int event, int action) {
  (void)(event);
  switch (action) {
    case OPEN: {
      openValve();
      break;
    }
    case CLOSE: {
      closeValve();
      break;
    }
    case TOGGLE: {
      if (channel.isValveOpen()) {
        closeValve();
      } else {
        openValve();
      }
      break;
    }
  }
}

void ValveBase::setDefaultCloseValveOnFloodType(uint8_t type) {
  if (type <= SUPLA_VALVE_CLOSE_ON_FLOOD_TYPE_ON_CHANGE) {
    defaultCloseValveOnFloodType = type;
    if (config.closeValveOnFloodType == 0) {
      config.closeValveOnFloodType = type;
    }
  }
}

// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "container.h"

#include <supla-common/proto.h>
#include <string.h>
#include <supla/channels/channel_state.h>
#include <supla/storage/canonical_config.h>
#include <supla/actions.h>
#include <supla/events.h>
#include <supla/log_wrapper.h>
#include <supla/protocol/protocol_layer.h>
#include <supla/storage/config.h>
#include <supla/storage/config_tags.h>
#include <supla/storage/storage.h>
#include <supla/time.h>

using Supla::Sensor::Container;

Container::Container() {
  channel.setType(SUPLA_CHANNELTYPE_CONTAINER);
  setFunction(SUPLA_CHANNELFNC_CONTAINER);
  channel.setContainerFillValue(-1);
  channel.setFlag(SUPLA_CHANNEL_FLAG_RUNTIME_CHANNEL_CONFIG_UPDATE);
  usedConfigTypes.set(SUPLA_CONFIG_TYPE_DEFAULT);
}

bool Supla::Sensor::ContainerConfiguration::valid() const {
  if (warningAboveLevel > 101 || alarmAboveLevel > 101 ||
      warningBelowLevel > 101 || alarmBelowLevel > 101 ||
      muteAlarmSoundWithoutAdditionalAuth > 1) return false;
  for (const auto &sensor : sensorData) {
    if (!sensor.source.reference().valid() || sensor.fillLevel > 100)
      return false;
  }
  return true;
}

Container::~Container() {
  for (uint8_t i = 0; i < 10; ++i)
    releaseChannelConsumer(0x20000 + (getChannelNumber() + 1) * 16 + i);
}

Supla::ChannelReference Container::sensorReference(uint8_t slot) const {
  return slot < 10 ? config.sensorData[slot].source.reference()
                   : ChannelReference{};
}

int Container::sensorChannelNumber(uint8_t slot) const {
  return localReferenceNumber(sensorReference(slot));
}

bool Container::setSensorSlot(uint8_t slot, int number, uint8_t level) {
  if (slot >= 10 || number < -1 || number > 255) return false;
  auto &sensor = config.sensorData[slot];
  if (sensor.source.reference().kind == ChannelReferenceKind::SERVER_CHANNEL &&
      sensorChannelNumber(slot) < 0) return false;
  sensor.source.assign(number < 0 || number == 255 ? ChannelReference{}
                               : ChannelReference::local(number));
  sensor.fillLevel = sensor.source.kind ? (level > 100 ? 100 : level) : 0;
  reconcileChannelDependencies();
  return true;
}

void Container::reconcileChannelDependencies() {
  for (uint8_t i = 0; i < 10; ++i)
    consumeChannelState(sensorReference(i),
                        0x20000 + (getChannelNumber() + 1) * 16 + i);
}

void Container::cleanupLegacy() {
  lastCleanupMs = millis();
  char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
  generateKey(key, Supla::ConfigTag::ContainerTag);
  cleanupPending =
      !Supla::StorageDetail::cleanupLegacyConfig(key, cleanupPending);
}

bool Container::persistConfig(const ContainerConfiguration &candidate,
                              uint32_t function) {
  if (!candidate.valid()) return false;
  ContainerStoredConfigV2 record;
  record.function = function;
  record.config = candidate;
  char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
  generateKey(key, "cnt_cfg2");
  return Supla::StorageDetail::persistCanonicalConfig(
      key, record, &persistenceUncertain, getChannelNumber());
}

void Container::setInternalLevelReporting(bool internalLevelReporting) {
  if (internalLevelReporting) {
    getChannel()->setFlag(
        SUPLA_CHANNEL_FLAG_FILL_LEVEL_REPORTING_IN_FULL_RANGE);
  } else {
    getChannel()->unsetFlag(
        SUPLA_CHANNEL_FLAG_FILL_LEVEL_REPORTING_IN_FULL_RANGE);
  }
}

bool Container::isInternalLevelReporting() const {
  return getChannel()->getFlags() &
         SUPLA_CHANNEL_FLAG_FILL_LEVEL_REPORTING_IN_FULL_RANGE;
}

void Container::iterateAlways() {
  reconcileChannelDependencies();
  if (cleanupPending && millis() - lastCleanupMs >= 60000) cleanupLegacy();
  if (lastReadTime == 0 || millis() - lastReadTime >= readIntervalMs) {
    lastReadTime = millis();
    int sensorValue = -1;
    bool invalidSensorState = false;
    if (isSensorDataUsed()) {
      sensorValue = getHighestSensorValueAndUpdateState();
      invalidSensorState = checkSensorInvalidState(sensorValue);
    }
    int value = 0;
    if (isInternalLevelReporting()) {
      value = readNewValue();
      if (value < 0 || value > 100) {
        invalidSensorState = true;
      }
      if (sensorValue > value) {
        value = sensorValue;
      } else if (sensorValue >= 0 && !invalidSensorState) {
        invalidSensorState = checkSensorInvalidState(value, 2);
      }
    } else {
      value = sensorValue;
    }

    channel.setContainerInvalidSensorState(invalidSensorState);
    if (isAlarmingUsed()) {
      bool warningActive = false;
      bool alarmActive = false;
      if (value >= 0 && value <= 100) {
        if (isWarningBelowLevelSet()) {
          warningActive = value <= getWarningBelowLevel();
        }
        if (!warningActive && isWarningAboveLevelSet()) {
          warningActive = value >= getWarningAboveLevel();
        }
        if (isAlarmBelowLevelSet()) {
          alarmActive = value <= getAlarmBelowLevel();
        }
        if (!alarmActive && isAlarmAboveLevelSet()) {
          alarmActive = value >= getAlarmAboveLevel();
        }
      }
      channel.setContainerWarning(warningActive);
      channel.setContainerAlarm(alarmActive);
    }

    channel.setContainerFillValue(value);
    if (isSoundAlarmSupported()) {
      if (isAlarmActive()) {
        setSoundAlarmOn(2);
      } else if (isWarningActive()) {
        setSoundAlarmOn(1);
      } else {
        setSoundAlarmOn(0);
      }
    } else {
      setSoundAlarmOn(0);
    }
  }
}

void Container::setValue(int value) {
  if (value < 0 || value > 100) {
    fillLevel = -1;
    return;
  }

  fillLevel = value;
}

int Container::readNewValue() {
  return fillLevel;
}

void Container::setReadIntervalMs(uint32_t timeMs) {
  if (timeMs < 100) {
    timeMs = 100;
  }
  readIntervalMs = timeMs;
}

void Container::setAlarmActive(bool alarmActive) {
  channel.setContainerAlarm(alarmActive);
}

bool Container::isAlarmActive() const {
  return channel.isContainerAlarmActive();
}

void Container::setWarningActive(bool warningActive) {
  channel.setContainerWarning(warningActive);
}

bool Container::isWarningActive() const {
  return channel.isContainerWarningActive();
}

void Container::setInvalidSensorStateActive(bool invalidSensorStateActive) {
  channel.setContainerInvalidSensorState(invalidSensorStateActive);
}

bool Container::isInvalidSensorStateActive() const {
  return channel.isContainerInvalidSensorStateActive();
}

void Container::setSoundAlarmOn(uint8_t level) {
  if (soundAlarmActivatedLevel != level) {
    if (soundAlarmActivatedLevel < level || level == 0) {
      channel.setContainerSoundAlarmOn(level > 0 || isExternalSoundAlarmOn());
    }
    soundAlarmActivatedLevel = level;
  }
  if (isExternalSoundAlarmOn() || level == 0) {
    channel.setContainerSoundAlarmOn(isExternalSoundAlarmOn());
  }
}

bool Container::isSoundAlarmOn() const {
  return channel.isContainerSoundAlarmOn();
}

void Container::updateConfigField(uint8_t *configField, int8_t value) {
  if (configField) {
    if (value > 100) {
      value = 100;
    }
    if (value < -1) {
      value = -1;
    }
    value = value + 1;
    bool alarmingWasUsed = isAlarmingUsed();
    *configField = value;
    if (alarmingWasUsed && !isAlarmingUsed()) {
      setAlarmActive(false);
      setWarningActive(false);
      setSoundAlarmOn(0);
    }
  }
}

void Container::muteSoundAlarm() {
  if (channel.isContainerSoundAlarmOn()) {
    channel.setContainerSoundAlarmOn(false);
    runAction(Supla::ON_CONTAINER_SOUND_ALARM_MUTED);
    setExternalSoundAlarmOff();
  }
}

void Container::setWarningAboveLevel(int8_t warningAboveLevel) {
  updateConfigField(&config.warningAboveLevel, warningAboveLevel);
}

int8_t Container::getWarningAboveLevel() const {
  return config.warningAboveLevel - 1;
}

void Container::setAlarmAboveLevel(int8_t alarmAboveLevel) {
  updateConfigField(&config.alarmAboveLevel, alarmAboveLevel);
}

int8_t Container::getAlarmAboveLevel() const {
  return config.alarmAboveLevel - 1;
}

void Container::setWarningBelowLevel(int8_t warningBelowLevel) {
  updateConfigField(&config.warningBelowLevel, warningBelowLevel);
}

int8_t Container::getWarningBelowLevel() const {
  return config.warningBelowLevel - 1;
}

void Container::setAlarmBelowLevel(int8_t alarmBelowLevel) {
  updateConfigField(&config.alarmBelowLevel, alarmBelowLevel);
}

int8_t Container::getAlarmBelowLevel() const {
  return config.alarmBelowLevel - 1;
}

void Container::setMuteAlarmSoundWithoutAdditionalAuth(
    bool muteAlarmSoundWithoutAdditionalAuth) {
  config.muteAlarmSoundWithoutAdditionalAuth =
      muteAlarmSoundWithoutAdditionalAuth;
}

bool Container::isMuteAlarmSoundWithoutAdditionalAuth() const {
  return config.muteAlarmSoundWithoutAdditionalAuth;
}

bool Container::setSensorData(uint8_t channelNumber, uint8_t fillLevel) {
  if (channelNumber == 255) return false;
  if (!fillLevel) fillLevel = 1;
  for (uint8_t i = 0; i < 10; ++i) {
    if (sensorChannelNumber(i) == channelNumber)
      return setSensorSlot(i, channelNumber, fillLevel);
  }
  for (uint8_t i = 0; i < 10; ++i) {
    if (sensorReference(i).kind == ChannelReferenceKind::NONE)
      return setSensorSlot(i, channelNumber, fillLevel);
  }
  return false;
}

void Container::removeSensorData(uint8_t channelNumber) {
  for (uint8_t i = 0; i < 10; ++i) {
    if (sensorChannelNumber(i) == channelNumber) {
      setSensorSlot(i, -1, 0);
      return;
    }
  }
}

int Container::getFillLevelForSensor(uint8_t channelNumber) const {
  for (uint8_t i = 0; i < 10; ++i) {
    if (sensorChannelNumber(i) == channelNumber)
      return config.sensorData[i].fillLevel;
  }
  return -1;
}

bool Container::isSensorDataUsed() const {
  for (const auto &sensor : config.sensorData)
    if (sensor.source.kind) return true;
  return false;
}

int8_t Container::getHighestSensorValueAndUpdateState() {
  // browse all sensor data and get highest value of sensor which is in
  // active state
  int8_t highestValue = -1;
  bool offline = false;
  for (uint8_t i = 0; i < 10; ++i) {
    const auto &sensor = config.sensorData[i];
    auto sensorState = this->sensorState(i);
    if (sensorState == SensorState::Unknown) {
      continue;
    }
    if (sensorState == SensorState::Offline) {
      offline = true;
      continue;
    }
    if (highestValue == -1) {
      highestValue = 0;
    }
    if (sensorState == SensorState::Active) {
      if (sensor.fillLevel > highestValue) {
        highestValue = sensor.fillLevel;
      }
    }
  }

  if (offline && !sensorOfflineReported) {
    runAction(Supla::ON_CONTAINER_SENSOR_OFFLINE_ACTIVE);
    sensorOfflineReported = true;
  } else if (!offline && sensorOfflineReported) {
    runAction(Supla::ON_CONTAINER_SENSOR_OFFLINE_INACTIVE);
    sensorOfflineReported = false;
  }

  return highestValue;
}

bool Container::checkSensorInvalidState(const int8_t currentfillLevel,
                                        const int8_t tolerance) const {
  for (uint8_t i = 0; i < 10; ++i) {
    const auto &sensor = config.sensorData[i];
    if (sensorState(i) == SensorState::Inactive) {
      if (sensor.fillLevel <= currentfillLevel - tolerance) {
        return true;
      }
    }
  }
  return false;
}

enum Supla::Sensor::SensorState Container::sensorState(uint8_t slot) const {
  const auto reference = sensorReference(slot);
  if (reference.kind == ChannelReferenceKind::NONE) return SensorState::Unknown;
  const auto state = consumeChannelState(reference,
      0x20000 + (getChannelNumber() + 1) * 16 + slot);
  bool value;
  if (!state.availableFor(ChannelCapability::ContainerLevel) ||
      !state.binary(&value)) return SensorState::Offline;
  return value ? SensorState::Active : SensorState::Inactive;
}

enum Supla::Sensor::SensorState Container::getSensorState(
    const uint8_t channelNumber) const {
  if (channelNumber == 255) return SensorState::Unknown;
  const ChannelState state(Channel::GetByChannelNumber(channelNumber));
  bool value;
  if (!state.availableFor(ChannelCapability::ContainerLevel) ||
      !state.binary(&value)) return SensorState::Offline;
  return value ? SensorState::Active : SensorState::Inactive;
}

bool Container::isAlarmingUsed() const {
  return isWarningAboveLevelSet() || isAlarmAboveLevelSet() ||
         isWarningBelowLevelSet() || isAlarmBelowLevelSet();
}

bool Container::isWarningAboveLevelSet() const {
  return config.warningAboveLevel > 0;
}

bool Container::isAlarmAboveLevelSet() const {
  return config.alarmAboveLevel > 0;
}

bool Container::isWarningBelowLevelSet() const {
  return config.warningBelowLevel > 0;
}

bool Container::isAlarmBelowLevelSet() const {
  return config.alarmBelowLevel > 0;
}

void Container::onLoadConfig(SuplaDeviceClass *) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (!cfg) return;
  const bool wasUncertain = persistenceUncertain;
  const int32_t storedFunction = cfg->getChannelFunction(getChannelNumber());
  if ((storedFunction == 0 ||
      storedFunction == SUPLA_CHANNELFNC_CONTAINER ||
      storedFunction == SUPLA_CHANNELFNC_SEPTIC_TANK ||
      storedFunction == SUPLA_CHANNELFNC_WATER_TANK) &&
      channel.isFunctionValid(storedFunction)) setFunction(storedFunction);
  char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
  generateKey(key, "cnt_cfg2");
  if (cfg->getBlobSize(key) >= 0) {
    ContainerStoredConfigV2 record;
    if (cfg->getBlobSize(key) != sizeof(record) ||
        !cfg->getBlob(key, reinterpret_cast<char *>(&record), sizeof(record)) ||
        record.version != 2 || !record.config.valid() ||
        record.function != channel.getDefaultFunction() ||
        (record.function != 0 &&
         record.function != SUPLA_CHANNELFNC_CONTAINER &&
         record.function != SUPLA_CHANNELFNC_SEPTIC_TANK &&
         record.function != SUPLA_CHANNELFNC_WATER_TANK)) {
      config = {};
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
    generateKey(key, Supla::ConfigTag::ContainerTag);
    LegacyContainerConfig legacy;
    if (cfg->getBlobSize(key) == sizeof(legacy) &&
        cfg->getBlob(key, reinterpret_cast<char *>(&legacy), sizeof(legacy))) {
      ContainerConfiguration candidate;
      memcpy(static_cast<void *>(&candidate), &legacy, 5);
      for (uint8_t i = 0; i < 10; ++i) {
        if (legacy.sensorData[i].channelNumber != 255) {
          candidate.sensorData[i].source.assign(
              ChannelReference::local(legacy.sensorData[i].channelNumber));
          candidate.sensorData[i].fillLevel = legacy.sensorData[i].fillLevel;
        }
      }
      if (candidate.valid()) {
        config = candidate;
        configDurable = persistConfig(candidate, channel.getDefaultFunction());
        if (configDurable) cleanupLegacy();
      }
    }
  }
  loadConfigChangeFlag();
  reconcileChannelDependencies();
}

Supla::ApplyConfigResult Container::applyChannelConfig(
    TSD_ChannelConfig *result, bool local) {
  if (!result || result->ConfigType != SUPLA_CONFIG_TYPE_DEFAULT ||
      !channel.isFunctionValid(result->Func) ||
      (result->Func != 0 &&
       result->Func != SUPLA_CHANNELFNC_CONTAINER &&
       result->Func != SUPLA_CHANNELFNC_SEPTIC_TANK &&
       result->Func != SUPLA_CHANNELFNC_WATER_TANK))
    return Supla::ApplyConfigResult::DataError;
  if (!result->ConfigSize || result->Func == 0) {
    if (result->Func == 0 && result->ConfigSize != 0)
      return Supla::ApplyConfigResult::DataError;
    auto candidate = config;
    if (static_cast<uint32_t>(result->Func) !=
            channel.getDefaultFunction() || result->Func == 0)
      candidate = {};
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
      result->ConfigSize != sizeof(TChannelConfig_Container) ||
      (result->Func != SUPLA_CHANNELFNC_CONTAINER &&
       result->Func != SUPLA_CHANNELFNC_SEPTIC_TANK &&
       result->Func != SUPLA_CHANNELFNC_WATER_TANK))
    return Supla::ApplyConfigResult::DataError;
  const auto wire =
      reinterpret_cast<TChannelConfig_Container *>(result->Config);
  ContainerConfiguration candidate;
  memcpy(static_cast<void *>(&candidate), wire, 5);
  memcpy(candidate.reserved, wire->Reserved, sizeof(candidate.reserved));
  const bool server = !local && usesServerReferences();
  for (uint8_t i = 0; i < 10; ++i) {
    const auto &field = wire->SensorInfo[i];
    auto &sensor = candidate.sensorData[i];
    if (server) {
      sensor.source.assign(field.ChannelId ?
          ChannelReference::server(field.ChannelId) : ChannelReference{});
    } else {
      if (field.IsSet > 1 || (field.IsSet && field.ChannelNo == 255))
        return Supla::ApplyConfigResult::DataError;
      sensor.source.assign(field.IsSet
                               ? ChannelReference::local(field.ChannelNo)
                               : ChannelReference{});
      const auto old = sensorReference(i);
      if (local && old.kind == ChannelReferenceKind::SERVER_CHANNEL &&
          localReferenceNumber(old) < 0) sensor.source.assign(old);
    }
    sensor.fillLevel = sensor.source.kind ? field.FillLevel : 0;
  }
  if (!candidate.valid()) return Supla::ApplyConfigResult::DataError;
  if (!persistConfig(candidate, result->Func)) {
    configDurable = false;
    return Supla::ApplyConfigResult::DataError;
  }
  configDurable = true;
  config = candidate;
  channel.setDefaultFunction(result->Func);
  cleanupLegacy();
  reconcileChannelDependencies();
  return Supla::ApplyConfigResult::Success;
}

void Container::purgeConfig() {
  auto cfg = Storage::ConfigInstance();
  if (!cfg) return;
  char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
  generateKey(key, "container");
  cfg->eraseKey(key);
  generateKey(key, "cnt_cfg2");
  cfg->eraseKey(key);
}

void Container::printConfig() const {
  SUPLA_LOG_DEBUG(
      "Container[%d]: warning above: %d, alarm above: %d, warning below: "
      "%d, alarm below: %d, mute by any user: %d",
      getChannelNumber(), config.warningAboveLevel - 1,
      config.alarmAboveLevel - 1, config.warningBelowLevel - 1,
      config.alarmBelowLevel - 1, config.muteAlarmSoundWithoutAdditionalAuth);
}

void Container::fillChannelConfig(void *channelConfig, int *size,
                                  uint8_t configType) {
  if (!size) return;
  *size = 0;
  if (!channelConfig || configType != SUPLA_CONFIG_TYPE_DEFAULT) return;
  TChannelConfig_Container wire = {};
  memcpy(&wire, &config, 5);
  memcpy(wire.Reserved, config.reserved, sizeof(wire.Reserved));
  for (uint8_t i = 0; i < 10; ++i) {
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
    wire.SensorInfo[i].FillLevel = config.sensorData[i].fillLevel;
  }
  memcpy(channelConfig, &wire, sizeof(wire));
  *size = sizeof(wire);
}

void Container::saveConfig() {
  configDurable = persistConfig(config, channel.getDefaultFunction());
  if (!configDurable) return;
  cleanupLegacy();
  saveConfigChangeFlag();
  for (auto proto = Supla::Protocol::ProtocolLayer::first(); proto;
       proto = proto->next()) proto->notifyConfigChange(getChannelNumber());
}

void Container::setSoundAlarmSupported(bool soundAlarmSupported) {
  this->soundAlarmSupported = soundAlarmSupported;
}

bool Container::isSoundAlarmSupported() const {
  return soundAlarmSupported;
}

int Container::handleCalcfgFromServer(TSD_DeviceCalCfgRequest *request) {
  if (request) {
    if (request->Command == SUPLA_CALCFG_CMD_MUTE_ALARM_SOUND) {
      if (!config.muteAlarmSoundWithoutAdditionalAuth &&
          !request->SuperUserAuthorized) {
        return SUPLA_CALCFG_RESULT_UNAUTHORIZED;
      }
      muteSoundAlarm();
      return SUPLA_CALCFG_RESULT_DONE;
    }
  }
  return SUPLA_CALCFG_RESULT_NOT_SUPPORTED;
}

void Container::handleAction(int, int action) {
  switch (action) {
    case Supla::MUTE_SOUND_ALARM: {
      muteSoundAlarm();
      break;
    }
    case Supla::ENABLE_EXTERNAL_SOUND_ALARM: {
      setExternalSoundAlarmOn();
      break;
    }
    case Supla::DISABLE_EXTERNAL_SOUND_ALARM: {
      setExternalSoundAlarmOff();
      break;
    }
  }
}

bool Container::isExternalSoundAlarmOn() const {
  return externalSoundAlarm;
}

void Container::setExternalSoundAlarmOn() {
  externalSoundAlarm = true;
}

void Container::setExternalSoundAlarmOff() {
  externalSoundAlarm = false;
}

/*
   Copyright (C) AC SOFTWARE SP. Z O.O

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include "valve_base.h"

#include <supla/network/network.h>
#include <supla/storage/config_tags.h>
#include <supla/log_wrapper.h>
#include <supla/time.h>
#include <supla/actions.h>

#include <string.h>

using Supla::Control::ValveBase;

Supla::Control::ValveConfig::ValveConfig() {
  memset(sensorData, 255, sizeof(sensorData));
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
}

void ValveBase::onInit() {
  lastSensorsCheckTimestamp = millis();
}

void ValveBase::iterateAlways() {
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

    if (channel.isValveOpen()) {
      if (floodDetected) {
        closeValve();
        channel.setValveFloodingFlag(true);
      }
    }
  }
}

bool ValveBase::isFloodDetected() const {
  for (auto const &sensor : config.sensorData) {
    if (sensor == 255) {
      continue;
    }

    auto ch = Supla::Channel::GetByChannelNumber(sensor);
    if (ch == nullptr) {
      SUPLA_LOG_WARNING(
          "Valve[%d] channel %d not found", getChannelNumber(), sensor);
      continue;
    }

    if (ch->getChannelType() != SUPLA_CHANNELTYPE_BINARYSENSOR) {
      SUPLA_LOG_WARNING("Valve[%d] channel %d is not a binary sensor",
                        getChannelNumber(),
                        sensor);
      continue;
    }

    if (ch->getValueBool() == true) {
      return true;
    }
  }
  return false;
}

int32_t ValveBase::handleNewValueFromServer(
    TSD_SuplaChannelNewValue *newValue) {
  // TODO(klew): update below logic for percentage variant
  SUPLA_LOG_DEBUG("Valve[%d]: handleNewValueFromServer, value[0] %d",
                  getChannelNumber(), newValue->value[0]);
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
  if (cfg) {
    loadFunctionFromConfig();
    // load TChannelConfig_Valve from Config
    char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    generateKey(key, Supla::ConfigTag::ValveCfgTag);
    cfg->getBlob(
        key, reinterpret_cast<char *>(&config), sizeof(TChannelConfig_Valve));

    printConfig();
    loadConfigChangeFlag();
  }
}

void ValveBase::purgeConfig() {
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    generateKey(key, Supla::ConfigTag::ValveCfgTag);
    cfg->eraseKey(key);
  }
}


void ValveBase::printConfig() const {
  for (auto const &sensor : config.sensorData) {
    if (sensor == 255) {
      continue;
    }
    SUPLA_LOG_DEBUG("Valve[%d]: sensor channel %d", getChannelNumber(), sensor);
  }
}

void ValveBase::onLoadState() {
  uint8_t state = 0;
  Supla::Storage::ReadState(
      reinterpret_cast<unsigned char *>(&state),
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

uint8_t ValveBase::applyChannelConfig(TSD_ChannelConfig *result, bool local) {
  (void)(local);
  SUPLA_LOG_DEBUG(
      "Valve[%d]:applyChannelConfig, func %d, configtype %d, configsize %d",
      getChannelNumber(),
      result->Func,
      result->ConfigType,
      result->ConfigSize);

  if (result->ConfigSize == 0) {
    return SUPLA_CONFIG_RESULT_TRUE;
  }

  setAndSaveFunction(result->Func);

  switch (result->Func) {
    case SUPLA_CHANNELFNC_VALVE_OPENCLOSE:
    case SUPLA_CHANNELFNC_VALVE_PERCENTAGE: {
      if (result->ConfigType == 0 &&
          result->ConfigSize == sizeof(TChannelConfig_Valve)) {
        auto cfg = reinterpret_cast<TChannelConfig_Valve *>(result->Config);
        for (unsigned int i = 0;
            i < sizeof(cfg->SensorInfo) / sizeof(cfg->SensorInfo[0]);
            i++) {
          if (cfg->SensorInfo[i].IsSet == 0) {
            config.sensorData[i] = 255;
          } else {
            config.sensorData[i] = cfg->SensorInfo[i].ChannelNo;
          }
        }
      }
      printConfig();
      saveConfig(false);
      break;
    }
    default: {
      SUPLA_LOG_WARNING("Valve[%d]: unsupported func %d",
                        getChannelNumber(),
                        result->Func);
      break;
    }
  }
  return SUPLA_CONFIG_RESULT_TRUE;
}

void ValveBase::fillChannelConfig(void *channelConfig, int *size) {
  if (size) {
    *size = 0;
  } else {
    return;
  }
  if (channelConfig == nullptr) {
    return;
  }

  switch (channel.getDefaultFunction()) {
    case SUPLA_CHANNELFNC_VALVE_OPENCLOSE:
    case SUPLA_CHANNELFNC_VALVE_PERCENTAGE: {
      auto cfg = reinterpret_cast<TChannelConfig_Valve *>(
          channelConfig);
      *size = sizeof(TChannelConfig_Valve);
      int i = 0;
      for (const auto &sensor : config.sensorData) {
        if (sensor == 255) {
          continue;
        }
        cfg->SensorInfo[i].IsSet = 1;
        cfg->SensorInfo[i].ChannelNo = sensor;
        i++;
      }

      break;
    }
    default:
      SUPLA_LOG_WARNING(
          "Valve[%d]: fill channel config for unknown function %d",
          channel.getChannelNumber(),
          channel.getDefaultFunction());
      return;
  }
}

void ValveBase::closeValve() {
  setValve(0);
}

void ValveBase::openValve() {
  setValve(100);
}

void ValveBase::setValve(uint8_t openLevel) {
  SUPLA_LOG_INFO("Valve[%d]: setValve %d (%s)", getChannelNumber(), openLevel,
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
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    generateKey(key, Supla::ConfigTag::ValveCfgTag);
    if (cfg->setBlob(key,
                     reinterpret_cast<const char *>(&config),
                     sizeof(ValveConfig))) {
      SUPLA_LOG_INFO("Valve[%d]: config saved successfully",
                     getChannelNumber());
    } else {
      SUPLA_LOG_WARNING("Valve[%d]: failed to save config", getChannelNumber());
    }

    if (local) {
      triggerSetChannelConfig();
    } else {
      clearChannelConfigChangedFlag();
    }
    saveConfigChangeFlag();
    cfg->saveWithDelay(5000);
  }
  for (auto proto = Supla::Protocol::ProtocolLayer::first();
      proto != nullptr; proto = proto->next()) {
    proto->notifyConfigChange(getChannelNumber());
  }
}

bool ValveBase::addSensor(uint8_t channelNumber) {
  if (channelNumber == 255) {
    SUPLA_LOG_WARNING(
        "Valve[%d] channel number %d is invalid",
        getChannelNumber(),
        channelNumber);
    return false;
  }

  auto ch = Supla::Channel::GetByChannelNumber(channelNumber);
  if (ch == nullptr) {
    SUPLA_LOG_WARNING(
        "Valve[%d] channel %d not found", getChannelNumber(), channelNumber);
    return false;
  }
  if (ch->getChannelType() != SUPLA_CHANNELTYPE_BINARYSENSOR) {
    SUPLA_LOG_WARNING(
        "Valve[%d] channel %d is not a binary sensor",
        getChannelNumber(),
        channelNumber);
    return false;
  }

  for (auto &sensor : config.sensorData) {
    if (sensor == channelNumber) {
      SUPLA_LOG_DEBUG("Valve[%d] channel %d already on a list",
                      getChannelNumber(),
                      channelNumber);
      return true;
    }
    if (sensor == 255) {
      sensor = channelNumber;
      saveConfig();
      return true;
    }
  }
  SUPLA_LOG_WARNING(
      "Valve[%d] sensor list is full. Channel %d not added",
      getChannelNumber(),
      channelNumber);
  return false;
}

bool ValveBase::removeSensor(uint8_t channelNumber) {
  if (channelNumber == 255) {
    SUPLA_LOG_WARNING(
        "Valve[%d] channel number %d is invalid",
        getChannelNumber(),
        channelNumber);
    return false;
  }

  for (auto &sensor : config.sensorData) {
    if (sensor == channelNumber) {
      sensor = 255;
      SUPLA_LOG_DEBUG("Valve[%d] channel %d removed from list",
                      getChannelNumber(),
                      channelNumber);
      saveConfig();
      return true;
    }
  }
  return false;
}

uint8_t ValveBase::getValueOpenStateFromDevice() {
  SUPLA_LOG_ERROR(
      "Valve[%d]: getValueOpenStateFromDevice not implemented",
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


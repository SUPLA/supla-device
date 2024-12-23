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

#include "container.h"

#include <supla-common/proto.h>
#include <supla/time.h>
#include <supla/log_wrapper.h>
#include <supla/storage/config.h>
#include <supla/storage/config_tags.h>

using Supla::Sensor::Container;

Container::Container() {
  channel.setType(SUPLA_CHANNELTYPE_CONTAINER);
  setDefaultFunction(SUPLA_CHANNELFNC_CONTAINER);
  channel.setContainerFillValue(-1);
  channel.setFlag(SUPLA_CHANNEL_FLAG_RUNTIME_CHANNEL_CONFIG_UPDATE);
}

void Container::iterateAlways() {
  if (lastReadTime == 0 || millis() - lastReadTime >= readIntervalMs) {
    lastReadTime = millis();
    int value = 0;
    if (isSensorDataUsed()) {
      value = getHighestSensorValue();
      channel.setContainerInvalidSensorState(checkSensorInvalidState(value));
    } else {
      value = readNewValue();
    }
    if (isAlarmingUsed()) {
      bool warningActive = false;
      bool alarmActive = false;
      if (value >= 0 && value <= 100) {
        if (isWarningBelowLevelSet()) {
          warningActive = value <= config.warningBelowLevel;
        }
        if (!warningActive && isWarningAboveLevelSet()) {
          warningActive = value >= config.warningAboveLevel;
        }
        if (isAlarmBelowLevelSet()) {
          alarmActive = value <= config.alarmBelowLevel;
        }
        if (!alarmActive && isAlarmAboveLevelSet()) {
          alarmActive = value >= config.alarmAboveLevel;
        }
      }
      channel.setContainerWarning(warningActive);
      channel.setContainerAlarm(alarmActive);
    }

    channel.setContainerFillValue(value);
    if (isSoundAlarmSupported()) {
      if (isAlarmActive() || isWarningActive()) {
        setSoundAlarmOn(true);
      } else {
        setSoundAlarmOn(false);
      }
    } else {
      setSoundAlarmOn(false);
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

void Container::setSoundAlarmOn(bool soundAlarmOn) {
  channel.setContainerSoundAlarmOn(soundAlarmOn);
}

bool Container::isSoundAlarmOn() const {
  return channel.isContainerSoundAlarmOn();
}

void Container::updateConfigField(uint8_t *configField, uint8_t value) {
  if (configField) {
    if (value > 101) {
      value = 0;
    }
    bool alarmingWasUsed = isAlarmingUsed();
    *configField = value;
    if (alarmingWasUsed && !isAlarmingUsed()) {
      setAlarmActive(false);
      setWarningActive(false);
      setSoundAlarmOn(false);
    }
  }
}

void Container::setWarningAboveLevel(uint8_t warningAboveLevel) {
  updateConfigField(&config.warningAboveLevel, warningAboveLevel);
}

uint8_t Container::getWarningAboveLevel() const {
  return config.warningAboveLevel - 1;
}

void Container::setAlarmAboveLevel(uint8_t alarmAboveLevel) {
  updateConfigField(&config.alarmAboveLevel, alarmAboveLevel);
}

uint8_t Container::getAlarmAboveLevel() const {
  return config.alarmAboveLevel - 1;
}

void Container::setWarningBelowLevel(uint8_t warningBelowLevel) {
  updateConfigField(&config.warningBelowLevel, warningBelowLevel);
}

uint8_t Container::getWarningBelowLevel() const {
  return config.warningBelowLevel - 1;
}

void Container::setAlarmBelowLevel(uint8_t alarmBelowLevel) {
  updateConfigField(&config.alarmBelowLevel, alarmBelowLevel);
}

uint8_t Container::getAlarmBelowLevel() const {
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
  if (fillLevel > 101) {
    fillLevel = 0;
  }
  if (channelNumber == 255) {
    SUPLA_LOG_WARNING(
        "Container[%d]::setSensorData: invalid channelNumber == 255",
        channelNumber);
    return false;
  }

  // check if sensor is on the list and set fill level
  for (auto &sensor : config.sensorData) {
    if (sensor.channelNumber == channelNumber) {
      sensor.fillLevel = fillLevel;
      return true;
    }
  }

  // sensor is not on a list so add it
  for (auto &sensor : config.sensorData) {
    if (sensor.channelNumber == 255) {
      sensor.channelNumber = channelNumber;
      sensor.fillLevel = fillLevel;
      return true;
    }
  }

  return true;
}

void Container::removeSensorData(uint8_t channelNumber) {
  for (auto &sensor : config.sensorData) {
    if (sensor.channelNumber == channelNumber) {
      sensor.channelNumber = 255;
      sensor.fillLevel = 0;
      break;
    }
  }
}

bool Container::isSensorDataUsed() const {
  for (auto const &sensor : config.sensorData) {
    if (sensor.channelNumber != 255) {
      return true;
    }
  }
  return false;
}

int8_t Container::getHighestSensorValue() const {
  // browse all sensor data and get highest value of sensor which is in
  // active state
  int8_t highestValue = -1;
  for (auto const &sensor : config.sensorData) {
    if (getSensorState(sensor.channelNumber) == 1) {
      if (sensor.fillLevel > highestValue) {
        highestValue = sensor.fillLevel;
      }
    }
  }
  return highestValue;
}

bool Container::checkSensorInvalidState(const int8_t currentfillLevel) const {
  for (auto const &sensor : config.sensorData) {
    if (getSensorState(sensor.channelNumber) == 0) {
      if (sensor.fillLevel < currentfillLevel) {
        return true;
      }
    }
  }
  return false;
}

int8_t Container::getSensorState(const uint8_t channelNumber) const {
  if (channelNumber == 255) {
    return -1;
  }

  auto ch = Supla::Channel::GetByChannelNumber(channelNumber);
  if (ch == nullptr) {
    SUPLA_LOG_WARNING(
        "Container[%d]::getSensorLevel: "
        "channel %d not found",
        getChannelNumber(),
        channelNumber);
    return -1;
  }

  if (ch->getChannelType() != SUPLA_CHANNELTYPE_BINARYSENSOR) {
    SUPLA_LOG_WARNING(
        "Container[%d]::getSensorLevel: "
        "channel %d is not a binary sensor",
        getChannelNumber(),
        channelNumber);
    return -1;
  }

  if (ch->getValueBool() == true) {
    return 1;
  }

  return 0;
}

bool Container::isAlarmingUsed() const {
  return config.alarmAboveLevel > 0 || config.alarmBelowLevel > 0 ||
         config.warningAboveLevel > 0 || config.warningBelowLevel > 0;
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
  if (!cfg) {
    return;
  }

  loadFunctionFromConfig();

  char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
  generateKey(key, Supla::ConfigTag::ContainerTag);

  ContainerConfig tempConfig = {};
  if (cfg->getBlob(
          key, reinterpret_cast<char *>(&tempConfig), sizeof(tempConfig))) {
    if (tempConfig.warningAboveLevel <= 101 &&
        tempConfig.alarmAboveLevel <= 101 &&
        tempConfig.warningBelowLevel <= 101 &&
        tempConfig.alarmBelowLevel <= 101) {
      config = tempConfig;
      printConfig();
    } else {
      SUPLA_LOG_WARNING("Container[%d]: invalid config values",
                        getChannelNumber());
    }
  } else {
    SUPLA_LOG_DEBUG("Container[%d]: no saved config found", getChannelNumber());
  }
}

uint8_t Container::applyChannelConfig(TSD_ChannelConfig *result, bool) {
  SUPLA_LOG_DEBUG(
      "Contaier[%d]:applyChannelConfig, func %d, configtype %d, configsize %d",
      getChannelNumber(),
      result->Func,
      result->ConfigType,
      result->ConfigSize);

  if (result->ConfigSize == 0) {
    return SUPLA_CONFIG_RESULT_TRUE;
  }

  setAndSaveFunction(result->Func);

  switch (result->Func) {
    case SUPLA_CHANNELFNC_CONTAINER:
    case SUPLA_CHANNELFNC_SEPTIC_TANK:
    case SUPLA_CHANNELFNC_WATER_TANK: {
      if (result->ConfigType == 0 &&
          result->ConfigSize == sizeof(TChannelConfig_Container)) {
        auto cfg = reinterpret_cast<TChannelConfig_Container *>(result->Config);
        if (cfg->WarningAboveLevel <= 101) {
          config.warningAboveLevel = cfg->WarningAboveLevel;
        }
        if (cfg->AlarmAboveLevel <= 101) {
          config.alarmAboveLevel = cfg->AlarmAboveLevel;
        }
        if (cfg->WarningBelowLevel <= 101) {
          config.warningBelowLevel = cfg->WarningBelowLevel;
        }
        if (cfg->AlarmBelowLevel <= 101) {
          config.alarmBelowLevel = cfg->AlarmBelowLevel;
        }

        for (unsigned int i = 0;
             i < sizeof(cfg->SensorInfo) / sizeof(cfg->SensorInfo[0]);
             i++) {
          if (cfg->SensorInfo[i].IsSet == 0) {
            config.sensorData[i].channelNumber = 255;
            config.sensorData[i].fillLevel = 0;
          } else {
            config.sensorData[i].channelNumber = cfg->SensorInfo[i].ChannelNo;
            config.sensorData[i].fillLevel = cfg->SensorInfo[i].FillLevel;
            if (config.sensorData[i].fillLevel > 100) {
              config.sensorData[i].fillLevel = 100;
            }
          }
        }
      }
      printConfig();
      saveConfig();
      break;
    }
    default: {
      SUPLA_LOG_WARNING("Container[%d]: unsupported func %d",
                        getChannelNumber(),
                        result->Func);
      break;
    }
  }
  return SUPLA_CONFIG_RESULT_TRUE;
}


void Container::printConfig() const {
  SUPLA_LOG_DEBUG(
      "Container[%d]: warning above: %d, alarm above: %d, warning below: "
      "%d, alarm below: %d",
      getChannelNumber(),
      config.warningAboveLevel,
      config.alarmAboveLevel,
      config.warningBelowLevel,
      config.alarmBelowLevel);

  for (auto const &sensor : config.sensorData) {
    if (sensor.channelNumber == 255) {
      continue;
    }
    SUPLA_LOG_DEBUG("Container[%d]: sensor channel %d, fill level: %d",
                    getChannelNumber(),
                    sensor.channelNumber,
                    sensor.fillLevel);
  }
}

void Container::fillChannelConfig(void *channelConfig, int *size) {
  if (size) {
    *size = 0;
  } else {
    return;
  }
  if (channelConfig == nullptr) {
    return;
  }

  switch (channel.getDefaultFunction()) {
    case SUPLA_CHANNELFNC_CONTAINER:
    case SUPLA_CHANNELFNC_SEPTIC_TANK:
    case SUPLA_CHANNELFNC_WATER_TANK: {
      auto cfg = reinterpret_cast<TChannelConfig_Container *>(
          channelConfig);
      *size = sizeof(TChannelConfig_Container);
      cfg->AlarmAboveLevel = config.alarmAboveLevel;
      cfg->AlarmBelowLevel = config.alarmBelowLevel;
      cfg->WarningAboveLevel = config.warningAboveLevel;
      cfg->WarningBelowLevel = config.warningBelowLevel;

      for (unsigned int i = 0;
           i < sizeof(cfg->SensorInfo) / sizeof(cfg->SensorInfo[0]);
           i++) {
        cfg->SensorInfo[i].IsSet = 0;
        cfg->SensorInfo[i].ChannelNo = 0;
        cfg->SensorInfo[i].FillLevel = 0;
        if (config.sensorData[i].channelNumber < 255) {
          cfg->SensorInfo[i].IsSet = 1;
          cfg->SensorInfo[i].ChannelNo = config.sensorData[i].channelNumber;
          cfg->SensorInfo[i].FillLevel = config.sensorData[i].fillLevel;
        }
      }

      break;
    }
    default:
      SUPLA_LOG_WARNING(
          "Container[%d]: fill channel config for unknown function %d",
          channel.getChannelNumber(),
          channel.getDefaultFunction());
      return;
  }
}

void Container::saveConfig() const {
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    generateKey(key, Supla::ConfigTag::ContainerTag);
    if (cfg->setBlob(key,
                     reinterpret_cast<const char *>(&config),
                     sizeof(ContainerConfig))) {
      SUPLA_LOG_INFO("Container[%d]: config saved successfully",
                     getChannelNumber());
    } else {
      SUPLA_LOG_WARNING("Container[%d]: failed to save config",
                        getChannelNumber());
    }

    cfg->saveWithDelay(5000);
  }
  for (auto proto = Supla::Protocol::ProtocolLayer::first();
      proto != nullptr; proto = proto->next()) {
    proto->notifyConfigChange(getChannelNumber());
  }
}

void Container::setSoundAlarmSupported(bool soundAlarmSupported) {
  this->soundAlarmSupported = soundAlarmSupported;
}

bool Container::isSoundAlarmSupported() const {
  return soundAlarmSupported;
}

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

#include <supla/suplet/config.h>

#if SUPLA_SUPLET_ENABLED

#include <math.h>
#include <string.h>
#include <supla/channels/channel.h>
#include <supla/element.h>
#include <supla/sensor/thermometer_driver.h>
#include <supla/suplet/thermometer_group.h>
#include <supla/time.h>

namespace {

const uint8_t kConfigVersion = 1;
const uint16_t kDefaultRefreshIntervalMs = 1000;

struct SerializedConfig {
  uint8_t version = kConfigVersion;
  uint8_t mode = static_cast<uint8_t>(Supla::Suplet::ThermometerGroupMode::Avg);
  uint16_t refreshIntervalMs = kDefaultRefreshIntervalMs;
  uint8_t sourceCount = 0;
  int16_t sourceChannels[SUPLA_SUPLET_THERMOMETER_GROUP_MAX_SOURCES] = {};
};

bool isValidMode(uint8_t mode) {
  return mode ==
             static_cast<uint8_t>(Supla::Suplet::ThermometerGroupMode::Avg) ||
         mode ==
             static_cast<uint8_t>(Supla::Suplet::ThermometerGroupMode::Min) ||
         mode == static_cast<uint8_t>(Supla::Suplet::ThermometerGroupMode::Max);
}

}  // namespace

namespace Supla {
namespace Suplet {

ThermometerGroup::ThermometerGroup(uint8_t subDeviceId,
                                   int channelNumber,
                                   const ThermometerGroupConfig &config)
    : VirtualThermometer(subDeviceId, channelNumber), config(config) {
  if (this->config.refreshIntervalMs == 0) {
    this->config.refreshIntervalMs = kDefaultRefreshIntervalMs;
  }
}

void ThermometerGroup::iterateAlways() {
  if (millis() - lastRefreshMs < config.refreshIntervalMs) {
    return;
  }
  lastRefreshMs = millis();
  GroupValue value = calculateGroupValue();
  if (!value.hasOnlineSource) {
    setValue(TEMPERATURE_NOT_AVAILABLE);
    getChannel()->setNewValue(TEMPERATURE_NOT_AVAILABLE);
    getChannel()->setStateOffline();
    return;
  }

  getChannel()->setStateOnline();
  setValue(value.value);
  getChannel()->setNewValue(value.value);
}

double ThermometerGroup::calculateValue() const {
  return calculateGroupValue().value;
}

ThermometerGroup::GroupValue ThermometerGroup::calculateGroupValue() const {
  GroupValue result = {};
  uint8_t validValueCount = 0;

  for (uint8_t i = 0; i < config.sourceCount; i++) {
    auto element =
        Supla::Element::getElementByChannelNumber(config.sourceChannels[i]);
    if (element == nullptr || element->getChannel() == nullptr) {
      continue;
    }

    if (!element->getChannel()->isStateOnline()) {
      continue;
    }
    result.hasOnlineSource = true;

    double value = element->getChannel()->getValueDouble();
    if (!isValidTemperature(value)) {
      continue;
    }

    if (validValueCount == 0) {
      result.value = value;
      result.hasValidValue = true;
    } else if (config.mode == ThermometerGroupMode::Min) {
      if (value < result.value) {
        result.value = value;
      }
    } else if (config.mode == ThermometerGroupMode::Max) {
      if (value > result.value) {
        result.value = value;
      }
    } else {
      result.value += value;
    }
    validValueCount++;
  }

  if (!result.hasValidValue) {
    result.value = TEMPERATURE_NOT_AVAILABLE;
    return result;
  }
  if (config.mode == ThermometerGroupMode::Avg) {
    result.value = result.value / validValueCount;
  }
  return result;
}

const ThermometerGroupConfig &ThermometerGroup::getConfig() const {
  return config;
}

bool ThermometerGroup::isValidTemperature(double value) {
  return !isnan(value) && value > TEMPERATURE_NOT_AVAILABLE;
}

bool parseThermometerGroupConfig(const uint8_t *data,
                                 uint16_t dataSize,
                                 ThermometerGroupConfig *config) {
  if (data == nullptr || config == nullptr ||
      dataSize != sizeof(SerializedConfig)) {
    return false;
  }

  SerializedConfig serialized = {};
  memcpy(&serialized, data, sizeof(serialized));
  if (serialized.version != kConfigVersion || !isValidMode(serialized.mode) ||
      serialized.sourceCount == 0 ||
      serialized.sourceCount > SUPLA_SUPLET_THERMOMETER_GROUP_MAX_SOURCES) {
    return false;
  }

  config->version = serialized.version;
  config->mode = static_cast<ThermometerGroupMode>(serialized.mode);
  config->refreshIntervalMs = serialized.refreshIntervalMs == 0
                                  ? kDefaultRefreshIntervalMs
                                  : serialized.refreshIntervalMs;
  config->sourceCount = serialized.sourceCount;
  for (uint8_t i = 0; i < config->sourceCount; i++) {
    if (serialized.sourceChannels[i] < 0) {
      return false;
    }
    config->sourceChannels[i] = serialized.sourceChannels[i];
  }
  for (uint8_t i = config->sourceCount;
       i < SUPLA_SUPLET_THERMOMETER_GROUP_MAX_SOURCES;
       i++) {
    config->sourceChannels[i] = 0;
  }
  return true;
}

bool serializeThermometerGroupConfig(const ThermometerGroupConfig &config,
                                     uint8_t *data,
                                     uint16_t dataSize,
                                     uint16_t *writtenSize) {
  if (data == nullptr || dataSize < sizeof(SerializedConfig) ||
      !isValidMode(static_cast<uint8_t>(config.mode)) ||
      config.sourceCount == 0 ||
      config.sourceCount > SUPLA_SUPLET_THERMOMETER_GROUP_MAX_SOURCES) {
    return false;
  }

  SerializedConfig serialized = {};
  serialized.version = kConfigVersion;
  serialized.mode = static_cast<uint8_t>(config.mode);
  serialized.refreshIntervalMs = config.refreshIntervalMs == 0
                                     ? kDefaultRefreshIntervalMs
                                     : config.refreshIntervalMs;
  serialized.sourceCount = config.sourceCount;
  for (uint8_t i = 0; i < config.sourceCount; i++) {
    if (config.sourceChannels[i] < 0) {
      return false;
    }
    serialized.sourceChannels[i] = config.sourceChannels[i];
  }

  memcpy(data, &serialized, sizeof(serialized));
  if (writtenSize != nullptr) {
    *writtenSize = sizeof(serialized);
  }
  return true;
}

}  // namespace Suplet
}  // namespace Supla

#endif  // SUPLA_SUPLET_ENABLED

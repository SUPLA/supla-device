/*
 Copyright (C) AC SOFTWARE SP. Z O.O.

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

#include "multi_ds_sensor.h"
#include "multi_ds_handler_base.h"

#include <supla/log_wrapper.h>
#include <supla/storage/config_tags.h>

using Supla::Sensor::MultiDsSensor;

void MultiDsSensor::onInit() {
  channel.setFlag(SUPLA_CHANNEL_FLAG_ALWAYS_ALLOW_CHANNEL_DELETION);
}

void MultiDsSensor::iterateAlways() {
  if (millis() - lastReadTime > 1000) {
    channel.setNewValue(getValue());
    lastReadTime = millis();
  }
}

double MultiDsSensor::getValue() {
  double value = handler->getTemperature(address);
  if (value == DEVICE_DISCONNECTED_C) {
    channel.setStateOffline();
    lastValidValue = TEMPERATURE_NOT_AVAILABLE;
    return lastValidValue;
  }

  if (!channel.isStateOnline()) {
    channel.setStateOnline();
  }

  if (value == 85.0) {
    value = TEMPERATURE_NOT_AVAILABLE;
  }

  if (value == TEMPERATURE_NOT_AVAILABLE) {
    retryCounter++;
    if (retryCounter > 3) {
      retryCounter = 0;
    } else {
      value = lastValidValue;
    }
  } else {
    retryCounter = 0;
  }

  lastValidValue = value;
  return value;
}

void MultiDsSensor::saveSensorConfig() {
  auto config = Supla::Storage::ConfigInstance();
  if (config) {
    char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    Supla::Config::generateKey(key, subDeviceId,
                               Supla::ConfigTag::DsSensorConfig);
    SUPLA_LOG_DEBUG("MultiDS: Saving config for key %s", key);
    DsSensorConfig sensorConfig;
    sensorConfig.channelNumber = channel.getChannelNumber();
    memcpy(sensorConfig.address, address, 8);
    config->setBlob(key, reinterpret_cast<char *>(&sensorConfig),
                    sizeof(sensorConfig));

    config->commit();
  }
}

void MultiDsSensor::purgeConfig() {
  Supla::Sensor::Thermometer::purgeConfig();

  auto config = Supla::Storage::ConfigInstance();
  if (config) {
    char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    Supla::Config::generateKey(key, subDeviceId,
                               Supla::ConfigTag::DsSensorConfig);
    SUPLA_LOG_DEBUG("MultiDS: Erasing config for key %s", key);
    config->eraseKey(key);
    config->commit();
  }
}

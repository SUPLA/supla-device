// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "multi_ds_sensor.h"
#include "multi_ds_handler_base.h"

#include <supla/log_wrapper.h>
#include <supla/storage/config_tags.h>
#include <supla/time.h>

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
  // DallasTemperature uses -127 C for a disconnected device. Keep this
  // platform-neutral so the common sensor implementation does not depend on
  // Arduino headers.
  if (value == -127.0) {
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
    Supla::Config::generateKey(key, getSubDeviceId(),
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
    Supla::Config::generateKey(key, getSubDeviceId(),
                               Supla::ConfigTag::DsSensorConfig);
    SUPLA_LOG_DEBUG("MultiDS: Erasing config for key %s", key);
    config->eraseKey(key);
    config->commit();
  }
}

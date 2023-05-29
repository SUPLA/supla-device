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

#include "thermometer.h"

#include <supla/time.h>
#include <supla/storage/config.h>
#include <supla/log_wrapper.h>

#include <stdio.h>

using Supla::Sensor::Thermometer;

Supla::Sensor::Thermometer::Thermometer() : lastReadTime(0) {
  channel.setType(SUPLA_CHANNELTYPE_THERMOMETER);
  channel.setDefault(SUPLA_CHANNELFNC_THERMOMETER);
}

Thermometer::Thermometer(ThermometerDriver *driver) : driver(driver) {
}

void Supla::Sensor::Thermometer::onInit() {
  if (driver) {
    driver->initialize();
  }
  channel.setNewValue(getValue());
}

double Supla::Sensor::Thermometer::getValue() {
  if (driver) {
    return driver->getValue();
  }
  return TEMPERATURE_NOT_AVAILABLE;
}

int16_t Supla::Sensor::Thermometer::getTempInt16() {
  if (getChannelNumber() >= 0) {
    double temp = getChannel()->getLastTemperature();
    if (temp <= TEMPERATURE_NOT_AVAILABLE) {
      return INT16_MIN;
    }
    temp *= 100;
    if (temp > INT16_MAX) {
      return INT16_MAX;
    }
    if (temp <= INT16_MIN) {
      return INT16_MIN + 1;
    }
    return temp;
  }
  return INT16_MIN;
}

void Supla::Sensor::Thermometer::iterateAlways() {
  if (millis() - lastReadTime > refreshIntervalMs) {
    lastReadTime = millis();
    channel.setNewValue(getValue());
  }
}

void Supla::Sensor::Thermometer::onLoadConfig(SuplaDeviceClass *sdc) {
  (void)(sdc);
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    int32_t value = 0;
    char key[16] = {};
    snprintf(key, sizeof(key), "corr_%d_0", getChannelNumber());
    if (cfg->getInt32(key, &value)) {
      double correction = 1.0 * value / 10.0;
      getChannel()->setCorrection(correction);
      SUPLA_LOG_DEBUG("Channel[%d] temperature correction %f",
          getChannelNumber(), correction);
    }
  }
}

double Supla::Sensor::Thermometer::getLastTemperature() {
  return getChannel()->getValueDouble();
}

void Supla::Sensor::Thermometer::setRefreshIntervalMs(int intervalMs) {
  refreshIntervalMs = intervalMs;
}


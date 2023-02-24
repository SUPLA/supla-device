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

Supla::Sensor::Thermometer::Thermometer() : lastReadTime(0) {
  channel.setType(SUPLA_CHANNELTYPE_THERMOMETER);
  channel.setDefault(SUPLA_CHANNELFNC_THERMOMETER);
}

void Supla::Sensor::Thermometer::onInit() {
  channel.setNewValue(getValue());
}

double Supla::Sensor::Thermometer::getValue() {
  return TEMPERATURE_NOT_AVAILABLE;
}

void Supla::Sensor::Thermometer::iterateAlways() {
  if (millis() - lastReadTime > refreshIntervalMs) {
    lastReadTime = millis();
    channel.setNewValue(getValue());
  }
}

void Supla::Sensor::Thermometer::onLoadConfig() {
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


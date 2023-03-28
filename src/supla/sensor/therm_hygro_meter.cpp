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

#include "therm_hygro_meter.h"

#include <supla/time.h>
#include <supla/storage/config.h>
#include <supla/sensor/thermometer.h>
#include <supla/log_wrapper.h>

#include <stdio.h>

Supla::Sensor::ThermHygroMeter::ThermHygroMeter() {
  channel.setType(SUPLA_CHANNELTYPE_HUMIDITYANDTEMPSENSOR);
  channel.setDefault(SUPLA_CHANNELFNC_HUMIDITYANDTEMPERATURE);
}

void Supla::Sensor::ThermHygroMeter::onInit() {
  channel.setNewValue(getTemp(), getHumi());
}

double Supla::Sensor::ThermHygroMeter::getTemp() {
  return TEMPERATURE_NOT_AVAILABLE;
}

double Supla::Sensor::ThermHygroMeter::getHumi() {
  return HUMIDITY_NOT_AVAILABLE;
}

int16_t Supla::Sensor::ThermHygroMeter::getHumiInt16() {
  if (getChannelNumber() >= 0) {
    double humi = getChannel()->getValueDoubleSecond();
    if (humi <= HUMIDITY_NOT_AVAILABLE) {
      return INT16_MIN;
    }
    humi *= 100;
    if (humi > INT16_MAX) {
      return INT16_MAX;
    }
    if (humi <= INT16_MIN) {
      return INT16_MIN + 1;
    }
    return humi;
  }
  return INT16_MIN;
}

void Supla::Sensor::ThermHygroMeter::iterateAlways() {
  if (millis() - lastReadTime > 10000) {
    lastReadTime = millis();
    channel.setNewValue(getTemp(), getHumi());
  }
}

void Supla::Sensor::ThermHygroMeter::onLoadConfig() {
  // set temperature correction
  Supla::Sensor::Thermometer::onLoadConfig();

  // set humidity correction:
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    int32_t value = 0;
    char key[16] = {};
    snprintf(key, sizeof(key), "corr_%d_1", getChannelNumber());
    if (cfg->getInt32(key, &value)) {
      double correction = 1.0 * value / 10.0;
      getChannel()->setCorrection(correction, true);
      SUPLA_LOG_DEBUG("Channel[%d] humidity correction %f",
          getChannelNumber(), correction);
    }
  }
}

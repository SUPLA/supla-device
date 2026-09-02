// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "thermometer.h"

#include <supla/time.h>
#include <supla/storage/config.h>

#include <stdio.h>

using Supla::Sensor::Thermometer;

Supla::Sensor::Thermometer::Thermometer() {
  channel.setType(SUPLA_CHANNELTYPE_THERMOMETER);
  channel.setDefaultFunction(SUPLA_CHANNELFNC_THERMOMETER);
}

Thermometer::Thermometer(ThermometerDriver *driver) : Thermometer() {
  this->driver = driver;
}

void Supla::Sensor::Thermometer::onInit() {
  if (driver) {
    driver->initialize();
  }
  channel.setNewValue(getTemp());
}

double Supla::Sensor::Thermometer::getValue() {
  if (driver) {
    return driver->getValue();
  }
  return TEMPERATURE_NOT_AVAILABLE;
}


void Supla::Sensor::Thermometer::iterateAlways() {
  if (millis() - lastReadTime > refreshIntervalMs) {
    lastReadTime = millis();
    channel.setNewValue(getTemp());
  }
}

void Supla::Sensor::Thermometer::setHumidityCorrection(int32_t correction) {
  (void)(correction);
}

double Supla::Sensor::Thermometer::getTemp() {
  return getValue();
}

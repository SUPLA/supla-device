// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "thermometer_driver.h"

using Supla::Sensor::ThermometerDriver;

ThermometerDriver::ThermometerDriver() {
}

int16_t ThermometerDriver::getTempInt16() {
  double temp = getValue();
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

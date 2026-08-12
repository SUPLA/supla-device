// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <supla/log_wrapper.h>
#include <supla/time.h>

#include "ntc10k.h"

Supla::Sensor::NTC10k::NTC10k() {
}

void Supla::Sensor::NTC10k::onInit() {
}

void Supla::Sensor::NTC10k::readSensor() {
//  double temperature = 0;
//  SUPLA_LOG_DEBUG("NTC10k: temp: %.2f", temperature);
//  lastValidTemp = temperature;
}

double Supla::Sensor::NTC10k::getValue() {
  readSensor();
  return lastValidTemp;
}

void Supla::Sensor::NTC10k::set(double val) {
  lastValidTemp = val;
}

void Supla::Sensor::NTC10k::iterateAlways() {
  if (millis() - lastReadTime > 2000) {
    lastReadTime = millis();
    channel.setNewValue(getValue());
  }
}

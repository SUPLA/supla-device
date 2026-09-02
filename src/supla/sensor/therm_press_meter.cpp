// SPDX-FileCopyrightText: malarz
// SPDX-License-Identifier: GPL-2.0-or-later

#include "therm_press_meter.h"

#include <supla/time.h>

Supla::Sensor::ThermPressMeter::ThermPressMeter() {
  channel.setType(SUPLA_CHANNELTYPE_THERMOMETER);
  channel.setDefaultFunction(SUPLA_CHANNELFNC_THERMOMETER);
}

void Supla::Sensor::ThermPressMeter::iterateAlways() {
  if (millis() - lastReadTime > refreshIntervalMs) {
    lastReadTime = millis();
    channel.setNewValue(getTemp());
    pressureChannel.setNewValue(getPressure());
  }
}

void Supla::Sensor::ThermPressMeter::setHumidityCorrection(int32_t correction) {
  (void)(correction);
}

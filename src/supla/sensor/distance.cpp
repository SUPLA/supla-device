// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "distance.h"
#include <supla/time.h>

Supla::Sensor::Distance::Distance() {
  channel.setType(SUPLA_CHANNELTYPE_DISTANCESENSOR);
  channel.setDefaultFunction(SUPLA_CHANNELFNC_DISTANCESENSOR);
  channel.setNewValue(DISTANCE_NOT_AVAILABLE);
}

double Supla::Sensor::Distance::getValue() {
  return DISTANCE_NOT_AVAILABLE;
}

void Supla::Sensor::Distance::iterateAlways() {
  if (millis() - lastReadTime >= readIntervalMs) {
    lastReadTime = millis();
    channel.setNewValue(getValue());
  }
}

void Supla::Sensor::Distance::setReadIntervalMs(uint32_t timeMs) {
  if (timeMs < 10) {
    timeMs = 10;
  }
  readIntervalMs = timeMs;
}

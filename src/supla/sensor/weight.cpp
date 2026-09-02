// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "weight.h"

#include <supla/time.h>

using Supla::Sensor::Weight;

Weight::Weight() {
  channel.setType(SUPLA_CHANNELTYPE_WEIGHTSENSOR);
  channel.setDefaultFunction(SUPLA_CHANNELFNC_WEIGHTSENSOR);
  channel.setNewValue(WEIGHT_NOT_AVAILABLE);
}

void Weight::setRefreshIntervalMs(int intervalMs) {
  refreshIntervalMs = intervalMs;
}

double Weight::getValue() {
  return WEIGHT_NOT_AVAILABLE;
}

void Weight::handleAction(int event, int action) {
  (void)(event);
  switch (action) {
    case Supla::TARE_SCALES: {
      tareScales();
      break;
    }
  }
}

void Weight::iterateAlways() {
  if (millis() - lastReadTime > refreshIntervalMs) {
    lastReadTime = millis();
    channel.setNewValue(getValue());
  }
}

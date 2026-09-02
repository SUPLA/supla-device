// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "hygro_meter.h"

Supla::Sensor::HygroMeter::HygroMeter() {
  channel.setType(SUPLA_CHANNELTYPE_HUMIDITYSENSOR);
  channel.setDefaultFunction(SUPLA_CHANNELFNC_HUMIDITY);
}

void Supla::Sensor::HygroMeter::setTemperatureCorrection(int32_t correction) {
  (void)(correction);
}

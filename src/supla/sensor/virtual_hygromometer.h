// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_SENSOR_VIRTUAL_HYGROMOMETER_H_
#define SRC_SUPLA_SENSOR_VIRTUAL_HYGROMOMETER_H_

#include "hygro_meter.h"

namespace Supla {
namespace Sensor {
class VirtualHygroMeter : public Supla::Sensor::HygroMeter {
 public:
  double getHumi() override {
    return humidity;
  }
  void setValue(double val) {
    humidity = val;
  }

 protected:
  double humidity = HUMIDITY_NOT_AVAILABLE;
};
};  // namespace Sensor
};  // namespace Supla


#endif  // SRC_SUPLA_SENSOR_VIRTUAL_HYGROMOMETER_H_

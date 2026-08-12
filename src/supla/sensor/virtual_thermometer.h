// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_SENSOR_VIRTUAL_THERMOMETER_H_
#define SRC_SUPLA_SENSOR_VIRTUAL_THERMOMETER_H_

#include "thermometer.h"

namespace Supla {
namespace Sensor {
class VirtualThermometer : public Supla::Sensor::Thermometer {
 public:
  double getValue() override {
    return temperature;
  }
  void setValue(double val) {
    temperature = val;
  }

 protected:
  double temperature = TEMPERATURE_NOT_AVAILABLE;
};
};  // namespace Sensor
};  // namespace Supla

#endif  // SRC_SUPLA_SENSOR_VIRTUAL_THERMOMETER_H_

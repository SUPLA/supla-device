// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_SENSOR_VIRTUAL_THERM_HYGRO_METER_H_
#define SRC_SUPLA_SENSOR_VIRTUAL_THERM_HYGRO_METER_H_

#include "therm_hygro_meter.h"

namespace Supla {
namespace Sensor {
class VirtualThermHygroMeter : public Supla::Sensor::ThermHygroMeter {
 public:
  double getTemp() override {
    return temperature;
  }

  double getHumi() override {
    return humidity;
  }

  void setTemp(double val) {
    temperature = val;
  }

  void setHumi(double val) {
    humidity = val;
  }

 protected:
  double temperature = TEMPERATURE_NOT_AVAILABLE;
  double humidity = HUMIDITY_NOT_AVAILABLE;
};
};  // namespace Sensor
};  // namespace Supla

#endif  // SRC_SUPLA_SENSOR_VIRTUAL_THERM_HYGRO_METER_H_

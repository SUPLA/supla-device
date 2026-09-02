// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_SENSOR_NTC10K_H_
#define SRC_SUPLA_SENSOR_NTC10K_H_

#include <supla/sensor/thermometer.h>

namespace Supla {
namespace Sensor {
class NTC10k : public Thermometer {
 public:
  NTC10k();

  void onInit() override;
  void iterateAlways() override;

  double getValue() override;

  void set(double);
  void readSensor();

 protected:
  double lastValidTemp = TEMPERATURE_NOT_AVAILABLE;
  int8_t retryCountTemp = 0;
};

};  // namespace Sensor
};  // namespace Supla

#endif  // SRC_SUPLA_SENSOR_NTC10K_H_

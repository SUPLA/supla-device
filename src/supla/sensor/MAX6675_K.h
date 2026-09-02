// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_SENSOR_MAX6675_K_H_
#define SRC_SUPLA_SENSOR_MAX6675_K_H_

#include <Arduino.h>
#include <supla/sensor/thermometer.h>

namespace Supla {
namespace Sensor {
class MAX6675_K : public Thermometer {
 public:
  MAX6675_K(uint8_t pin_CLK, uint8_t pin_CS, uint8_t pin_DO);
  double getValue();

 private:
  void onInit();
  byte spiRead();

 protected:
  int8_t pin_CLK;
  int8_t pin_CS;
  int8_t pin_DO;
};

};  // namespace Sensor
};  // namespace Supla

#endif  // SRC_SUPLA_SENSOR_MAX6675_K_H_

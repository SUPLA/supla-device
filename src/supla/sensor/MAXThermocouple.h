// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_SENSOR_MAXTHERMOCOUPLE_H_
#define SRC_SUPLA_SENSOR_MAXTHERMOCOUPLE_H_

#include <Arduino.h>
#include <supla/sensor/thermometer.h>

namespace Supla {
namespace Sensor {
class MAXThermocouple : public Thermometer {
 public:
  MAXThermocouple(uint8_t pin_CLK, uint8_t pin_CS, uint8_t pin_DO);
  double getValue();

 private:
  void onInit();
  uint32_t spiRead(void);

 protected:
  int8_t pin_CLK;
  int8_t pin_CS;
  int8_t pin_DO;
};

};  // namespace Sensor
};  // namespace Supla

#endif  // SRC_SUPLA_SENSOR_MAXTHERMOCOUPLE_H_

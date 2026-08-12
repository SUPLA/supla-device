// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_SENSOR_SI7021_SONOFF_H_
#define SRC_SUPLA_SENSOR_SI7021_SONOFF_H_

#include <Arduino.h>
#include <supla/sensor/therm_hygro_meter.h>

namespace Supla {
namespace Sensor {
class Si7021Sonoff : public ThermHygroMeter {
 public:
  explicit Si7021Sonoff(int pin);
  double getTemp();
  double getHumi();

 private:
  void iterateAlways();
  void onInit();
  double readTemp(uint8_t* data);
  double readHumi(uint8_t* data);
  void read();
  bool waitState(bool state);

 protected:
  int8_t pin;
  double temperature;
  double humidity;
  int8_t retryCount;
};

};  // namespace Sensor
};  // namespace Supla

#endif  // SRC_SUPLA_SENSOR_SI7021_SONOFF_H_

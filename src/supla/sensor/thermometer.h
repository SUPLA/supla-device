// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_SENSOR_THERMOMETER_H_
#define SRC_SUPLA_SENSOR_THERMOMETER_H_

#include <supla/sensor/thermometer_driver.h>

#include "therm_hygro_meter.h"

namespace Supla {
namespace Sensor {
class Thermometer : public ThermHygroMeter {
 public:
  Thermometer();
  explicit Thermometer(ThermometerDriver *driver);
  virtual double getValue();
  void onInit() override;
  void iterateAlways() override;
  double getTemp() override;

 protected:
  void setHumidityCorrection(int32_t correction) override;
  ThermometerDriver *driver = nullptr;
};

};  // namespace Sensor
};  // namespace Supla

#endif  // SRC_SUPLA_SENSOR_THERMOMETER_H_

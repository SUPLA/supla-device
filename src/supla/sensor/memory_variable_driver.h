// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_SENSOR_MEMORY_VARIABLE_DRIVER_H_
#define SRC_SUPLA_SENSOR_MEMORY_VARIABLE_DRIVER_H_

#include <math.h>

#include "measurement_driver.h"

namespace Supla {
namespace Sensor {
class MemoryVariableDriver : public MeasurementDriver {
 public:
  MemoryVariableDriver() = default;
  ~MemoryVariableDriver() = default;

  void initialize() override;
  double getValue() override;
  void setValue(const double &value) override;

 private:
  double value = NAN;
};
}  // namespace Sensor
}  // namespace Supla

#endif  // SRC_SUPLA_SENSOR_MEMORY_VARIABLE_DRIVER_H_

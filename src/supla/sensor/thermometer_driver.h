// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_SENSOR_THERMOMETER_DRIVER_H_
#define SRC_SUPLA_SENSOR_THERMOMETER_DRIVER_H_

#include <stdint.h>

#include "measurement_driver.h"

#define TEMPERATURE_NOT_AVAILABLE -275.0

namespace Supla {
namespace Sensor {
class ThermometerDriver : public MeasurementDriver {
 public:
  ThermometerDriver();
  virtual ~ThermometerDriver() = default;

  virtual int16_t getTempInt16();
};

}  // namespace Sensor
}  // namespace Supla

#endif  // SRC_SUPLA_SENSOR_THERMOMETER_DRIVER_H_

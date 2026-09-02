// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_SENSOR_MEASUREMENT_DRIVER_H_
#define SRC_SUPLA_SENSOR_MEASUREMENT_DRIVER_H_

namespace Supla {
namespace Sensor {
class MeasurementDriver {
 public:
  MeasurementDriver() = default;
  virtual ~MeasurementDriver() = default;

  virtual void initialize() = 0;
  virtual double getValue() = 0;
  virtual void setValue(const double &) {}
};

}  // namespace Sensor
}  // namespace Supla
#endif  // SRC_SUPLA_SENSOR_MEASUREMENT_DRIVER_H_

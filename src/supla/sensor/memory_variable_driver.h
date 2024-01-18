/*
   Copyright (C) AC SOFTWARE SP. Z O.O

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
   */

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

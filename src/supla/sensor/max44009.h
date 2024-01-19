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

// Dependencies:
// https://github.com/RobTillaart/Max44009

// Defualt I2C address: 0x4A
// Alternative I2C address: 0x4B

#ifndef SRC_SUPLA_SENSOR_MAX44009_H_
#define SRC_SUPLA_SENSOR_MAX44009_H_

#include <supla/sensor/general_purpose_measurement.h>

#include <math.h>
#include <Wire.h>
#include <Max44009.h>
#include <supla/log_wrapper.h>

namespace Supla {
namespace Sensor {
class Max44009 : public GeneralPurposeMeasurement {
 public:
  explicit Max44009(int i2cAddress = 0x4A, TwoWire *wire = &Wire)
      : GeneralPurposeMeasurement(nullptr, false), sensor(i2cAddress, wire) {
    setDefaultUnitAfterValue("lx");
  }

  void onInit() override {
    channel.setNewValue(getValue());
  }

 protected:
  double getValue() override {
    double value = sensor.getLux();
    if (isnan(value) || value <= 0) {
      if (invalidCounter < 3) {
        invalidCounter++;
        return lastValue;
      }
      lastValue = NAN;
      return lastValue;
    }
    invalidCounter = 0;
    lastValue = value;
    return value;
  }

  ::Max44009 sensor;

  double lastValue = NAN;
  int invalidCounter = 0;
};

}  // namespace Sensor
}  // namespace Supla

#endif  // SRC_SUPLA_SENSOR_MAX44009_H_

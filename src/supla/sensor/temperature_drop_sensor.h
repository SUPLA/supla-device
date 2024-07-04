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

#ifndef SRC_SUPLA_SENSOR_TEMPERATURE_DROP_SENSOR_H_
#define SRC_SUPLA_SENSOR_TEMPERATURE_DROP_SENSOR_H_

#include <supla/element.h>

#include "virtual_binary.h"
#include "therm_hygro_meter.h"

namespace Supla {
namespace Sensor {

#define MAX_TEMPERATURE_MEASUREMENTS 60

class TemperatureDropSensor : public Supla::Element {
 public:
  explicit TemperatureDropSensor(ThermHygroMeter *thermometer);

  void onInit() override;
  void iterateAlways() override;

  bool isDropDetected() const;

 private:
  Supla::Sensor::VirtualBinary virtualBinary;
  ThermHygroMeter *thermometer = nullptr;

  int16_t getAverage(int fromIndex, int toIndex) const;
  bool detectTemperatureDrop(int16_t temperature, int16_t *average) const;

  uint32_t lastTemperatureUpdate = 0;

  int16_t measurements[MAX_TEMPERATURE_MEASUREMENTS] = {};
  int measurementIndex = 0;
  uint32_t probeIntervalMs = 30000;
  int16_t temperatureDropThreshold = -200;  // -2 degree

  int16_t averageAtDropDetection = INT16_MIN;
  uint32_t dropDetectionTimestamp = 0;
};

}  // namespace Sensor
}  // namespace Supla

#endif  // SRC_SUPLA_SENSOR_TEMPERATURE_DROP_SENSOR_H_

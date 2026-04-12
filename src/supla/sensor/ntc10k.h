/*
 * Copyright (C) AC SOFTWARE SP. Z O.O
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef SRC_SUPLA_SENSOR_NTC10K_H_
#define SRC_SUPLA_SENSOR_NTC10K_H_

#include <supla/sensor/thermometer.h>

namespace Supla {
namespace Sensor {
class NTC10k : public Thermometer {
 public:
  NTC10k(int pin,
         float seriesResistor = 10000,
         float nominalResistance = 10000,
         float nominalTemp = 25,
         float beta = 3950,
         int samples = 10);

  void onInit() override;
  void iterateAlways() override;

  double getValue() override;

  void set(double);
  void readSensor();

 protected:
  int pin;
  float seriesResistor;
  float nominalResistance;
  float nominalTemp;
  float beta;
  int samples;
  double lastValidTemp = TEMPERATURE_NOT_AVAILABLE;
  int8_t retryCountTemp = 0;
  float Analog_Read_Value = 0;
};
};  // namespace Sensor
};  // namespace Supla

#endif  // SRC_SUPLA_SENSOR_NTC10K_H_

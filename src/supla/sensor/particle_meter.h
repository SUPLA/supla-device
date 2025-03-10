/*
   Copyright (C) malarz

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

#ifndef SRC_SUPLA_SENSOR_PARTICLE_METER_H_
#define SRC_SUPLA_SENSOR_PARTICLE_METER_H_

#include <supla/sensor/general_purpose_measurement.h>

namespace Supla {
namespace Sensor {
class ParticleMeter : public Supla::Element {
 public:
  ParticleMeter() {
    lastReadTime = millis();
  }

  double getPM1() {
    return pm1value;
  }
  double getPM2_5() {
    return pm2_5value;
  }
  double getPM4() {
    return pm4value;
  }
  double getPM10() {
    return pm10value;
  }

  GeneralPurposeMeasurement* getPM1channel() {
    return pm1channel;
  }
  GeneralPurposeMeasurement* getPM2_5channel() {
    return pm2_5channel;
  }
  GeneralPurposeMeasurement* getPM4channel() {
    return pm4channel;
  }
  GeneralPurposeMeasurement* getPM10channel() {
    return pm10channel;
  }

 protected:
  uint32_t refreshIntervalMs = 600000;
  uint32_t lastReadTime = 0;

  double pm1value = NAN;
  double pm2_5value = NAN;
  double pm4value = NAN;
  double pm10value = NAN;

  GeneralPurposeMeasurement *pm1channel = nullptr;
  GeneralPurposeMeasurement *pm2_5channel = nullptr;
  GeneralPurposeMeasurement *pm4channel = nullptr;
  GeneralPurposeMeasurement *pm10channel = nullptr;
};

}  // namespace Sensor
}  // namespace Supla

#endif  // SRC_SUPLA_SENSOR_PARTICLE_METER_H_

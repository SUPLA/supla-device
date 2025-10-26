/*
 Copyright (C) AC SOFTWARE SP. Z O.O.

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

#ifndef SRC_SUPLA_SENSOR_HC_SR04_GPM_H_
#define SRC_SUPLA_SENSOR_HC_SR04_GPM_H_

//#include "supla/channel.h"
#include <supla/sensor/general_purpose_measurement.h>

#define DURATION_COUNT 2

namespace Supla {
namespace Sensor {
class HC_SR04_GPM : public GeneralPurposeMeasurement {
 public:
  explicit HC_SR04_GPM(
    int8_t trigPin,
    int8_t echoPin,
    int16_t minIn = 0,
    int16_t maxIn = 500,
    int16_t minOut = 0,
    int16_t maxOut = 500);
  void onInit();
  virtual double getValue();
  void setMinMaxIn(int16_t minIn, int16_t maxIn);
  void setMinMaxOut(int16_t minOut, int16_t maxOut);

 protected:
  int8_t _trigPin;
  int8_t _echoPin;
  int16_t _minIn;
  int16_t _maxIn;
  int16_t _minOut;
  int16_t _maxOut;
  char failCount;
  uint64_t readouts[5];
  int index;
};

};  // namespace Sensor
};  // namespace Supla

#endif  // SRC_SUPLA_SENSOR_HC_SR04_GPM_H_

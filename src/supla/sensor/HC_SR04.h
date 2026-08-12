// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_SENSOR_HC_SR04_H_
#define SRC_SUPLA_SENSOR_HC_SR04_H_

#include "supla/channel.h"
#include "supla/io.h"
#include "supla/sensor/distance.h"

#define DURATION_COUNT 2

namespace Supla {
namespace Sensor {
class HC_SR04 : public Distance {
 public:
  HC_SR04(int8_t trigPin,
          int8_t echoPin,
          int16_t minIn = 0,
          int16_t maxIn = 500,
          int16_t minOut = 0,
          int16_t maxOut = 500,
          Supla::Io::Base *io = nullptr);
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
  Supla::Io::Base *io;
};

};  // namespace Sensor
};  // namespace Supla

#endif  // SRC_SUPLA_SENSOR_HC_SR04_H_

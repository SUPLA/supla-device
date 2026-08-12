// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_SENSOR_IMPULSE_COUNTER_H_
#define SRC_SUPLA_SENSOR_IMPULSE_COUNTER_H_

#include <supla-common/proto.h>
#include <supla/io.h>
#include <supla/sensor/virtual_impulse_counter.h>

namespace Supla {

namespace Sensor {
class ImpulseCounter : public VirtualImpulseCounter {
 public:
  explicit ImpulseCounter(Supla::Io::IoPin impulsePin,
                          bool _detectLowToHigh = false,
                          bool inputPullup = true,
                          uint16_t _debounceDelay = 10,
                          uint16_t minSignalTimeToCountMs = 0);
  ImpulseCounter(Supla::Io::Base *io,
                 int _impulsePin,
                 bool _detectLowToHigh = false,
                 bool inputPullup = true,
                 uint16_t _debounceDelay = 10,
                 uint16_t minSignalTimeToCountMs = 0);
  ImpulseCounter(int _impulsePin,
                 bool _detectLowToHigh = false,
                 bool inputPullup = true,
                 uint16_t _debounceDelay = 10,
                 uint16_t minSignalTimeToCountMs = 0);

  void onInit() override;
  void onFastTimer() override;

 protected:
  uint32_t lastImpulseMillis =
      0;  // Stores timestamp of last impulse (used to ignore
          // changes of state during 10 ms timeframe)
  uint32_t lastChangeMs = 0;

  Supla::Io::IoPin impulsePin;  // Pin where impulses are counted
  uint16_t debounceDelayMs = 10;
  uint16_t minSignalTimeToCountMs = 10;

  bool detectLowToHigh = false;  // defines if we count raining (LOW to HIGH) or
                                 // falling (HIGH to LOW) edge
  int8_t prevState = 0;  // Store previous state of pin (LOW/HIGH). It is used
                         // to track changes on pin state.
  int8_t newStateCandidate = 0;  // Stores new state of pin (LOW/HIGH)
};

}  // namespace Sensor
}  // namespace Supla

#endif  // SRC_SUPLA_SENSOR_IMPULSE_COUNTER_H_

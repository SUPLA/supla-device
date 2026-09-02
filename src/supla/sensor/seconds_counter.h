// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_SENSOR_SECONDS_COUNTER_H_
#define SRC_SUPLA_SENSOR_SECONDS_COUNTER_H_

#include <supla/sensor/virtual_impulse_counter.h>

namespace Supla {
namespace Sensor {
class SecondsCounter : public VirtualImpulseCounter {
 public:
  SecondsCounter();

  void iterateAlways() override;
  void handleAction(int event, int action) override;

  void enable();
  void disable();

 private:
  uint32_t lastMillis = {};
  uint32_t remainingMillis = {};
  bool isEnabled = false;
};

}  // namespace Sensor
}  // namespace Supla

#endif  // SRC_SUPLA_SENSOR_SECONDS_COUNTER_H_

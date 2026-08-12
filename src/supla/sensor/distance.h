// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_SENSOR_DISTANCE_H_
#define SRC_SUPLA_SENSOR_DISTANCE_H_

#include "supla/channel_element.h"

#define DISTANCE_NOT_AVAILABLE -1.0

namespace Supla {
namespace Sensor {
class Distance : public ChannelElement {
 public:
  Distance();

  virtual double getValue();
  virtual void setReadIntervalMs(uint32_t timeMs);

  void iterateAlways() override;

 protected:
  uint32_t lastReadTime = 0;
  uint32_t readIntervalMs = 100;
};

};  // namespace Sensor
};  // namespace Supla

#endif  // SRC_SUPLA_SENSOR_DISTANCE_H_

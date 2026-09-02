// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_SENSOR_WIND_H_
#define SRC_SUPLA_SENSOR_WIND_H_

#include <supla/channel_element.h>
#include <supla/time.h>

#define WIND_NOT_AVAILABLE -1.0

namespace Supla {
namespace Sensor {
class Wind : public ChannelElement {
 public:
  Wind() {
    channel.setType(SUPLA_CHANNELTYPE_WINDSENSOR);
    channel.setDefaultFunction(SUPLA_CHANNELFNC_WINDSENSOR);
    channel.setNewValue(WIND_NOT_AVAILABLE);
  }

  virtual double getValue() {
    return WIND_NOT_AVAILABLE;
  }

  void iterateAlways() {
    if (millis() - lastReadTime > 10000) {
      lastReadTime = millis();
      channel.setNewValue(getValue());
    }
  }

 protected:
  uint32_t lastReadTime = 0;
};

};  // namespace Sensor
};  // namespace Supla

#endif  // SRC_SUPLA_SENSOR_WIND_H_

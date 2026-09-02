// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_SENSOR_RAIN_H_
#define SRC_SUPLA_SENSOR_RAIN_H_

#include <supla/channel_element.h>
#include <supla/time.h>

#define RAIN_NOT_AVAILABLE -1.0

namespace Supla {
namespace Sensor {
class Rain : public ChannelElement {
 public:
  Rain() : lastReadTime(0) {
    channel.setType(SUPLA_CHANNELTYPE_RAINSENSOR);
    channel.setDefaultFunction(SUPLA_CHANNELFNC_RAINSENSOR);
    channel.setNewValue(RAIN_NOT_AVAILABLE);
  }

  virtual double getValue() {
    return RAIN_NOT_AVAILABLE;
  }

  void iterateAlways() {
    if (millis() - lastReadTime > 10000) {
      lastReadTime = millis();
      channel.setNewValue(getValue());
    }
  }

 protected:
  uint32_t lastReadTime;
};

};  // namespace Sensor
};  // namespace Supla

#endif  // SRC_SUPLA_SENSOR_RAIN_H_

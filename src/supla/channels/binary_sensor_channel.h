// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_CHANNELS_BINARY_SENSOR_CHANNEL_H_
#define SRC_SUPLA_CHANNELS_BINARY_SENSOR_CHANNEL_H_

#include "channel.h"

namespace Supla {
class BinarySensorChannel : public Channel {
 public:
  bool getValueBool() override;
  void setServerInvertLogic(bool invert);
  bool isServerInvertLogic() const;

 protected:
  // serverInvertLogic does not affect state reported to Supla server. However
  // it may be used internally by device in order to respect server settings.
  // Also external integrations (i.e. MQTT) use it to report inverted logic.
  // getValueBool returns value with serverInvertLogic applied.
  bool serverInvertLogic = false;
};

}  // namespace Supla

#endif  // SRC_SUPLA_CHANNELS_BINARY_SENSOR_CHANNEL_H_

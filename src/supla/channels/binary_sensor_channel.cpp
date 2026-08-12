// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "binary_sensor_channel.h"
#include <supla/log_wrapper.h>
#include <supla/channels/channel.h>

using Supla::BinarySensorChannel;

bool BinarySensorChannel::getValueBool() {
  return serverInvertLogic ? !Channel::getValueBool() : Channel::getValueBool();
}

void BinarySensorChannel::setServerInvertLogic(bool invert) {
  SUPLA_LOG_DEBUG(
      "Binary[%d] setServerInvertLogic %d", getChannelNumber(), invert);
  serverInvertLogic = invert;
}

bool BinarySensorChannel::isServerInvertLogic() const {
  return serverInvertLogic;
}

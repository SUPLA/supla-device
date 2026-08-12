// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_PORTING_LINUX_SUPLA_CUSTOM_CHANNEL_H_
#define EXTRAS_PORTING_LINUX_SUPLA_CUSTOM_CHANNEL_H_

#include <supla/element_with_channel_actions.h>
#include <string>
#include "sensor/sensor_parsed.h"

namespace Supla {
class CustomChannel
    : public Supla::Sensor::SensorParsed<Supla::ElementWithChannelActions> {
 public:
  explicit CustomChannel(Supla::Parser::Parser *);

  void onInit() override;

  Supla::Channel *getChannel() override;
//  const char *getValue();
  void setValue(std::string);

 private:
  uint8_t valueBuf[SUPLA_CHANNELVALUE_SIZE] = {};
  Supla::Channel channel;
};

}  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_SUPLA_CUSTOM_CHANNEL_H_


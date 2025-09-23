/*
   Copyright (C) AC SOFTWARE SP. Z O.O

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


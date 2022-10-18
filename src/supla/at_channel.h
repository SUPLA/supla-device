/*
 Copyright (C) AC SOFTWARE SP. Z O.O.

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

#ifndef SRC_SUPLA_AT_CHANNEL_H_
#define SRC_SUPLA_AT_CHANNEL_H_

#include "channel.h"

namespace Supla {

class AtChannel : public Channel {
 public:
  void sendUpdate() override;
  void pushAction(uint32_t action);
  void activateAction(uint32_t action);
  uint32_t popAction();
  void setRelatedChannel(uint8_t channelNumber);
  void setDisablesLocalOperation(uint32_t actions);

 protected:
  uint32_t actionToSend = 0;
};

};  // namespace Supla

#endif  // SRC_SUPLA_AT_CHANNEL_H_

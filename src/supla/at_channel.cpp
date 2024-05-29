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

#include "at_channel.h"

#include <supla/protocol/protocol_layer.h>
#include <supla/device/register_device.h>

namespace Supla {

  void AtChannel::sendUpdate() {
    if (valueChanged) {
      auto actionId = popAction();
      if (actionId) {
        for (auto proto = Supla::Protocol::ProtocolLayer::first();
            proto != nullptr; proto = proto->next()) {
          proto->sendActionTrigger(channelNumber, actionId);
        }
      }
    } else {
      Channel::sendUpdate();
    }
  }

  uint32_t AtChannel::popAction() {
    for (int i = 0; i < 32; i++) {
      if (actionToSend & (1 << i)) {
        actionToSend ^= (1 << i);
        if (actionToSend == 0) {
          clearUpdateReady();
        }
        return (1 << i);
      }
    }
    return 0;
  }

  void AtChannel::pushAction(uint32_t action) {
    actionToSend |= action;
    setUpdateReady();
  }

  void AtChannel::activateAction(uint32_t action) {
    setActionTriggerCaps(getActionTriggerCaps() | action);
  }

  void AtChannel::setRelatedChannel(uint8_t relatedChannel) {
    actionTriggerProperties.relatedChannelNumber = relatedChannel + 1;
  }

  void AtChannel::setDisablesLocalOperation(uint32_t actions) {
    actionTriggerProperties.disablesLocalOperation = actions;
  }

};  // namespace Supla

// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "at_channel.h"

#include <supla/channels/channel.h>
#include <supla/protocol/protocol_layer.h>

#include <string.h>

namespace Supla {

void AtChannel::sendUpdate() {
  if (channelNumber < 0 || channelNumber > 255) {
    return;
  }
  if (isValueUpdateReady()) {
    auto actionId = popAction();
    if (actionId) {
      for (auto proto = Supla::Protocol::ProtocolLayer::first();
           proto != nullptr;
           proto = proto->next()) {
        proto->sendActionTrigger(static_cast<uint8_t>(channelNumber), actionId);
      }
      if (actionToSend || valueUpdatePending) {
        setSendValue();
      }
    } else {
      valueUpdatePending = false;
      Channel::sendUpdate();
    }
  } else {
    Channel::sendUpdate();
  }
}

uint32_t AtChannel::popAction() {
  for (int i = 0; i < 32; i++) {
    if (actionToSend & (1 << i)) {
      actionToSend ^= (1 << i);
      if (actionToSend == 0 && !valueUpdatePending) {
        clearSendValue();
      }
      return (1 << i);
    }
  }
  return 0;
}

void AtChannel::pushAction(uint32_t action) {
  actionToSend |= action;
  setSendValue();
}

void AtChannel::activateAction(uint32_t action) {
  setActionTriggerCaps(getActionTriggerCaps() | action);
}

void AtChannel::setRelatedChannel(uint8_t relatedChannel) {
  TActionTriggerProperties properties = actionTriggerProperties;
  properties.relatedChannelNumber = relatedChannel + 1;
  setActionTriggerProperties(properties);
}

void AtChannel::setDisablesLocalOperation(uint32_t actions) {
  TActionTriggerProperties properties = actionTriggerProperties;
  properties.disablesLocalOperation = actions;
  setActionTriggerProperties(properties);
}

void AtChannel::enableValueUpdates() {
  valueUpdatesEnabled = true;
  valueUpdatePending = false;
  clearSendValue();
}

void AtChannel::setActionTriggerProperties(
    const TActionTriggerProperties &properties) {
  char rawValue[SUPLA_CHANNELVALUE_SIZE] = {};
  memcpy(rawValue, &properties, sizeof(properties));

  if (setNewValue(rawValue)) {
    if (valueUpdatesEnabled) {
      valueUpdatePending = true;
    } else {
      clearSendValue();
    }
  }
}

};  // namespace Supla

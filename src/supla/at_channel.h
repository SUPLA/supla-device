// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_AT_CHANNEL_H_
#define SRC_SUPLA_AT_CHANNEL_H_

#include <stdint.h>
#include <supla/channels/channel.h>

namespace Supla {

class AtChannel : public Channel {
 public:
  void sendUpdate() override;
  void pushAction(uint32_t action);
  void activateAction(uint32_t action);
  uint32_t popAction();
  void setRelatedChannel(uint8_t channelNumber);
  void setDisablesLocalOperation(uint32_t actions);
  void enableValueUpdates();

 protected:
  void setActionTriggerProperties(const TActionTriggerProperties &properties);

  uint32_t actionToSend = 0;
  bool valueUpdatePending = false;
  bool valueUpdatesEnabled = false;
};

};  // namespace Supla

#endif  // SRC_SUPLA_AT_CHANNEL_H_

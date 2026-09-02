// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_CHANNEL_ELEMENT_H_
#define SRC_SUPLA_CHANNEL_ELEMENT_H_

#include <supla/channels/channel.h>

#include <new>

#include "element_with_channel_actions.h"

namespace Supla {

class ChannelElement : public ElementWithChannelActions {
 public:
  explicit ChannelElement(int channelNumber = -1);
  ~ChannelElement() override;
  Channel *getChannel() override;
  const Channel *getChannel() const override;

 protected:
  ChannelElement(Channel &externalChannel, ElementMode mode);

  alignas(Channel) unsigned char ownedChannelStorage[sizeof(Channel)] = {};
  Channel &channel;
  bool ownsChannel = false;
};

};  // namespace Supla

#endif  // SRC_SUPLA_CHANNEL_ELEMENT_H_

// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "channel_element.h"

#include <supla/channels/channel.h>

Supla::ChannelElement::ChannelElement(int channelNumber)
    : ElementWithChannelActions(Supla::ElementMode::Registered),
      channel(*new (ownedChannelStorage) Supla::Channel(channelNumber)),
      ownsChannel(true) {
}

Supla::ChannelElement::ChannelElement(Channel &externalChannel,
                                      ElementMode mode)
    : ElementWithChannelActions(mode), channel(externalChannel) {
}

Supla::ChannelElement::~ChannelElement() {
  if (ownsChannel) {
    channel.~Channel();
  }
}

Supla::Channel *Supla::ChannelElement::getChannel() {
  return &channel;
}

const Supla::Channel *Supla::ChannelElement::getChannel() const {
  return &channel;
}

// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_CHANNELS_CHANNEL_EXTENDED_H_
#define SRC_SUPLA_CHANNELS_CHANNEL_EXTENDED_H_

#include "channel.h"

#include <supla-common/proto.h>

namespace Supla {
class ChannelExtended : public Channel {
 public:
  bool isExtended() const override;
  TSuplaChannelExtendedValue *getExtValue() override;

 protected:
  TSuplaChannelExtendedValue extValue = {};
};

};  // namespace Supla

#endif  // SRC_SUPLA_CHANNELS_CHANNEL_EXTENDED_H_

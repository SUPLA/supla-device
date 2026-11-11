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
  bool setNewTextValue(const char *text);
  bool refreshTextValue();

 protected:
  TSuplaChannelExtendedValue extValue = {};

 private:
  uint64_t textSequence = 0;
};

};  // namespace Supla

#endif  // SRC_SUPLA_CHANNELS_CHANNEL_EXTENDED_H_

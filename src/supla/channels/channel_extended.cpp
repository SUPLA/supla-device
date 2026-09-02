// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "channel_extended.h"
#include <supla-common/proto.h>

namespace Supla {
bool ChannelExtended::isExtended() const {
  return true;
}

TSuplaChannelExtendedValue *ChannelExtended::getExtValue() {
  return &extValue;
}

};  // namespace Supla

// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "channel_reference.h"

#include <supla/channels/channel.h>
#ifndef ARDUINO_ARCH_AVR
#include <supla/suplan/suplan_server_identity.h>
#endif

namespace Supla {
bool ChannelReference::valid() const {
  switch (kind) {
    case ChannelReferenceKind::NONE:
      return id == 0;
    case ChannelReferenceKind::LOCAL_CHANNEL_NUMBER:
      return id < 255;
    case ChannelReferenceKind::SERVER_CHANNEL:
      return id > 0 && id <= INT32_MAX;
  }
  return false;
}

ChannelResolution resolveChannelReference(
    const ChannelReference &reference, const Device::ServerIdentity *identity) {
  ChannelResolution result;
  if (!reference.valid()) return result;
  if (reference.kind == ChannelReferenceKind::LOCAL_CHANNEL_NUMBER) {
    result.channel = Channel::GetByChannelNumber(reference.id);
    if (result.channel) result.kind = ChannelResolutionKind::kLocal;
    return result;
  }
#ifndef ARDUINO_ARCH_AVR
  if (reference.kind == ChannelReferenceKind::SERVER_CHANNEL && identity) {
    const auto resolved = identity->resolve(reference.id);
    if (resolved.location == Device::ServerChannelLocation::kLocal) {
      result.channel = Channel::GetByChannelNumber(resolved.channelNumber);
      if (result.channel) result.kind = ChannelResolutionKind::kLocal;
    } else if (resolved.location == Device::ServerChannelLocation::kRemote) {
      result.kind = ChannelResolutionKind::kRemote;
      result.resourceId = reference.id;
    }
  }
#else
  (void)identity;
#endif
  return result;
}
}  // namespace Supla

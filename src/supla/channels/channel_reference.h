// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_CHANNELS_CHANNEL_REFERENCE_H_
#define SRC_SUPLA_CHANNELS_CHANNEL_REFERENCE_H_

#include <stdint.h>

namespace Supla {
class Channel;
namespace Device {
class ServerIdentity;
}

enum class ChannelReferenceKind : uint8_t {
  NONE = 0,
  LOCAL_CHANNEL_NUMBER = 1,
  SERVER_CHANNEL = 2,
};

// Configuration identity only. Runtime credentials and values live elsewhere.
struct ChannelReference {
  ChannelReference(uint32_t value = 0,
                   ChannelReferenceKind tag = ChannelReferenceKind::NONE)
      : id(value), kind(tag) {}
  uint32_t id;
  ChannelReferenceKind kind;

  static ChannelReference local(uint8_t number) {
    return {number, ChannelReferenceKind::LOCAL_CHANNEL_NUMBER};
  }
  static ChannelReference server(uint32_t channelId) {
    return {channelId, ChannelReferenceKind::SERVER_CHANNEL};
  }
  bool valid() const;
  bool operator==(const ChannelReference &other) const {
    return kind == other.kind && id == other.id;
  }
  bool operator!=(const ChannelReference &other) const {
    return !(*this == other);
  }
};

enum class ChannelResolutionKind : uint8_t { kUnresolved, kLocal, kRemote };
struct ChannelResolution {
  ChannelResolutionKind kind = ChannelResolutionKind::kUnresolved;
  Channel *channel = nullptr;
  uint32_t resourceId = 0;
};

ChannelResolution resolveChannelReference(
    const ChannelReference &reference,
    const Device::ServerIdentity *identity = nullptr);

}  // namespace Supla
#endif  // SRC_SUPLA_CHANNELS_CHANNEL_REFERENCE_H_

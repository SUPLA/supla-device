// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_CHANNELS_CHANNEL_REFERENCE_H_
#define SRC_SUPLA_CHANNELS_CHANNEL_REFERENCE_H_

#include <stdint.h>

namespace Supla {
class Channel;
class ChannelState;
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

// Fixed private storage encoding, independent of host ABI padding.
#pragma pack(push, 1)
struct StoredChannelReference {
  uint8_t kind = 0;
  uint32_t id = 0;
  ChannelReference reference() const {
    return {id, static_cast<ChannelReferenceKind>(kind)};
  }
  void assign(ChannelReference ref) {
    kind = static_cast<uint8_t>(ref.kind);
    id = ref.id;
  }
};
#pragma pack(pop)
static_assert(sizeof(StoredChannelReference) == 5,
              "Stored references must be exactly five bytes");

const Device::ServerIdentity *acceptedReferenceIdentity();
ChannelState consumeChannelState(ChannelReference reference, uint32_t consumer);
void releaseChannelConsumer(uint32_t consumer);
int localReferenceNumber(ChannelReference reference);
bool referenceWireId(ChannelReference reference, uint32_t *id);
bool usesServerReferences();

ChannelResolution resolveChannelReference(
    const ChannelReference &reference,
    const Device::ServerIdentity *identity = nullptr);

}  // namespace Supla
#endif  // SRC_SUPLA_CHANNELS_CHANNEL_REFERENCE_H_

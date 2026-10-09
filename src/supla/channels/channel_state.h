// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_CHANNELS_CHANNEL_STATE_H_
#define SRC_SUPLA_CHANNELS_CHANNEL_STATE_H_

#include <stdint.h>
#include <supla-common/proto.h>

namespace Supla {
class Channel;

enum class ChannelCapability : uint8_t {
  Temperature, BinaryState, FloodDetection, ContainerLevel, HvacMaster,
  HvacDemand
};

// One side-effect-free, Source-owned materialization boundary.
bool buildChannelSnapshot(Channel *channel, TDS_SuplaDeviceChannel_E *output);

// A borrowed read-only view. Never registers a Channel or owns its lifecycle.
class ChannelState {
 public:
  explicit ChannelState(Channel *local = nullptr) : local_(local) {}
  ChannelState(const TDS_SuplaDeviceChannel_E *snapshot, bool usable,
               uint32_t receivedMs = 0)
      : snapshot_(snapshot), usable_(usable), receivedMs_(receivedMs) {}
  bool available() const;
  bool availableFor(ChannelCapability capability) const;
  bool binary(bool *result) const;
  bool hvac(THVACValue *result) const;
  uint32_t receivedMs() const { return receivedMs_; }
  bool offline() const;
  uint32_t type() const;
  uint32_t function() const;
  uint32_t validityTimeSec() const;
  bool value(uint8_t output[8]) const;
  double temperature() const;

 private:
  Channel *local_ = nullptr;
  const TDS_SuplaDeviceChannel_E *snapshot_ = nullptr;
  bool usable_ = false;
  uint32_t receivedMs_ = 0;
};
}  // namespace Supla
#endif  // SRC_SUPLA_CHANNELS_CHANNEL_STATE_H_

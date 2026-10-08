// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_CHANNELS_CHANNEL_STATE_H_
#define SRC_SUPLA_CHANNELS_CHANNEL_STATE_H_

#include <stdint.h>
#include <supla-common/proto.h>

namespace Supla {
class Channel;

// One side-effect-free, Source-owned materialization boundary.
bool buildChannelSnapshot(Channel *channel, TDS_SuplaDeviceChannel_E *output);

// A borrowed read-only view. Never registers a Channel or owns its lifecycle.
class ChannelState {
 public:
  explicit ChannelState(Channel *local = nullptr) : local_(local) {}
  ChannelState(const TDS_SuplaDeviceChannel_E *snapshot, bool usable)
      : snapshot_(snapshot), usable_(usable) {}
  bool available() const;
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
};
}  // namespace Supla
#endif  // SRC_SUPLA_CHANNELS_CHANNEL_STATE_H_

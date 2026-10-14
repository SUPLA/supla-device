// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_CHANNELS_CHANNEL_STATE_H_
#define SRC_SUPLA_CHANNELS_CHANNEL_STATE_H_

#include <stdint.h>
#include <supla-common/proto.h>

namespace Supla {
class Channel;
namespace Device { class RemoteChannelManager; }

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
               uint32_t receivedMs = 0, const uint32_t *generation = nullptr)
      : snapshot_(snapshot), usable_(usable), receivedMs_(receivedMs)
#ifndef ARDUINO_ARCH_AVR
        , generation_(generation), expectedGeneration_(generation ? *generation
                                                                 : 0)
#endif
  {
#ifdef ARDUINO_ARCH_AVR
    (void)generation;
#endif
  }
#ifndef ARDUINO_ARCH_AVR
  // Dynamic slots use the manager's stable lifetime, never a pointer into a
  // freed cache node. The manager must outlive this borrowed view.
  ChannelState(const TDS_SuplaDeviceChannel_E *snapshot, bool usable,
               uint32_t receivedMs,
               const Device::RemoteChannelManager *manager, uint32_t generation)
      : snapshot_(snapshot), usable_(usable), managerGuard_(true),
        receivedMs_(receivedMs), manager_(manager),
        expectedGeneration_(generation) {}
#endif
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
  double numeric(bool secondary, bool *valid) const;
  double getValueDouble() const;
  double getValueDoubleFirst() const { return getValueDouble(); }
  double getValueDoubleSecond() const;
  bool getValueInt64(uint64_t *result) const;
  bool getValueBool(bool *result) const;

 private:
  Channel *local_ = nullptr;
  const TDS_SuplaDeviceChannel_E *snapshot_ = nullptr;
  bool usable_ = false;
#ifndef ARDUINO_ARCH_AVR
  bool managerGuard_ = false;
#endif
  uint32_t receivedMs_ = 0;
#ifndef ARDUINO_ARCH_AVR
  union {
    const uint32_t *generation_ = nullptr;
    const Device::RemoteChannelManager *manager_;
  };
  uint32_t expectedGeneration_ = 0;
#endif
  bool current() const;
};
class ChannelStateProvider {
 public:
  virtual ~ChannelStateProvider() = default;
  virtual ChannelState state() = 0;
  virtual Channel *localChannel() = 0;
};
}  // namespace Supla
#endif  // SRC_SUPLA_CHANNELS_CHANNEL_STATE_H_

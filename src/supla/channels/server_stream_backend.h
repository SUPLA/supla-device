// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef SRC_SUPLA_CHANNELS_SERVER_STREAM_BACKEND_H_
#define SRC_SUPLA_CHANNELS_SERVER_STREAM_BACKEND_H_
#ifndef ARDUINO_ARCH_AVR
#include <supla/channels/remote_channel_manager.h>

#ifndef SUPLA_SERVER_STREAM_MAX_CHANNELS
#define SUPLA_SERVER_STREAM_MAX_CHANNELS 32
#endif
#ifndef SUPLA_SERVER_STREAM_MAX_CONSUMERS
#define SUPLA_SERVER_STREAM_MAX_CONSUMERS (2 * SUPLA_SERVER_STREAM_MAX_CHANNELS)
#endif
static_assert(SUPLA_SERVER_STREAM_MAX_CHANNELS > 0 &&
              SUPLA_SERVER_STREAM_MAX_CHANNELS <= 256,
              "SERVER_STREAM supports 1..256 channels");
static_assert(SUPLA_SERVER_STREAM_MAX_CONSUMERS > 0 &&
              SUPLA_SERVER_STREAM_MAX_CONSUMERS <= 65535,
              "SERVER_STREAM consumer bound must fit uint16");
namespace Supla {
namespace Protocol { class SuplaSrpc; }
namespace Device {
// SRPC owns this optional adapter; the shared manager owns all demand logic.
class ServerStreamBackend : public RemoteChannelBackend {
 public:
  ServerStreamBackend(RemoteChannelManager *manager, Protocol::SuplaSrpc *srpc);
  ~ServerStreamBackend() override;
  bool stableStorage() const override { return false; }
  int capacity() const override { return SUPLA_SERVER_STREAM_MAX_CHANNELS; }
  int consumerCapacity() const override {
    return SUPLA_SERVER_STREAM_MAX_CONSUMERS;
  }
  ConsumeIntent intentAt(int index) override;
  RemoteChannelResource &resourceAt(int index) override;
  bool allocateResource(int index) override;
  void clearResource(int index) override;
  bool storeIntent(int index, const ConsumeIntent &intent) override;
  void detached() override { manager_ = nullptr; }
  void reconcileResource(RemoteChannelResource &resource) override;
  void releaseResource(RemoteChannelResource &) override {}
  void cancelControls(uint32_t) override {}
  void refreshResource(RemoteChannelResource &, uint32_t) override {}
  bool fresh(const RemoteChannelResource &, uint32_t) const override {
    return true;  // Only Server publishes expiry; there is no local TTL.
  }
  bool reachable(const RemoteChannelResource &) const override;
  void ensureStarted(RemoteChannelResource &resource) override;
  void ensureCompleted(RemoteChannelResource &resource,
      const TSD_SuplaEnsureResourceAccessResult &result) override;
  void disconnected();
  bool receive(const TSD_SuplaRemoteChannelState &state, uint32_t nowMs);
  size_t allocatedBytes() const;
  bool attached() const { return manager_ != nullptr; }
  bool attachedTo(RemoteChannelManager *manager) const {
    return manager_ == manager;
  }

 private:
  enum class Interest : uint8_t { None, Requested, Pending, Granted, Rejected };
  struct Resource : RemoteChannelResource {
    Interest interest = Interest::None;
  };
  struct Slots {
    Resource *resources[SUPLA_SERVER_STREAM_MAX_CHANNELS] = {};
    ChannelConsumer *intents[SUPLA_SERVER_STREAM_MAX_CONSUMERS] = {};
  };
  bool allocateSlots();
  void trimSlots();
  void invalidate(Resource *resource);
  RemoteChannelManager *manager_;
  Protocol::SuplaSrpc *srpc_;
  Slots *slots_ = nullptr;
  // Empty read-only lookup results; never an admitted entry or snapshot.
  RemoteChannelResource emptyResource_;
};
}  // namespace Device
}  // namespace Supla
#endif
#endif  // SRC_SUPLA_CHANNELS_SERVER_STREAM_BACKEND_H_

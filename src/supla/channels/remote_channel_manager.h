// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef SRC_SUPLA_CHANNELS_REMOTE_CHANNEL_MANAGER_H_
#define SRC_SUPLA_CHANNELS_REMOTE_CHANNEL_MANAGER_H_
#ifndef ARDUINO_ARCH_AVR

#include <supla/channels/channel_state.h>
#include <supla/suplan/remote_access_port.h>
#include <suplan/suplan_types.h>

namespace Supla {
enum class RemoteDelivery : uint8_t {
  SupLanPeer = SUPLA_RESOURCE_DELIVERY_SUPLAN_PEER,
  ServerStream = SUPLA_RESOURCE_DELIVERY_SERVER_STREAM
};
struct RemoteChannelKey {
  uint32_t channelId;
  RemoteDelivery delivery;
  bool operator==(const RemoteChannelKey &other) const {
    return channelId == other.channelId && delivery == other.delivery;
  }
};
enum class ChannelAccessStatus : uint8_t {
  INVALID, IDENTITY_UNRESOLVED, LOCAL_CHANNEL, RESOURCE_LIMIT, PENDING_APPROVAL,
  REJECTED, PROVISIONING, READY, PEER_UNAVAILABLE
};
namespace Device {
enum class RemoteAccess : uint8_t { PENDING, READY, BLOCKED };
struct ConsumeIntent {
  uint32_t consumerId;
  SupLan::ResourceId resource;
  uint8_t permissions;
  bool approvalEligible = false;
  RemoteDelivery delivery = RemoteDelivery::SupLanPeer;
};

// Only validated Channel demand is retained. Type and delivery belong to the
// admitting manager/backend; the public request still validates both in full.
// No owner/index indirection: tokens and canonical IDs retain all 32 bits.
struct ChannelConsumer {
  uint32_t consumerId = 0;
  uint32_t channelId = 0;
  uint8_t permissions = 0;
  bool approvalEligible = false;
  ConsumeIntent intent(RemoteDelivery delivery) const {
    return {consumerId, {SupLan::kResourceTypeChannel, channelId},
            permissions, approvalEligible, delivery};
  }
  void assign(const ConsumeIntent &request) {
    consumerId = request.consumerId;
    channelId = request.resource.id;
    permissions = request.permissions;
    approvalEligible = request.approvalEligible;
  }
};
static_assert(sizeof(ChannelConsumer) == 12,
              "Channel consumer storage must retain its measured saving");

// Shared cache/lifetime state. No peer, session, transport timer or credential.
struct RemoteChannelResource {
  SupLan::ResourceId id = {};
  uint32_t receivedMs = 0;
  TDS_SuplaDeviceChannel_E snapshot = {};
  uint32_t generation = 0;
  RemoteChannelResource()
      : access(RemoteAccess::PENDING), permissions(0), manualPermissions(0),
        effective(0), approval(0), approvalPermissions(0), hasSnapshot(false),
        ensureAttempted(false), manualAttempted(false),
        continuityPending(false) {}
  RemoteAccess access : 2;
  uint8_t permissions : 3;
  uint8_t manualPermissions : 3;
  uint8_t effective : 3;
  uint8_t approval : 2;
  uint8_t approvalPermissions : 3;
  uint8_t hasSnapshot : 1;
  uint8_t ensureAttempted : 1;
  uint8_t manualAttempted : 1;
  uint8_t continuityPending : 1;
  bool used() const { return id.id != 0; }
};

// One adapter per transport, never a vtable/allocation per resource.
// Storage belongs to the registered adapter, but all management uses the
// common algorithm below. An absent backend allocates no cache/intent table.
class RemoteChannelBackend {
 public:
  virtual ~RemoteChannelBackend() = default;
  // Fixed D2D slots permit a direct generation guard without a cache lookup.
  // A backend freeing nodes must use the manager guard instead.
  virtual bool stableStorage() const { return true; }
  virtual int capacity() const = 0;
  virtual int consumerCapacity() const { return capacity(); }
  virtual ConsumeIntent intentAt(int index) = 0;
  virtual RemoteChannelResource &resourceAt(int index) = 0;
  virtual void clearResource(int index) = 0;
  virtual bool allocateResource(int index) {
    clearResource(index);
    return true;
  }
  virtual bool storeIntent(int index, const ConsumeIntent &intent) = 0;
  virtual void detached() {}
  virtual void ensureStarted(RemoteChannelResource &) {}
  virtual void ensureCompleted(RemoteChannelResource &,
      const TSD_SuplaEnsureResourceAccessResult &) {}
  virtual void reconcileResource(RemoteChannelResource &resource) = 0;
  virtual void releaseResource(RemoteChannelResource &resource) = 0;
  virtual void cancelControls(uint32_t owner) = 0;
  virtual void refreshResource(RemoteChannelResource &resource,
                               uint32_t nowMs) = 0;
  virtual bool fresh(const RemoteChannelResource &resource,
                      uint32_t nowMs) const = 0;
  virtual bool reachable(const RemoteChannelResource &resource) const = 0;
  virtual bool controlResource(RemoteChannelResource &, const uint8_t *,
                                size_t, uint32_t) { return false; }
  virtual bool actionResource(RemoteChannelResource &, uint32_t,
                               const uint8_t *, size_t, uint32_t) {
    return false;
  }
};

class RemoteChannelManager {
  friend class ServerStreamBackend;
 public:
  explicit RemoteChannelManager(RemoteAccessPort *access) : access_(access) {}
  RemoteChannelManager(const RemoteChannelManager &) = delete;
  RemoteChannelManager &operator=(const RemoteChannelManager &) = delete;
  // Adapters must outlive registration and unregister before destroying slots.
  bool registerBackend(RemoteDelivery delivery, RemoteChannelBackend *backend);
  void unregisterBackend(RemoteDelivery delivery);
  bool supports(RemoteDelivery delivery) const;
  bool consume(const ConsumeIntent &intent);
  void remove(uint32_t consumerId);
  void iterate(uint32_t nowMs);
  ChannelState state(RemoteChannelKey key, uint32_t nowMs);
  RemoteAccess access(RemoteChannelKey key);
  uint32_t continuity(RemoteChannelKey key);
  uint8_t effectivePermissions(RemoteChannelKey key);
  bool transportAvailable(RemoteChannelKey key);
  uint8_t approvalStatus(RemoteChannelKey key, uint8_t permissions = 0);
  ChannelAccessStatus status(RemoteChannelKey key, uint8_t permissions);
  bool control(RemoteChannelKey key, const uint8_t *value, size_t length,
               uint32_t owner = 0);
  bool action(RemoteChannelKey key, uint32_t actionId, const uint8_t *params,
              size_t length, uint32_t owner = 0);
  void ensureResult(const TSD_SuplaEnsureResourceAccessResult &result);
  void serverReconnected();
  void invalidateServerScope();
  // A readiness transition can unblock admission without freeing a pool slot.
  // Do not reset active demand, cache continuity or the pending ENSURE slot.
  void retryDeferredConsumers() {
    if (nextGeneration_ != UINT32_MAX) ++nextGeneration_;
  }
  bool currentGeneration(uint32_t generation) const;
  int consumerCount() const;
  bool hasConsumer(uint32_t consumer) const;
  int resourceCount() const;
  bool resourceExhausted() const { return admissionExhausted_; }
  uint32_t admissionGeneration() const {
    return nextGeneration_ == UINT32_MAX ? UINT32_MAX : nextGeneration_ + 1;
  }

 protected:
  RemoteChannelResource *findResource(RemoteChannelKey key) const;
  void invalidate(RemoteChannelResource *resource);
  bool usable(RemoteChannelKey key, const RemoteChannelResource &resource,
               uint32_t nowMs) const;
  // Called only after the backend's authentication/authorization/source check.
  void acceptSnapshot(RemoteChannelResource *resource, const uint8_t *payload,
                       size_t length, uint32_t nowMs);
  uint32_t nextGeneration_ = 0;

 private:
  RemoteChannelBackend *backend(RemoteDelivery delivery) const;
  void demand(RemoteChannelResource *resource, RemoteDelivery delivery);
  RemoteChannelBackend *backends_[2] = {};
  RemoteAccessPort *access_;
  RemoteChannelKey ensureResource_ = {0, RemoteDelivery::SupLanPeer};
  uint32_t ensureNextMs_ = 0;
  bool admissionExhausted_ = false;
  bool ensurePending_ = false;
  uint8_t ensurePermissions_ = 0;
};
static_assert(sizeof(RemoteChannelResource) <= 56,
              "Common snapshot state exceeded its extraction baseline");
static_assert(sizeof(RemoteChannelManager) <= (sizeof(void *) == 8 ? 48 : 32),
              "Common coordinator must not own an extra resource cache");
}  // namespace Device
}  // namespace Supla
#endif
#endif  // SRC_SUPLA_CHANNELS_REMOTE_CHANNEL_MANAGER_H_

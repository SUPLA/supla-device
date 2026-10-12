// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef SRC_SUPLA_SUPLAN_REMOTE_RESOURCE_MANAGER_H_
#define SRC_SUPLA_SUPLAN_REMOTE_RESOURCE_MANAGER_H_

#ifndef ARDUINO_ARCH_AVR
#include <supla/channels/channel_state.h>
#include <supla/suplan/remote_access_port.h>
#include <suplan/suplan_runtime.h>

namespace Supla {
namespace Device {
class ServerAssociations;
class ResourceBindingManager;
enum class RemoteAccess : uint8_t { PENDING, READY, BLOCKED };
struct ConsumeIntent {
  uint32_t consumerId;
  SupLan::ResourceId resource;
  uint8_t permissions;
  bool approvalEligible = false;
};

class RemoteResourceManager {
 public:
  static const int kCapacity = SUPLAN_MAX_READ_DEPENDENCIES;
  struct Presence {
    uint32_t awakeUntilMs_ = 0;
    uint32_t nextPresenceMs_ = 0;
    uint16_t awakeWindowMs_ = 0;
    uint8_t presenceCursor_ = 0;
    bool resetPending_ = false;
    bool awakePending_ = false;
  };
  Presence &presence() { return presence_; }
  ServerAssociations *associations = nullptr;
  ResourceBindingManager *bindings = nullptr;
  RemoteResourceManager(SupLan::PeerTable *peers, SupLan::Runtime *runtime,
                        RemoteAccessPort *access);
  void attachRuntime(SupLan::Runtime *runtime) { runtime_ = runtime; }
  bool consume(const ConsumeIntent &intent);
  void remove(uint32_t consumerId);
  void iterate(uint32_t nowMs);
  ChannelState state(const SupLan::ResourceId &resource, uint32_t nowMs);
  RemoteAccess access(const SupLan::ResourceId &resource);
  uint32_t continuity(const SupLan::ResourceId &resource);
  uint8_t effectivePermissions(const SupLan::ResourceId &resource);
  bool peerAvailable(const SupLan::ResourceId &resource);
  uint8_t approvalStatus(const SupLan::ResourceId &resource,
                         uint8_t permissions = 0);
  bool control(const SupLan::ResourceId &resource, const uint8_t *value,
               size_t length, uint32_t owner = 0);
  bool action(const SupLan::ResourceId &resource, uint32_t actionId,
              const uint8_t *params, size_t length, uint32_t owner = 0);
  void receive(uint8_t peerIndex, const SupLan::ResourceId &resource,
               const uint8_t *payload, size_t length, uint32_t nowMs);
  void ensureResult(const TSD_SuplaEnsureResourceAccessResult &result);
  void serverReconnected();
  void authorizationChanged();
  void sessionChanged(uint8_t peerIndex);
  bool allowPresence(uint8_t peerIndex, uint32_t nowMs);
  void peerAwake(uint8_t peerIndex, uint16_t windowMs, bool reset,
                 const uint8_t nonce[16], uint32_t nowMs);
  int consumerCount() const;
  bool hasConsumer(uint32_t consumer) const;
  int resourceCount() const;
  bool resourceExhausted() const { return admissionExhausted_; }
  uint32_t admissionGeneration() const {
    return nextGeneration_ == UINT32_MAX ? UINT32_MAX : nextGeneration_ + 1;
  }

 private:
  struct Resource {
    SupLan::ResourceId id = {};
    uint32_t receivedMs = 0;
    uint32_t lastReadMs = 0;
    uint32_t wakeUntilMs = 0;
    uint32_t hintEpochMs = 0;
    TDS_SuplaDeviceChannel_E snapshot = {};
    uint8_t locator[16] = {};
    uint8_t lastHintNonce[16] = {};
    uint32_t generation = 0;
    uint8_t peerIndex = 0xff;
    Resource() : access(RemoteAccess::PENDING), permissions(0),
                 manualPermissions(0), effective(0), approval(0),
                 approvalPermissions(0), hintCount(0), hasSnapshot(false),
                 ensureAttempted(false),
                 manualAttempted(false), readAttempted(false),
                 hasHintNonce(false), continuityPending(false) {}
    RemoteAccess access : 2;
    uint8_t permissions : 3;
    uint8_t manualPermissions : 3;
    uint8_t effective : 3;
    uint8_t approval : 2;
    uint8_t approvalPermissions : 3;
    uint8_t hintCount : 2;
    uint8_t hasSnapshot : 1;
    uint8_t ensureAttempted : 1;
    uint8_t manualAttempted : 1;
    uint8_t readAttempted : 1;
    uint8_t hasHintNonce : 1;
    uint8_t continuityPending : 1;
    bool used() const { return id.id != 0; }
  };
  Resource *find(const SupLan::ResourceId &resource);
  int readyPeer(const Resource &resource) const;
  void demand(Resource *resource);
  void invalidate(Resource *resource);
  void reconcile(Resource *resource);
  bool usable(const Resource &resource, uint32_t nowMs) const;
  Presence presence_;
  SupLan::PeerTable *peers_;
  SupLan::Runtime *runtime_;
  RemoteAccessPort *access_;
  ConsumeIntent intents_[kCapacity] = {};
  Resource resources_[kCapacity];
  uint32_t nextGeneration_ = 0;
  bool admissionExhausted_ = false;
  bool ensurePending_ = false;
  uint8_t ensurePermissions_ = 0;
  SupLan::ResourceId ensureResource_ = {};
  // Response deadline while pending; enqueue/retry cooldown otherwise.
  uint32_t ensureNextMs_ = 0;
};
static_assert(sizeof(RemoteResourceManager) <=
                  64 + 124 * RemoteResourceManager::kCapacity,
              "Remote resources exceed their bounded workspace");
}  // namespace Device
}  // namespace Supla
#endif  // !ARDUINO_ARCH_AVR
#endif  // SRC_SUPLA_SUPLAN_REMOTE_RESOURCE_MANAGER_H_

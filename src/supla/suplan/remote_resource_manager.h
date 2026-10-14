// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef SRC_SUPLA_SUPLAN_REMOTE_RESOURCE_MANAGER_H_
#define SRC_SUPLA_SUPLAN_REMOTE_RESOURCE_MANAGER_H_
#ifndef ARDUINO_ARCH_AVR
#include <supla/channels/remote_channel_manager.h>
#include <suplan/suplan_runtime.h>
namespace Supla {
namespace Device {
class ServerAssociations;
class ResourceBindingManager;
// Existing name/API retained for component integrations. Common management is
// inherited; only the SupLAN backend and its bounded storage live here.
class RemoteResourceManager : public RemoteChannelManager,
                              private RemoteChannelBackend {
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
  ~RemoteResourceManager() override;
  void attachRuntime(SupLan::Runtime *runtime) { runtime_ = runtime; }
  void receive(uint8_t peerIndex, const SupLan::ResourceId &resource,
               const uint8_t *payload, size_t length, uint32_t nowMs);
  void authorizationChanged();
  void sessionChanged(uint8_t peerIndex);
  bool allowPresence(uint8_t peerIndex, uint32_t nowMs);
  void peerAwake(uint8_t peerIndex, uint16_t windowMs, bool reset,
                 const uint8_t nonce[16], uint32_t nowMs);

  // Existing D2D component API remains source-compatible and explicitly peer.
  ChannelState state(const SupLan::ResourceId &id, uint32_t now) {
    return RemoteChannelManager::state(peerKey(id), now);
  }
  RemoteAccess access(const SupLan::ResourceId &id) {
    return RemoteChannelManager::access(peerKey(id));
  }
  uint32_t continuity(const SupLan::ResourceId &id) {
    return RemoteChannelManager::continuity(peerKey(id));
  }
  uint8_t effectivePermissions(const SupLan::ResourceId &id) {
    return RemoteChannelManager::effectivePermissions(peerKey(id));
  }
  bool peerAvailable(const SupLan::ResourceId &id) {
    return transportAvailable(peerKey(id));
  }
  uint8_t approvalStatus(const SupLan::ResourceId &id,
                         uint8_t permissions = 0) {
    return RemoteChannelManager::approvalStatus(peerKey(id), permissions);
  }
  bool control(const SupLan::ResourceId &id, const uint8_t *value, size_t size,
               uint32_t owner = 0) {
    return RemoteChannelManager::control(peerKey(id), value, size, owner);
  }
  bool action(const SupLan::ResourceId &id, uint32_t actionId,
              const uint8_t *params, size_t size, uint32_t owner = 0) {
    return RemoteChannelManager::action(peerKey(id), actionId, params, size,
                                         owner);
  }

 private:
  static RemoteChannelKey peerKey(const SupLan::ResourceId &id) {
    return {id.type == SupLan::kResourceTypeChannel ? id.id : 0,
            RemoteDelivery::SupLanPeer};
  }
  struct Resource : RemoteChannelResource {
    uint32_t lastReadMs = 0;
    uint32_t wakeUntilMs = 0;
    uint32_t hintEpochMs = 0;
    uint8_t locator[16] = {};
    uint8_t lastHintNonce[16] = {};
    uint8_t peerIndex = 0xff;
    Resource() : hintCount(0), readAttempted(false), hasHintNonce(false) {}
    uint8_t hintCount : 2;
    uint8_t readAttempted : 1;
    uint8_t hasHintNonce : 1;
  };
  Resource *find(const SupLan::ResourceId &resource);
  int readyPeer(const Resource &resource) const;
  void reconcile(Resource *resource);
  bool usable(const Resource &resource, uint32_t nowMs) const {
    return RemoteChannelManager::usable(
        {resource.id.id, RemoteDelivery::SupLanPeer}, resource, nowMs);
  }
  int capacity() const override { return kCapacity; }
  ConsumeIntent &intentAt(int index) override { return intents_[index]; }
  RemoteChannelResource &resourceAt(int index) override {
    return resources_[index];
  }
  void clearResource(int index) override { resources_[index] = {}; }
  void reconcileResource(RemoteChannelResource &resource) override {
    reconcile(static_cast<Resource *>(&resource));
  }
  void releaseResource(RemoteChannelResource &resource) override;
  void cancelControls(uint32_t owner) override;
  void refreshResource(RemoteChannelResource &resource,
                        uint32_t nowMs) override;
  bool fresh(const RemoteChannelResource &resource,
              uint32_t nowMs) const override;
  bool reachable(const RemoteChannelResource &resource) const override;
  bool controlResource(RemoteChannelResource &resource, const uint8_t *value,
                        size_t length, uint32_t owner) override;
  bool actionResource(RemoteChannelResource &resource, uint32_t actionId,
                       const uint8_t *params, size_t length,
                       uint32_t owner) override;
  Presence presence_;
  SupLan::PeerTable *peers_;
  SupLan::Runtime *runtime_;
  ConsumeIntent intents_[kCapacity] = {};
  Resource resources_[kCapacity];
};
static_assert(sizeof(RemoteResourceManager) <=
                  64 + 124 * RemoteResourceManager::kCapacity,
              "Remote resources exceed their bounded workspace");
}  // namespace Device
}  // namespace Supla
#endif  // !ARDUINO_ARCH_AVR
#endif  // SRC_SUPLA_SUPLAN_REMOTE_RESOURCE_MANAGER_H_

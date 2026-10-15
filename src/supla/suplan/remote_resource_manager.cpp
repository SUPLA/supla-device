// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later
#include "remote_resource_manager.h"
#ifndef ARDUINO_ARCH_AVR
#include <string.h>
namespace Supla {
namespace Device {
RemoteResourceManager::RemoteResourceManager(SupLan::PeerTable *peers,
    SupLan::Runtime *runtime, RemoteAccessPort *access)
    : RemoteChannelManager(access), peers_(peers), runtime_(runtime) {
  registerBackend(RemoteDelivery::SupLanPeer, this);
}
RemoteResourceManager::~RemoteResourceManager() {
  unregisterBackend(RemoteDelivery::ServerStream);
  unregisterBackend(RemoteDelivery::SupLanPeer);
}
RemoteResourceManager::Resource *RemoteResourceManager::find(
    const SupLan::ResourceId &id) {
  if (id.type != SupLan::kResourceTypeChannel) return nullptr;
  return static_cast<Resource *>(
      findResource({id.id, RemoteDelivery::SupLanPeer}));
}
int RemoteResourceManager::readyPeer(const Resource &resource) const {
  if (!peers_) return -1;
  for (int i = 0; i < SUPLAN_MAX_PERSISTENT_PEERS; ++i) {
    const auto *peer = peers_->get(i);
    if (peer && peer->destination &&
        peer->contextBytes[0] == SupLan::kAuthorityServer &&
        peers_->runtimeEligible(i)) {
      for (uint8_t right = 1; right <= 4; right <<= 1) {
        if (peers_->authorize(i, resource.id, right)) return i;
      }
    }
  }
  return -1;
}

RemoteResourceManager::PeerPresence *RemoteResourceManager::peerPresence(
    uint8_t index) {
  if (!peers_ || index >= SUPLAN_MAX_PERSISTENT_PEERS) return nullptr;
  const auto peer = peers_->get(index);
  const bool eligible = peer && peer->destination &&
      peer->contextBytes[0] == SupLan::kAuthorityServer &&
      peers_->runtimeEligible(index);
  return reconcilePeer(index, eligible ? peer : nullptr);
}

RemoteResourceManager::PeerPresence *RemoteResourceManager::reconcilePeer(
    uint8_t index, const SupLan::PeerRecord *peer) {
  // The caller has already checked this slot's SERVER Destination ownership.
  // No callbacks or PeerTable mutation occur between that check and this call.
  auto &state = peerPresence_[index];
  if (peer && state.bound &&
      memcmp(state.locator, peer->peerLocator, 16) == 0) return &state;
  if (state.bound) {
    // Synchronous invalidation replaces the old per-resource locator check.
    // Also fence other borrowed views, not just the resource being reconciled.
    // Its transport cleanup only queries the production read-only
    // bindingSessionNeeded hook; it cannot reenter resource admission.
    for (auto &resource : resources_) {
      if (!resource.used() || resource.peerIndex != index) continue;
      if (runtime_) runtime_->removeReadDependency(index, resource.id);
      invalidate(&resource);
      resource.peerIndex = 0xff;
      resource.effective = 0;
      resource.readAttempted = false;
      resource.ensureAttempted = false;
      resource.access = RemoteAccess::PENDING;
    }
    state = {};
  }
  if (!peer) return nullptr;
  memcpy(state.locator, peer->peerLocator, 16);
  state.bound = true;
  return &state;
}

void RemoteResourceManager::reconcile(Resource *resource) {
  const int peerIndex = readyPeer(*resource);
  if (resource->peerIndex != 0xff && resource->peerIndex != peerIndex)
    peerPresence(resource->peerIndex);
  if (peerIndex < 0) {
    if (resource->effective || resource->peerIndex != 0xff) {
      if (runtime_ && resource->peerIndex != 0xff)
        runtime_->removeReadDependency(resource->peerIndex, resource->id);
      invalidate(resource);
      resource->readAttempted = false;
      resource->ensureAttempted = false;
      resource->access = RemoteAccess::PENDING;
    }
    resource->peerIndex = 0xff;
    resource->effective = 0;
    return;
  }
  // readyPeer has validated role/authority/ownership and precise rights.
  reconcilePeer(peerIndex, peers_->get(peerIndex));
  if (resource->peerIndex != peerIndex) {
    if (runtime_ && resource->peerIndex != 0xff)
      runtime_->removeReadDependency(resource->peerIndex, resource->id);
    invalidate(resource);
    resource->readAttempted = false;
  }
  const uint8_t previous = resource->effective;
  resource->effective = 0;
  for (uint8_t right = 1; right <= 4; right <<= 1) {
    if (peers_->authorize(peerIndex, resource->id, right))
      resource->effective |= right;
  }
  if ((previous & SupLan::kPermissionRead) &&
      !(resource->effective & resource->permissions &
        SupLan::kPermissionRead)) {
    invalidate(resource);
    resource->readAttempted = false;
    if (runtime_) runtime_->removeReadDependency(peerIndex, resource->id);
  }
  resource->peerIndex = peerIndex;
  resource->access = (resource->effective & resource->permissions) ==
                            resource->permissions
                        ? RemoteAccess::READY : RemoteAccess::PENDING;
}

bool RemoteResourceManager::fresh(const RemoteChannelResource &resource,
                                   uint32_t nowMs) const {
  const uint32_t ttl = resource.snapshot.ValueValidityTimeSec;
  return ttl == 0 || (nowMs - resource.receivedMs) / 1000 < ttl;
}
void RemoteResourceManager::refreshResource(RemoteChannelResource &entry,
                                           uint32_t nowMs) {
  auto &resource = static_cast<Resource &>(entry);
  if (resource.effective & resource.permissions & SupLan::kPermissionRead) {
    const uint32_t ttl = resource.snapshot.ValueValidityTimeSec;
    const uint32_t refreshMs =
        resource.hasSnapshot && ttl && ttl < 60 ? ttl * 1000 : 60000;
    if (runtime_ && !runtime_->recoveryStatus(resource.peerIndex).sleeping &&
        (!resource.readAttempted ||
         (static_cast<uint32_t>(nowMs - resource.lastReadMs) >= refreshMs &&
          !runtime_->readNeedsRefresh(resource.peerIndex, resource.id)))) {
      if (runtime_->requestRead(resource.peerIndex, resource.id)) {
        auto &peer = peerPresence_[resource.peerIndex];
        const int32_t awakeRemaining = peer.wakeUntilMs - nowMs;
        if (peer.awake && awakeRemaining > 0) {
          runtime_->refreshPeer(resource.peerIndex, awakeRemaining, false);
        } else {
          peer.awake = false;
        }
        resource.lastReadMs = nowMs;
        resource.readAttempted = true;
      }
    }
  }
}
void RemoteResourceManager::releaseResource(RemoteChannelResource &entry) {
  auto &resource = static_cast<Resource &>(entry);
  if (runtime_ && resource.peerIndex != 0xff)
    runtime_->removeReadDependency(resource.peerIndex, resource.id);
}
void RemoteResourceManager::cancelControls(uint32_t owner) {
  if (runtime_) runtime_->cancelOwnedControls(owner);
}
bool RemoteResourceManager::reachable(
    const RemoteChannelResource &entry) const {
  const auto &resource = static_cast<const Resource &>(entry);
  return runtime_ && resource.peerIndex != 0xff &&
      runtime_->hasSession(resource.peerIndex) &&
      !runtime_->recoveryStatus(resource.peerIndex).sleeping;
}
bool RemoteResourceManager::controlResource(RemoteChannelResource &entry,
    const uint8_t *value, size_t length, uint32_t owner) {
  const auto &resource = static_cast<Resource &>(entry);
  return runtime_ && runtime_->sendControl(resource.peerIndex, resource.id,
                                           value, length, owner);
}
bool RemoteResourceManager::actionResource(RemoteChannelResource &entry,
    uint32_t actionId, const uint8_t *params, size_t length, uint32_t owner) {
  const auto &resource = static_cast<Resource &>(entry);
  return runtime_ && runtime_->sendControlAction(
      resource.peerIndex, resource.id, actionId, params, length, owner);
}
void RemoteResourceManager::receive(uint8_t peerIndex,
                                    const SupLan::ResourceId &id,
                                    const uint8_t *payload, size_t length,
                                    uint32_t nowMs) {
  auto *resource = find(id);
  if (!resource || !payload || length != sizeof(resource->snapshot)) return;
  reconcile(resource);
  if (!(resource->effective & resource->permissions &
        SupLan::kPermissionRead) || resource->peerIndex != peerIndex)
    return;
  acceptSnapshot(resource, payload, length, nowMs);
}
void RemoteResourceManager::sessionChanged(uint8_t peerIndex) {
  for (auto &resource : resources_) {
    if (resource.used() && resource.peerIndex == peerIndex &&
        resource.hasSnapshot) {
      resource.continuityPending = true;
      resource.generation = nextGeneration_ == UINT32_MAX
                                ? 0 : ++nextGeneration_;
    }
  }
}
void RemoteResourceManager::authorizationChanged() {
  // Removed peers can have no resources left; retain a same-identity quota
  // through ACL edits, but never through an observed association replacement.
  for (int i = 0; i < SUPLAN_MAX_PERSISTENT_PEERS; ++i)
    if (peerPresence_[i].bound) peerPresence(i);
  for (auto &resource : resources_) {
    if (!resource.used()) continue;
    reconcile(&resource);
  }
}
bool RemoteResourceManager::allowPresence(uint8_t index, uint32_t nowMs) {
  auto peer = peerPresence(index);
  if (!peer) return false;
  for (auto &resource : resources_) {
    if (!resource.used()) continue;
    reconcile(&resource);
    if (!(resource.effective & resource.permissions &
        SupLan::kPermissionRead) ||
        resource.peerIndex != index) {
      continue;
    }
    if (static_cast<uint32_t>(nowMs - peer->hintEpochMs) >= 1000) {
      peer->hintCount = 0;
      peer->hintEpochMs = nowMs;
    }
    return peer->hintCount < 2;
  }
  return false;
}
void RemoteResourceManager::peerAwake(uint8_t index, uint16_t window,
                                      bool reset, const uint8_t nonce[16],
                                      uint32_t nowMs) {
  if (!nonce || (!reset && !window) || !allowPresence(index, nowMs)) return;
  auto &peer = peerPresence_[index];
  if (peer.hasHintNonce && memcmp(nonce, peer.lastHintNonce, 16) == 0) return;
  // Admission/release of a Channel cannot reset this association's quota.
  ++peer.hintCount;
  memcpy(peer.lastHintNonce, nonce, 16);
  peer.hasHintNonce = true;
  if (!reset) {
    peer.wakeUntilMs = nowMs + window;
    peer.awake = true;
  }
  bool refresh = false;
  for (auto &resource : resources_) {
    if (!resource.used()) continue;
    reconcile(&resource);
    if (!(resource.effective & resource.permissions &
        SupLan::kPermissionRead) ||
        resource.peerIndex != index)
      continue;
    if (reset || !usable(resource, nowMs)) {
      resource.readAttempted = false;
      refresh = true;
    }
  }
  if (runtime_) {
    runtime_->refreshPeer(index, window, reset && refresh);
  }
}

}  // namespace Device
}  // namespace Supla
#endif  // !ARDUINO_ARCH_AVR

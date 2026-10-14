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

void RemoteResourceManager::reconcile(Resource *resource) {
  const int peerIndex = readyPeer(*resource);
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
  const auto *peer = peers_->get(peerIndex);
  if (resource->peerIndex != peerIndex ||
      memcmp(resource->locator, peer->peerLocator, 16) != 0) {
    invalidate(resource);
    resource->readAttempted = false;
    memcpy(resource->locator, peer->peerLocator, 16);
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
        const int32_t awakeRemaining = resource.wakeUntilMs - nowMs;
        if (awakeRemaining > 0) {
          runtime_->refreshPeer(resource.peerIndex, awakeRemaining, false);
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
  for (auto &resource : resources_) {
    if (!resource.used()) continue;
    reconcile(&resource);
  }
}
bool RemoteResourceManager::allowPresence(uint8_t index, uint32_t nowMs) {
  for (auto &resource : resources_) {
    if (!resource.used()) continue;
    reconcile(&resource);
    if (!(resource.effective & resource.permissions &
        SupLan::kPermissionRead) ||
        resource.peerIndex != index) {
      continue;
    }
    if (static_cast<uint32_t>(nowMs - resource.hintEpochMs) >= 1000) {
      resource.hintCount = 0;
      resource.hintEpochMs = nowMs;
    }
    if (resource.hintCount >= 2) return false;
    return true;
  }
  return false;
}
void RemoteResourceManager::peerAwake(uint8_t index, uint16_t window,
                                      bool reset, const uint8_t nonce[16],
                                      uint32_t nowMs) {
  bool refresh = false;
  bool accepted = false;
  for (auto &resource : resources_) {
    if (!resource.used()) continue;
    reconcile(&resource);
    if (!(resource.effective & resource.permissions &
        SupLan::kPermissionRead) ||
        resource.peerIndex != index ||
        (resource.hasHintNonce &&
         memcmp(nonce, resource.lastHintNonce, 16) == 0))
      continue;
    if (static_cast<uint32_t>(nowMs - resource.hintEpochMs) >= 1000) {
      resource.hintEpochMs = nowMs;
      resource.hintCount = 0;
    }
    if (resource.hintCount >= 2) continue;
    // Charge only fresh authenticated hints, after nonce deduplication.
    ++resource.hintCount;
    accepted = true;
    if (!reset) resource.wakeUntilMs = nowMs + window;
    memcpy(resource.lastHintNonce, nonce, 16);
    resource.hasHintNonce = true;
    if (reset || !usable(resource, nowMs)) {
      resource.readAttempted = false;
      refresh = true;
    }
  }
  if (accepted && runtime_) {
    runtime_->refreshPeer(index, window, reset && refresh);
  }
}

}  // namespace Device
}  // namespace Supla
#endif  // !ARDUINO_ARCH_AVR

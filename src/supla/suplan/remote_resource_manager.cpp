// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later
#include "remote_resource_manager.h"

#ifndef ARDUINO_ARCH_AVR
#include <string.h>

namespace Supla {
namespace Device {
namespace {
bool sameResource(const SupLan::ResourceId &a, const SupLan::ResourceId &b) {
  return a.type == b.type && a.id == b.id;
}
const uint32_t kEnsureTimeoutMs = 10000;
}  // namespace

RemoteResourceManager::RemoteResourceManager(SupLan::PeerTable *peers,
                                             SupLan::Runtime *runtime,
                                             RemoteAccessPort *access)
    : peers_(peers), runtime_(runtime), access_(access) {}

RemoteResourceManager::Resource *RemoteResourceManager::find(
    const SupLan::ResourceId &resource) {
  for (auto &entry : resources_) {
    if (entry.used && sameResource(entry.id, resource)) return &entry;
  }
  return nullptr;
}

bool RemoteResourceManager::consume(const ConsumeIntent &intent) {
  if (!intent.consumerId ||
      intent.resource.type != SupLan::kResourceTypeChannel ||
      !intent.resource.id || intent.resource.id > INT32_MAX ||
      !intent.permissions || (intent.permissions & ~7))
    return false;
  ConsumeIntent *slot = nullptr;
  for (auto &entry : intents_) {
    if (entry.consumerId == intent.consumerId) {
      if (sameResource(entry.resource, intent.resource) &&
          entry.permissions == intent.permissions)
        return true;
      remove(intent.consumerId);
      slot = &entry;
      break;
    }
    if (!entry.consumerId) slot = &entry;
  }
  if (!slot) return false;
  auto *resource = find(intent.resource);
  if (!resource) {
    for (auto &entry : resources_) {
      if (!entry.used) {
        resource = &entry;
        *resource = {};
        resource->used = true;
        resource->id = intent.resource;
        break;
      }
    }
  }
  if (!resource) return false;
  *slot = intent;
  resource->permissions |= intent.permissions;
  reconcile(resource);
  return true;
}

void RemoteResourceManager::remove(uint32_t consumerId) {
  for (auto &intent : intents_) {
    if (intent.consumerId == consumerId) intent = {};
  }
  for (auto &resource : resources_) {
    if (!resource.used) continue;
    uint8_t permissions = 0;
    for (const auto &intent : intents_) {
      if (intent.consumerId && sameResource(intent.resource, resource.id)) {
        permissions |= intent.permissions;
      }
    }
    if (!permissions) {
      if (ensurePending_ && sameResource(ensureResource_, resource.id)) {
        if (access_) access_->cancelEnsure();
        ensurePending_ = false;
        ensureNextMs_ = 0;
      }
      if (runtime_ && resource.peerIndex != 0xff) {
        runtime_->removeReadDependency(resource.peerIndex, resource.id);
      }
      resource = {};
    } else {
      resource.permissions = permissions;
    }
  }
}

int RemoteResourceManager::readyPeer(const Resource &resource) const {
  if (!peers_) return -1;
  for (int i = 0; i < SUPLAN_MAX_PERSISTENT_PEERS; ++i) {
    const auto *peer = peers_->get(i);
    if (peer && peer->destination &&
        peer->contextBytes[0] == SupLan::kAuthorityServer &&
        peers_->runtimeEligible(i) &&
        peers_->authorize(i, resource.id, resource.permissions))
      return i;
  }
  return -1;
}

void RemoteResourceManager::reconcile(Resource *resource) {
  const int peerIndex = readyPeer(*resource);
  if (peerIndex < 0) {
    if (resource->access == RemoteAccess::READY) {
      resource->hasSnapshot = false;
      resource->readAttempted = false;
      resource->ensureAttempted = false;
      resource->access = RemoteAccess::PENDING;
    }
    resource->peerIndex = 0xff;
    return;
  }
  const auto *peer = peers_->get(peerIndex);
  if (resource->peerIndex != peerIndex ||
      memcmp(resource->locator, peer->peerLocator, 16) != 0) {
    resource->hasSnapshot = false;
    resource->readAttempted = false;
    memcpy(resource->locator, peer->peerLocator, 16);
  }
  resource->peerIndex = peerIndex;
  resource->access = RemoteAccess::READY;
}

bool RemoteResourceManager::usable(const Resource &resource,
                                   uint32_t nowMs) const {
  if (resource.access != RemoteAccess::READY || !resource.hasSnapshot) {
    return false;
  }
  const uint32_t ttl = resource.snapshot.ValueValidityTimeSec;
  return ttl == 0 || (nowMs - resource.receivedMs) / 1000 < ttl;
}

void RemoteResourceManager::iterate(uint32_t nowMs) {
  if (ensurePending_ &&
      static_cast<int32_t>(nowMs - ensureNextMs_) >= 0) {
    if (access_) access_->cancelEnsure();
    ensurePending_ = false;
    if (auto *resource = find(ensureResource_)) {
      resource->ensureAttempted = false;
    }
    ensureNextMs_ = nowMs + 1000;
  }
  // Continue after the last ENSURE so a silent resource cannot starve others.
  const auto *previous = find(ensureResource_);
  const int start = previous ? (previous - resources_ + 1) % kCapacity : 0;
  for (int i = 0; i < kCapacity; ++i) {
    auto &resource = resources_[(start + i) % kCapacity];
    if (!resource.used) continue;
    reconcile(&resource);
    if (resource.access == RemoteAccess::READY) {
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
    } else if (!ensurePending_ && !resource.ensureAttempted && access_ &&
               (!ensureNextMs_ ||
                static_cast<int32_t>(nowMs - ensureNextMs_) >= 0)) {
      TDS_SuplaEnsureResourceAccess request = {};
      request.Resource.ResourceType = resource.id.type;
      request.Resource.ResourceId = resource.id.id;
      request.Permissions = resource.permissions;
      request.DeliveryMode = SUPLA_RESOURCE_DELIVERY_SUPLAN_PEER;
      if (access_->ensure(request)) {
        ensurePending_ = true;
        ensureNextMs_ = nowMs + kEnsureTimeoutMs;
        ensureResource_ = resource.id;
        resource.ensureAttempted = true;
      } else {
        ensureNextMs_ = nowMs + 1000;
      }
    }
  }
}

ChannelState RemoteResourceManager::state(const SupLan::ResourceId &id,
                                          uint32_t nowMs) {
  auto *resource = find(id);
  if (!resource) return ChannelState();
  reconcile(resource);
  return ChannelState(&resource->snapshot, usable(*resource, nowMs));
}
RemoteAccess RemoteResourceManager::access(const SupLan::ResourceId &id) {
  auto *resource = find(id);
  if (!resource) return RemoteAccess::PENDING;
  reconcile(resource);
  return resource->access;
}
void RemoteResourceManager::receive(uint8_t peerIndex,
                                    const SupLan::ResourceId &id,
                                    const uint8_t *payload, size_t length,
                                    uint32_t nowMs) {
  auto *resource = find(id);
  if (!resource || !payload || length != sizeof(resource->snapshot)) return;
  reconcile(resource);
  if (resource->access != RemoteAccess::READY ||
      resource->peerIndex != peerIndex)
    return;
  TDS_SuplaDeviceChannel_E snapshot;
  memcpy(&snapshot, payload, sizeof(snapshot));
  if (snapshot.Number != 0xff) return;
  resource->snapshot = snapshot;
  resource->receivedMs = nowMs;
  resource->hasSnapshot = true;
}
void RemoteResourceManager::ensureResult(
    const TSD_SuplaEnsureResourceAccessResult &result) {
  if (!ensurePending_) return;
  ensurePending_ = false;
  ensureNextMs_ = 0;
  auto *resource = find(ensureResource_);
  if (!resource) return;
  reconcile(resource);
  if (resource->access == RemoteAccess::READY) return;
  if (result.Result != SUPLA_SUPLAN_RESULT_OK ||
      result.AccessStatus != SUPLA_SUPLAN_ACCESS_STATUS_GRANTED ||
      result.DeliveryMode != SUPLA_RESOURCE_DELIVERY_SUPLAN_PEER) {
    resource->access = RemoteAccess::BLOCKED;
  }
}
void RemoteResourceManager::serverReconnected() {
  ensurePending_ = false;
  ensureNextMs_ = 0;
  for (auto &resource : resources_) {
    if (!resource.used) continue;
    resource.ensureAttempted = false;
    if (resource.access == RemoteAccess::BLOCKED) {
      resource.access = RemoteAccess::PENDING;
    }
  }
}
void RemoteResourceManager::authorizationChanged() {
  for (auto &resource : resources_) {
    if (!resource.used) continue;
    reconcile(&resource);
  }
}
int RemoteResourceManager::consumerCount() const {
  int count = 0;
  for (const auto &entry : intents_) count += entry.consumerId != 0;
  return count;
}
int RemoteResourceManager::resourceCount() const {
  int count = 0;
  for (const auto &entry : resources_) count += entry.used;
  return count;
}
bool RemoteResourceManager::allowPresence(uint8_t index, uint32_t nowMs) {
  for (auto &resource : resources_) {
    if (!resource.used) continue;
    reconcile(&resource);
    if (resource.access != RemoteAccess::READY || resource.peerIndex != index) {
      continue;
    }
    if (static_cast<uint32_t>(nowMs - resource.hintEpochMs) >= 1000) {
      resource.hintCount = 0;
      resource.hintEpochMs = nowMs;
    }
    if (resource.hintCount >= 2) return false;
    ++resource.hintCount;
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
    if (!resource.used) continue;
    reconcile(&resource);
    if (resource.access != RemoteAccess::READY || resource.peerIndex != index ||
        (resource.hasHintNonce &&
         memcmp(nonce, resource.lastHintNonce, 16) == 0))
      continue;
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

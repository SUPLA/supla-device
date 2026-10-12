// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later
#include "remote_resource_manager.h"

#ifndef ARDUINO_ARCH_AVR
#include <string.h>
#include <supla/element.h>

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
    if (entry.used() && sameResource(entry.id, resource)) return &entry;
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
          entry.permissions == intent.permissions &&
          entry.approvalEligible == intent.approvalEligible)
        return true;
      if (sameResource(entry.resource, intent.resource)) {
        if (runtime_ && (entry.permissions & SupLan::kPermissionControl) &&
            !(intent.permissions & SupLan::kPermissionControl))
          runtime_->cancelOwnedControls(intent.consumerId);
        entry = intent;
        auto *resource = find(intent.resource);
        if (!resource) return false;
        const uint8_t previous = resource->manualPermissions;
        demand(resource);
        if (previous != resource->manualPermissions) {
          resource->manualAttempted = false;
          resource->approval = SUPLA_SUPLAN_ACCESS_STATUS_INVALID;
        }
        reconcile(resource);
        return true;
      }
      remove(intent.consumerId);
      slot = &entry;
      break;
    }
    if (!entry.consumerId) slot = &entry;
  }
  if (!slot) {
    admissionExhausted_ = true;
    return false;
  }
  auto *resource = find(intent.resource);
  if (!resource) {
    for (auto &entry : resources_) {
      if (!entry.used()) {
        resource = &entry;
        *resource = {};
        if (nextGeneration_ == UINT32_MAX) return false;
        resource->generation = ++nextGeneration_;
        resource->id = intent.resource;
        break;
      }
    }
  }
  if (!resource) {
    admissionExhausted_ = true;
    return false;
  }
  *slot = intent;
  const uint8_t previous = resource->manualPermissions;
  demand(resource);
  if (previous != resource->manualPermissions)
    resource->manualAttempted = false;
  reconcile(resource);
  return true;
}

void RemoteResourceManager::remove(uint32_t consumerId) {
  if (runtime_) runtime_->cancelOwnedControls(consumerId);
  for (auto &intent : intents_) {
    if (intent.consumerId == consumerId) {
      intent = {};
      if (nextGeneration_ != UINT32_MAX) ++nextGeneration_;
    }
  }
  for (auto &resource : resources_) {
    if (!resource.used()) continue;
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
      demand(&resource);
      reconcile(&resource);
    }
  }
}

void RemoteResourceManager::demand(Resource *resource) {
  resource->permissions = 0;
  resource->manualPermissions = 0;
  for (const auto &intent : intents_) {
    if (!intent.consumerId || !sameResource(intent.resource, resource->id))
      continue;
    uint8_t rights = intent.permissions;
    if (rights & SupLan::kPermissionControl) rights |= SupLan::kPermissionRead;
    resource->permissions |= rights;
    if (intent.approvalEligible) resource->manualPermissions |= rights;
  }
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

void RemoteResourceManager::invalidate(Resource *resource) {
  resource->hasSnapshot = false;
  resource->generation = nextGeneration_ == UINT32_MAX
                             ? 0 : ++nextGeneration_;
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

bool RemoteResourceManager::usable(const Resource &resource,
                                   uint32_t nowMs) const {
  if (!(resource.effective & resource.permissions &
        SupLan::kPermissionRead) || !resource.hasSnapshot ||
      resource.snapshot.Default == SUPLA_CHANNELFNC_NONE ||
      !resource.generation) {
    return false;
  }
  const uint32_t ttl = resource.snapshot.ValueValidityTimeSec;
  return ttl == 0 || (nowMs - resource.receivedMs) / 1000 < ttl;
}

void RemoteResourceManager::iterate(uint32_t nowMs) {
  admissionExhausted_ = false;
  // Stable sequential admission; existing allocations are never preempted.
  for (int number = 0; number < 255; ++number) {
    if (auto element = Element::getElementByChannelNumber(number))
      element->reconcileChannelDependencies();
  }
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
    if (!resource.used()) continue;
    reconcile(&resource);
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
    const bool manual = resource.manualPermissions && !resource.manualAttempted;
    const bool missing = (resource.effective & resource.permissions) !=
                         resource.permissions;
    if (!ensurePending_ && (manual || (missing && !resource.ensureAttempted)) &&
        access_ &&
               (!ensureNextMs_ ||
                static_cast<int32_t>(nowMs - ensureNextMs_) >= 0)) {
      TDS_SuplaEnsureResourceAccess request = {};
      request.Resource.ResourceType = resource.id.type;
      request.Resource.ResourceId = resource.id.id;
      request.Permissions = manual ? resource.manualPermissions
                                   : resource.permissions;
      if (manual) request.Flags =
        SUPLA_SUPLAN_ENSURE_ACCESS_FLAG_ALLOW_APPROVAL;
      request.DeliveryMode = SUPLA_RESOURCE_DELIVERY_SUPLAN_PEER;
      if (access_->ensure(request)) {
        ensurePending_ = true;
        ensureNextMs_ = nowMs + kEnsureTimeoutMs;
        ensureResource_ = resource.id;
        ensurePermissions_ = request.Permissions;
        if (manual) resource.manualAttempted = true;
        // Config-only rights are never promoted to manual approval.
        resource.ensureAttempted = !manual ||
            request.Permissions == resource.permissions;
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
  return ChannelState(&resource->snapshot, usable(*resource, nowMs),
                      resource->receivedMs);
}
RemoteAccess RemoteResourceManager::access(const SupLan::ResourceId &id) {
  auto *resource = find(id);
  if (!resource) return RemoteAccess::PENDING;
  reconcile(resource);
  return resource->access;
}
uint32_t RemoteResourceManager::continuity(const SupLan::ResourceId &id) {
  auto *resource = find(id);
  return resource && !resource->continuityPending ? resource->generation : 0;
}
uint8_t RemoteResourceManager::effectivePermissions(
    const SupLan::ResourceId &id) {
  auto *resource = find(id);
  if (!resource) return 0;
  reconcile(resource);
  return resource->effective;
}
bool RemoteResourceManager::peerAvailable(const SupLan::ResourceId &id) {
  auto *resource = find(id);
  if (!resource || !runtime_) return false;
  reconcile(resource);
  return resource->peerIndex != 0xff &&
         runtime_->hasSession(resource->peerIndex) &&
         !runtime_->recoveryStatus(resource->peerIndex).sleeping;
}
uint8_t RemoteResourceManager::approvalStatus(const SupLan::ResourceId &id,
                                              uint8_t permissions) {
  auto *resource = find(id);
  if (resource && permissions) {
    reconcile(resource);
    // An aggregate ENSURE answer cannot identify the rejected subset.
    // Do not attribute another consumer's decision to this handle.
    if ((permissions & ~resource->effective) !=
        (resource->approvalPermissions & ~resource->effective))
      return SUPLA_SUPLAN_ACCESS_STATUS_INVALID;
  }
  return resource ? resource->approval : SUPLA_SUPLAN_ACCESS_STATUS_INVALID;
}
bool RemoteResourceManager::control(const SupLan::ResourceId &id,
                                    const uint8_t *value, size_t length,
                                    uint32_t owner) {
  auto *resource = find(id);
  if (!resource || !runtime_) return false;
  reconcile(resource);
  return (resource->effective & SupLan::kPermissionControl) &&
         runtime_->sendControl(resource->peerIndex, id, value, length, owner);
}
bool RemoteResourceManager::action(const SupLan::ResourceId &id,
    uint32_t actionId,
                                   const uint8_t *params, size_t length,
                                   uint32_t owner) {
  auto *resource = find(id);
  if (!resource || !runtime_) return false;
  reconcile(resource);
  return (resource->effective & SupLan::kPermissionControl) &&
         runtime_->sendControlAction(resource->peerIndex, id, actionId,
                                     params, length, owner);
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
  TDS_SuplaDeviceChannel_E snapshot;
  memcpy(&snapshot, payload, sizeof(snapshot));
  if (snapshot.Number != 0xff) return;
  if (snapshot.Offline != SUPLA_CHANNEL_OFFLINE_FLAG_ONLINE &&
      resource->hasSnapshot) invalidate(resource);
  resource->snapshot = snapshot;
  resource->receivedMs = nowMs;
  resource->continuityPending = false;
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
  resource->approval = result.AccessStatus;
  resource->approvalPermissions = ensurePermissions_;
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
    if (!resource.used()) continue;
    resource.ensureAttempted = false;
    resource.manualAttempted = false;
    if (resource.access == RemoteAccess::BLOCKED) {
      resource.access = RemoteAccess::PENDING;
    }
  }
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
int RemoteResourceManager::consumerCount() const {
  int count = 0;
  for (const auto &entry : intents_) count += entry.consumerId != 0;
  return count;
}
bool RemoteResourceManager::hasConsumer(uint32_t consumer) const {
  for (const auto &intent : intents_)
    if (intent.consumerId == consumer) return true;
  return false;
}
int RemoteResourceManager::resourceCount() const {
  int count = 0;
  for (const auto &entry : resources_) count += entry.used();
  return count;
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
    if (!resource.used()) continue;
    reconcile(&resource);
    if (!(resource.effective & resource.permissions &
        SupLan::kPermissionRead) ||
        resource.peerIndex != index ||
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

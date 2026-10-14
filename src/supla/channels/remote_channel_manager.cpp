// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later
#include "remote_channel_manager.h"
#ifndef ARDUINO_ARCH_AVR
#include <string.h>
#include <supla/element.h>

namespace Supla {
namespace Device {
namespace {
const RemoteDelivery kDeliveries[] = {
  RemoteDelivery::SupLanPeer, RemoteDelivery::ServerStream
};
bool sameResource(const SupLan::ResourceId &a, const SupLan::ResourceId &b) {
  return a.type == b.type && a.id == b.id;
}
}  // namespace

RemoteChannelBackend *RemoteChannelManager::backend(RemoteDelivery mode) const {
  for (int i = 0; i < 2; ++i)
    if (kDeliveries[i] == mode) return backends_[i];
  return nullptr;
}
bool RemoteChannelManager::supports(RemoteDelivery mode) const {
  return backend(mode) != nullptr;
}
bool RemoteChannelManager::registerBackend(RemoteDelivery mode,
                                           RemoteChannelBackend *adapter) {
  if (!adapter || adapter->capacity() <= 0 ||
      adapter->consumerCapacity() <= 0) return false;
  for (int i = 0; i < 2; ++i)
    if (backends_[i] == adapter && kDeliveries[i] != mode) return false;
  for (int i = 0; i < 2; ++i) {
    if (kDeliveries[i] != mode) continue;
    if (backends_[i] && backends_[i] != adapter) return false;
    backends_[i] = adapter;
    return true;
  }
  return false;
}
void RemoteChannelManager::unregisterBackend(RemoteDelivery mode) {
  auto adapter = backend(mode);
  if (!adapter) return;
  for (int i = 0; i < adapter->consumerCapacity(); ++i) {
    const auto owner = adapter->intentAt(i).consumerId;
    if (owner) remove(owner);
  }
  for (int i = 0; i < 2; ++i)
    if (kDeliveries[i] == mode) backends_[i] = nullptr;
  adapter->detached();
}
RemoteChannelResource *RemoteChannelManager::findResource(
    RemoteChannelKey key) const {
  auto adapter = backend(key.delivery);
  if (!adapter || !key.channelId) return nullptr;
  for (int i = 0; i < adapter->capacity(); ++i) {
    auto &entry = adapter->resourceAt(i);
    if (entry.used() && entry.id.id == key.channelId) return &entry;
  }
  return nullptr;
}
bool RemoteChannelManager::consume(const ConsumeIntent &intent) {
  if (!intent.consumerId ||
      intent.resource.type != SupLan::kResourceTypeChannel ||
      !intent.resource.id || intent.resource.id > INT32_MAX ||
      !intent.permissions || (intent.permissions & ~7)) return false;
  if (intent.delivery == RemoteDelivery::ServerStream &&
      intent.permissions != SupLan::kPermissionRead) return false;
  auto adapter = backend(intent.delivery);
  if (!adapter && access_ &&
      access_->prepareBackend(static_cast<uint8_t>(intent.delivery), this))
    adapter = backend(intent.delivery);
  if (!adapter) return false;
  // The owner token denotes exactly one contribution, including its mode.
  for (auto other : backends_) {
    if (!other || other == adapter) continue;
    for (int i = 0; i < other->consumerCapacity(); ++i)
      if (other->intentAt(i).consumerId == intent.consumerId) {
        remove(intent.consumerId);
        break;
      }
  }
  int slot = -1;
  for (int i = 0; i < adapter->consumerCapacity(); ++i) {
    auto &entry = adapter->intentAt(i);
    if (entry.consumerId == intent.consumerId) {
      if (sameResource(entry.resource, intent.resource) &&
          entry.permissions == intent.permissions &&
          entry.approvalEligible == intent.approvalEligible) return true;
      if (sameResource(entry.resource, intent.resource)) {
        if ((entry.permissions & SupLan::kPermissionControl) &&
            !(intent.permissions & SupLan::kPermissionControl))
          adapter->cancelControls(intent.consumerId);
        if (!adapter->storeIntent(i, intent)) return false;
        auto resource = findResource({intent.resource.id, intent.delivery});
        if (!resource) return false;
        const uint8_t previous = resource->manualPermissions;
        demand(resource, intent.delivery);
        if (previous != resource->manualPermissions) {
          resource->manualAttempted = false;
          resource->approval = SUPLA_SUPLAN_ACCESS_STATUS_INVALID;
        }
        adapter->reconcileResource(*resource);
        return true;
      }
      remove(intent.consumerId);
      slot = i;
      break;
    }
    if (!entry.consumerId) slot = i;
  }
  if (slot < 0) {
    admissionExhausted_ = true;
    return false;
  }
  auto resource = findResource({intent.resource.id, intent.delivery});
  if (!resource) {
    for (int i = 0; i < adapter->capacity(); ++i) {
      if (!adapter->resourceAt(i).used()) {
        if (nextGeneration_ == UINT32_MAX) return false;
        if (!adapter->allocateResource(i)) {
          admissionExhausted_ = true;
          return false;
        }
        resource = &adapter->resourceAt(i);
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
  if (!adapter->storeIntent(slot, intent)) {
    // No consumer owns a freshly allocated entry yet.
    if (!resource->permissions) {
      for (int i = 0; i < adapter->capacity(); ++i)
        if (&adapter->resourceAt(i) == resource) {
          adapter->clearResource(i);
          break;
        }
    }
    admissionExhausted_ = true;
    return false;
  }
  const uint8_t previous = resource->manualPermissions;
  demand(resource, intent.delivery);
  if (previous != resource->manualPermissions)
    resource->manualAttempted = false;
  adapter->reconcileResource(*resource);
  return true;
}
void RemoteChannelManager::remove(uint32_t consumerId) {
  if (!consumerId) return;
  for (auto adapter : backends_) {
    if (!adapter) continue;
    adapter->cancelControls(consumerId);
    for (int i = 0; i < adapter->consumerCapacity(); ++i) {
      auto &intent = adapter->intentAt(i);
      if (intent.consumerId == consumerId) {
        adapter->storeIntent(i, {});
        if (nextGeneration_ != UINT32_MAX) ++nextGeneration_;
      }
    }
  }
  for (int mode = 0; mode < 2; ++mode) {
    auto adapter = backends_[mode];
    if (!adapter) continue;
    for (int i = 0; i < adapter->capacity(); ++i) {
      auto &resource = adapter->resourceAt(i);
      if (!resource.used()) continue;
      demand(&resource, kDeliveries[mode]);
      if (!resource.permissions) {
        if (ensurePending_ &&
            ensureResource_ == RemoteChannelKey{resource.id.id,
                                                kDeliveries[mode]}) {
          if (access_) access_->cancelEnsure();
          ensurePending_ = false;
          ensureNextMs_ = 0;
        }
        adapter->releaseResource(resource);
        adapter->clearResource(i);
      } else {
        adapter->reconcileResource(resource);
      }
    }
  }
}
void RemoteChannelManager::demand(RemoteChannelResource *resource,
                                  RemoteDelivery delivery) {
  resource->permissions = 0;
  resource->manualPermissions = 0;
  auto adapter = backend(delivery);
  for (int i = 0; i < adapter->consumerCapacity(); ++i) {
    const auto &intent = adapter->intentAt(i);
    if (!intent.consumerId || !sameResource(intent.resource, resource->id))
      continue;
    uint8_t rights = intent.permissions;
    if (rights & SupLan::kPermissionControl) rights |= SupLan::kPermissionRead;
    resource->permissions |= rights;
    if (intent.approvalEligible) resource->manualPermissions |= rights;
  }
}
void RemoteChannelManager::invalidate(RemoteChannelResource *resource) {
  resource->hasSnapshot = false;
  resource->generation = nextGeneration_ == UINT32_MAX ? 0 : ++nextGeneration_;
}
bool RemoteChannelManager::usable(RemoteChannelKey key,
    const RemoteChannelResource &resource, uint32_t nowMs) const {
  auto adapter = backend(key.delivery);
  return adapter && (resource.effective & resource.permissions &
                     SupLan::kPermissionRead) && resource.hasSnapshot &&
      resource.snapshot.Default != SUPLA_CHANNELFNC_NONE &&
      resource.generation && adapter->fresh(resource, nowMs);
}
void RemoteChannelManager::iterate(uint32_t nowMs) {
  admissionExhausted_ = false;
  for (int number = 0; number < 255; ++number) {
    if (auto element = Element::getElementByChannelNumber(number))
      element->reconcileChannelDependencies();
  }
  if (ensurePending_ && static_cast<int32_t>(nowMs - ensureNextMs_) >= 0) {
    if (access_) access_->cancelEnsure();
    ensurePending_ = false;
    if (auto resource = findResource(ensureResource_))
      resource->ensureAttempted = false;
    ensureNextMs_ = nowMs + 1000;
  }
  // Rotate after the previous key across backend and resource boundaries.
  const int firstMode = ensureResource_.delivery == RemoteDelivery::SupLanPeer
                            ? 1 : 0;
  for (int offset = 0; offset < 2; ++offset) {
    const int mode = (firstMode + offset) % 2;
    auto adapter = backends_[mode];
    if (!adapter) continue;
    int start = 0;
    if (ensureResource_.delivery == kDeliveries[mode]) {
      for (int i = 0; i < adapter->capacity(); ++i)
        if (adapter->resourceAt(i).id.id == ensureResource_.channelId &&
            ensureResource_.channelId)
          start = (i + 1) % adapter->capacity();
    }
    for (int i = 0; i < adapter->capacity(); ++i) {
      auto &resource = adapter->resourceAt((start + i) % adapter->capacity());
      if (!resource.used()) continue;
      adapter->reconcileResource(resource);
      adapter->refreshResource(resource, nowMs);
      const bool manual =
          resource.manualPermissions && !resource.manualAttempted;
      const bool missing =
          (resource.effective & resource.permissions) != resource.permissions;
      if (ensurePending_ ||
          !(manual || (missing && !resource.ensureAttempted)) ||
          !access_ || (ensureNextMs_ &&
          static_cast<int32_t>(nowMs - ensureNextMs_) < 0)) continue;
      TDS_SuplaEnsureResourceAccess request = {};
      request.Resource.ResourceType = resource.id.type;
      request.Resource.ResourceId = resource.id.id;
      request.Permissions =
          manual ? resource.manualPermissions : resource.permissions;
      if (manual)
        request.Flags = SUPLA_SUPLAN_ENSURE_ACCESS_FLAG_ALLOW_APPROVAL;
      request.DeliveryMode = static_cast<uint8_t>(kDeliveries[mode]);
      if (access_->ensure(request)) {
        ensurePending_ = true;
        ensureNextMs_ = nowMs + 10000;
        ensureResource_ = {resource.id.id, kDeliveries[mode]};
        ensurePermissions_ = request.Permissions;
        adapter->ensureStarted(resource);
        if (manual) resource.manualAttempted = true;
        resource.ensureAttempted =
            !manual || request.Permissions == resource.permissions;
      } else {
        ensureNextMs_ = nowMs + 1000;
      }
    }
  }
}
ChannelState RemoteChannelManager::state(RemoteChannelKey key, uint32_t nowMs) {
  auto resource = findResource(key);
  if (!resource) return ChannelState();
  backend(key.delivery)->reconcileResource(*resource);
  if (backend(key.delivery)->stableStorage())
    return ChannelState(&resource->snapshot, usable(key, *resource, nowMs),
                        resource->receivedMs, &resource->generation);
  return ChannelState(&resource->snapshot, usable(key, *resource, nowMs),
                      resource->receivedMs, this, resource->generation);
}
RemoteAccess RemoteChannelManager::access(RemoteChannelKey key) {
  auto resource = findResource(key);
  if (!resource) return RemoteAccess::PENDING;
  backend(key.delivery)->reconcileResource(*resource);
  return resource->access;
}
uint32_t RemoteChannelManager::continuity(RemoteChannelKey key) {
  auto resource = findResource(key);
  return resource && !resource->continuityPending ? resource->generation : 0;
}
uint8_t RemoteChannelManager::effectivePermissions(RemoteChannelKey key) {
  auto resource = findResource(key);
  if (!resource) return 0;
  backend(key.delivery)->reconcileResource(*resource);
  return resource->effective;
}
bool RemoteChannelManager::transportAvailable(RemoteChannelKey key) {
  auto resource = findResource(key);
  if (!resource) return false;
  auto adapter = backend(key.delivery);
  adapter->reconcileResource(*resource);
  return adapter->reachable(*resource);
}
uint8_t RemoteChannelManager::approvalStatus(RemoteChannelKey key,
                                            uint8_t permissions) {
  auto resource = findResource(key);
  if (resource && permissions) {
    backend(key.delivery)->reconcileResource(*resource);
    if ((permissions & ~resource->effective) !=
        (resource->approvalPermissions & ~resource->effective))
      return SUPLA_SUPLAN_ACCESS_STATUS_INVALID;
  }
  return resource ? resource->approval : SUPLA_SUPLAN_ACCESS_STATUS_INVALID;
}
ChannelAccessStatus RemoteChannelManager::status(RemoteChannelKey key,
                                                 uint8_t permissions) {
  if (!key.channelId || key.channelId > INT32_MAX || !permissions ||
      (permissions & ~7) || !supports(key.delivery))
    return ChannelAccessStatus::INVALID;
  if (!findResource(key)) return ChannelAccessStatus::RESOURCE_LIMIT;
  if (permissions & SupLan::kPermissionControl)
    permissions |= SupLan::kPermissionRead;
  if ((effectivePermissions(key) & permissions) == permissions)
    return transportAvailable(key) ? ChannelAccessStatus::READY
                             : ChannelAccessStatus::PEER_UNAVAILABLE;
  switch (approvalStatus(key, permissions)) {
    case SUPLA_SUPLAN_ACCESS_STATUS_PENDING_APPROVAL:
      return ChannelAccessStatus::PENDING_APPROVAL;
    case SUPLA_SUPLAN_ACCESS_STATUS_REJECTED:
      return ChannelAccessStatus::REJECTED;
    default: return ChannelAccessStatus::PROVISIONING;
  }
}
bool RemoteChannelManager::control(RemoteChannelKey key, const uint8_t *value,
                                   size_t length, uint32_t owner) {
  if (key.delivery != RemoteDelivery::SupLanPeer) return false;
  auto resource = findResource(key);
  if (!resource) return false;
  auto adapter = backend(key.delivery);
  adapter->reconcileResource(*resource);
  return (resource->effective & SupLan::kPermissionControl) &&
      adapter->controlResource(*resource, value, length, owner);
}
bool RemoteChannelManager::action(RemoteChannelKey key, uint32_t actionId,
    const uint8_t *params, size_t length, uint32_t owner) {
  if (key.delivery != RemoteDelivery::SupLanPeer) return false;
  auto resource = findResource(key);
  if (!resource) return false;
  auto adapter = backend(key.delivery);
  adapter->reconcileResource(*resource);
  return (resource->effective & SupLan::kPermissionControl) &&
      adapter->actionResource(*resource, actionId, params, length, owner);
}
void RemoteChannelManager::acceptSnapshot(RemoteChannelResource *resource,
    const uint8_t *payload, size_t length, uint32_t nowMs) {
  if (!resource || !payload || length != sizeof(resource->snapshot)) return;
  TDS_SuplaDeviceChannel_E snapshot;
  memcpy(&snapshot, payload, sizeof(snapshot));
  if (snapshot.Number != 0xff) return;
  if ((snapshot.Offline != SUPLA_CHANNEL_OFFLINE_FLAG_ONLINE ||
       snapshot.Default == SUPLA_CHANNELFNC_NONE) && resource->hasSnapshot)
    invalidate(resource);
  resource->snapshot = snapshot;
  resource->receivedMs = nowMs;
  resource->continuityPending = false;
  resource->hasSnapshot = true;
}
void RemoteChannelManager::ensureResult(
    const TSD_SuplaEnsureResourceAccessResult &result) {
  if (!ensurePending_ || result.DeliveryMode !=
      static_cast<uint8_t>(ensureResource_.delivery)) return;
  ensurePending_ = false;
  ensureNextMs_ = 0;
  auto resource = findResource(ensureResource_);
  if (!resource) return;
  backend(ensureResource_.delivery)->ensureCompleted(*resource, result);
  backend(ensureResource_.delivery)->reconcileResource(*resource);
  resource->approval = result.AccessStatus;
  resource->approvalPermissions = ensurePermissions_;
  if (resource->access == RemoteAccess::READY) return;
  if (result.Result != SUPLA_SUPLAN_RESULT_OK ||
      result.AccessStatus != SUPLA_SUPLAN_ACCESS_STATUS_GRANTED)
    resource->access = RemoteAccess::BLOCKED;
}
void RemoteChannelManager::serverReconnected() {
  retryDeferredConsumers();
  ensurePending_ = false;
  ensureNextMs_ = 0;
  for (auto adapter : backends_) {
    if (!adapter) continue;
    for (int i = 0; i < adapter->capacity(); ++i) {
      auto &resource = adapter->resourceAt(i);
      if (!resource.used()) continue;
      resource.ensureAttempted = false;
      resource.manualAttempted = false;
      if (resource.access == RemoteAccess::BLOCKED)
        resource.access = RemoteAccess::PENDING;
    }
  }
}
bool RemoteChannelManager::currentGeneration(uint32_t generation) const {
  if (!generation) return false;
  for (auto adapter : backends_) {
    if (!adapter) continue;
    for (int i = 0; i < adapter->capacity(); ++i) {
      auto &resource = adapter->resourceAt(i);
      if (resource.used() && resource.generation == generation) {
        adapter->reconcileResource(resource);
        return resource.generation == generation && resource.hasSnapshot &&
            (resource.effective & resource.permissions &
             SupLan::kPermissionRead) &&
            resource.snapshot.Default != SUPLA_CHANNELFNC_NONE;
      }
    }
  }
  return false;
}
int RemoteChannelManager::consumerCount() const {
  int count = 0;
  for (auto adapter : backends_) {
    if (!adapter) continue;
    for (int i = 0; i < adapter->consumerCapacity(); ++i)
      count += adapter->intentAt(i).consumerId != 0;
  }
  return count;
}
bool RemoteChannelManager::hasConsumer(uint32_t consumer) const {
  if (!consumer) return false;
  for (auto adapter : backends_) {
    if (!adapter) continue;
    for (int i = 0; i < adapter->consumerCapacity(); ++i)
      if (adapter->intentAt(i).consumerId == consumer) return true;
  }
  return false;
}
int RemoteChannelManager::resourceCount() const {
  int count = 0;
  for (auto adapter : backends_) {
    if (!adapter) continue;
    for (int i = 0; i < adapter->capacity(); ++i)
      count += adapter->resourceAt(i).used();
  }
  return count;
}
}  // namespace Device
}  // namespace Supla
#endif  // !ARDUINO_ARCH_AVR

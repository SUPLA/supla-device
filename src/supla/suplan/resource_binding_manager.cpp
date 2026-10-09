// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later
#include "resource_binding_manager.h"
#ifndef ARDUINO_ARCH_AVR
#include <string.h>
#include <stdio.h>
#include <supla/channels/channel_reference.h>
#include <supla/control/hvac_base.h>
#include <supla/control/relay_hvac_aggregator.h>
#include <supla/element.h>
#include <supla/storage/config.h>
#include <supla/storage/storage.h>
#include <supla/suplan/remote_resource_manager.h>
#include <supla/suplan/suplan_server_associations.h>
#include <supla/suplan/suplan_server_identity.h>
#include <supla/time.h>
#include <suplan/suplan_wire.h>

namespace Supla {
namespace Device {
namespace {
void bindingKey(uint8_t slot, char *key, size_t size) {
  snprintf(key, size, "slb_%u", slot);
}
bool sameRelation(const SupLan::PeerContext &a,
                  const SupLan::PeerContext &b) {
  return a.authorityType == b.authorityType && a.authorityId == b.authorityId &&
      a.source.nameSpace == b.source.nameSpace &&
      a.source.nodeId == b.source.nodeId &&
      a.destination.nameSpace == b.destination.nameSpace &&
      a.destination.nodeId == b.destination.nodeId;
}
const uint8_t kInstalled = 3;
const uint8_t kRejected = 24;
const uint8_t kCapacity = 26;
}  // namespace

ResourceBindingManager::ResourceBindingManager(SupLan::PeerTable *peers,
    SupLan::Runtime *runtime, RemoteResourceManager *resources,
    RemoteAccessPort *port)
    : peers_(peers), runtime_(runtime), resources_(resources), port_(port) {
  attachRuntime(runtime);
}
ResourceBindingManager::~ResourceBindingManager() {
  for (int i = 0; i < SUPLAN_MAX_PERSISTENT_PEERS; ++i) detach(i);
}
void ResourceBindingManager::attachRuntime(SupLan::Runtime *runtime) {
  retry_.length = 0;
  runtime_ = runtime;
}
void ResourceBindingManager::attachAssociations(ServerAssociations *state) {
  for (int i = 0; i < SUPLAN_MAX_PERSISTENT_PEERS; ++i) {
    detach(i);
    sets_[i] = {};
  }
  associations_ = state;
  loaded_ = false;
}
int ResourceBindingManager::bindingCount() const {
  int count = 0;
  for (const auto &entry : contributors_) if (entry.source) ++count;
  return count;
}
bool ResourceBindingManager::sessionNeeded(uint8_t peer) const {
  const auto owner = peers_->get(peer);
  return associations_ && associations_->identity() &&
      associations_->identity()->serverSyncComplete() && owner &&
      !owner->destination && peers_->hasActiveGrants(peer);
}
int ResourceBindingManager::peerForSet(uint8_t set) const {
  if (!sets_[set].used) return -1;
  for (int i = 0; i < SUPLAN_MAX_PERSISTENT_PEERS; ++i) {
    const auto peer = peers_->get(i);
    if (peer && peer->destination &&
        !memcmp(peer->contextBytes, sets_[set].context, 27)) return i;
  }
  return -1;
}
int ResourceBindingManager::targetNumber(uint32_t id) const {
  const auto identity = associations_ ? associations_->identity() : nullptr;
  if (!identity || !identity->identityAvailable()) return -1;
  // Configuration acceptance is allowed inside the transition barrier, but
  // activation still uses the normal resolver and remains unavailable there.
  for (int n = 0; n < 255; ++n) {
    const auto channel = Channel::GetByChannelNumber(n);
    if (channel && channel->getServerChannelId() == id &&
        channel->getChannelType() == SUPLA_CHANNELTYPE_RELAY &&
        (channel->getDefaultFunction() == SUPLA_CHANNELFNC_PUMPSWITCH ||
         channel->getDefaultFunction() ==
             SUPLA_CHANNELFNC_HEATORCOLDSOURCESWITCH) &&
        Control::RelayHvacAggregator::GetInstance(n))
      return n;
  }
  return -1;
}
bool ResourceBindingManager::validateBody(uint8_t peer, const uint8_t *body,
                                         size_t size,
                                         bool authorization) const {
  if (!body || !size || size != 1U + body[0] * 12U) return false;
  for (uint16_t i = 0; i < body[0]; ++i) {
    const auto entry = body + 1 + 12 * i;
    const SupLan::ResourceId source = {entry[1], SupLan::getUint32(entry + 2)};
    if (entry[0] != 1 || entry[11] != 0 ||
        source.type != SupLan::kResourceTypeChannel || !source.id ||
        source.id > INT32_MAX || entry[6] != SupLan::kResourceTypeChannel ||
        (authorization &&
         !peers_->authorize(peer, source, SupLan::kPermissionRead)) ||
        targetNumber(SupLan::getUint32(entry + 7)) < 0) return false;
    for (uint16_t j = 0; j < i; ++j)
      if (!memcmp(entry, body + 1 + 12 * j, 12)) return false;
  }
  return true;
}
void ResourceBindingManager::detach(uint8_t set) {
  for (int i = 0; i < SUPLAN_MAX_RESOURCE_BINDINGS; ++i) {
    auto &entry = contributors_[i];
    if (entry.source && entry.set == set) {
      if (resources_) resources_->remove(0x40000 + i);
      entry = {};
    }
  }
}
void ResourceBindingManager::activate(uint8_t set, const uint8_t *body,
                                     size_t size) {
  (void)size;
  // Preserve genuinely fresh sample history only for retained exact bindings.
  for (int i = 0; i < SUPLAN_MAX_RESOURCE_BINDINGS; ++i) {
    auto &old = contributors_[i];
    if (!old.source || old.set != set) continue;
    bool retained = false;
    for (int j = 0; j < body[0]; ++j) {
      const auto entry = body + 1 + 12 * j;
      retained = retained || (old.source == SupLan::getUint32(entry + 2) &&
                              old.target == SupLan::getUint32(entry + 7));
    }
    if (!retained) {
      if (resources_) resources_->remove(0x40000 + i);
      old = {};
    }
  }
  for (int j = 0; j < body[0]; ++j) {
    const auto wire = body + 1 + 12 * j;
    const auto source = SupLan::getUint32(wire + 2);
    const auto target = SupLan::getUint32(wire + 7);
    Contributor *slot = nullptr;
    bool exists = false;
    for (auto &entry : contributors_) {
      if (entry.source == source && entry.target == target && entry.set == set)
        exists = true;
      if (!entry.source) slot = &entry;
    }
    if (!exists && slot) {
      *slot = {};
      slot->source = source;
      slot->target = target;
      slot->set = set;
    }
  }
}
bool ResourceBindingManager::persist(uint8_t set, const uint8_t *record,
                                     size_t length) {
  auto cfg = Storage::ConfigInstance();
  if (!cfg) return false;
  char key[16];
  bindingKey(set, key, sizeof(key));
  const int oldSize = cfg->getBlobSize(key);
  const bool previous =
      oldSize > 0 && oldSize <= kRecordCapacity &&
      cfg->getBlob(key, reinterpret_cast<char *>(previous_), oldSize);
  if (!sets_[set].uncertain && previous &&
      oldSize == static_cast<int>(length) && !memcmp(previous_, record, length))
    return true;
  bool saved =
      length ? cfg->setBlob(key, reinterpret_cast<const char *>(record), length)
             : (oldSize < 0 || cfg->eraseKey(key));
  saved = saved && cfg->commit();
  if (saved && length) {
    uint8_t confirmed[kRecordCapacity];
    saved = cfg->getBlobSize(key) == static_cast<int>(length) &&
        cfg->getBlob(key, reinterpret_cast<char *>(confirmed), length) &&
        !memcmp(confirmed, record, length);
  } else if (saved) {
    saved = cfg->getBlobSize(key) < 0;
  }
  sets_[set].uncertain = !saved;
  // Keep unrelated Config commits from flushing an unacknowledged candidate.
  // Flash may already contain the complete candidate; uncertain replay still
  // requires a commit, never an equality-only durability claim.
  if (!saved) {
    if (previous) {
      cfg->setBlob(key, reinterpret_cast<char *>(previous_), oldSize);
    } else {
      cfg->eraseKey(key);
    }
  }
  return saved;
}
uint8_t ResourceBindingManager::accept(uint8_t peer, const uint8_t *body,
                                      size_t size) {
  if (!loaded_) load();
  const auto owner = peers_->get(peer);
  if (!owner || !owner->destination || !validateBody(peer, body, size))
    return kRejected;
  int slot = -1;
  for (int i = 0; i < SUPLAN_MAX_PERSISTENT_PEERS; ++i) {
    if (sets_[i].used && !memcmp(sets_[i].context, owner->contextBytes, 27)) {
      slot = i;
      break;
    }
  }
  if (!body[0] && slot < 0) return kInstalled;
  if (body[0] > SUPLAN_MAX_BINDINGS_PER_PEER) return kCapacity;
  int previousCount = 0;
  for (const auto &entry : contributors_)
    if (entry.source && entry.set == slot) ++previousCount;
  if (bindingCount() - previousCount + body[0] > SUPLAN_MAX_RESOURCE_BINDINGS)
    return kCapacity;
  if (slot < 0) {
    for (int i = 0; i < SUPLAN_MAX_PERSISTENT_PEERS; ++i)
      if (!sets_[i].used) {
        slot = i;
        break;
      }
  }
  if (slot < 0) return kCapacity;
  record_[0] = 1;
  memcpy(record_ + 1, owner->contextBytes, 27);
  memcpy(record_ + 28, body, size);
  if (!persist(slot, record_, body[0] ? size + 28 : 0)) return 0;
  memcpy(sets_[slot].context, owner->contextBytes, 27);
  sets_[slot].used = body[0] != 0;
  activate(slot, body, size);
  return kInstalled;
}
void ResourceBindingManager::load() {
  if (!associations_ || !associations_->available() ||
      !associations_->identity()->identityAvailable()) return;
  auto cfg = Storage::ConfigInstance();
  if (!cfg) return;
  loaded_ = true;
  for (int i = 0; i < SUPLAN_MAX_PERSISTENT_PEERS; ++i) {
    char key[16];
    bindingKey(i, key, sizeof(key));
    const auto size = cfg->getBlobSize(key);
    if (size < 29 || size > kRecordCapacity ||
        !cfg->getBlob(key, reinterpret_cast<char *>(record_), size) ||
        record_[0] != 1 || size != 29 + 12 * record_[28]) continue;
    memcpy(sets_[i].context, record_ + 1, 27);
    sets_[i].used = true;
    const auto peer = peerForSet(i);
    if (peer >= 0 && record_[28] <= SUPLAN_MAX_BINDINGS_PER_PEER &&
        bindingCount() + record_[28] <= SUPLAN_MAX_RESOURCE_BINDINGS &&
        validateBody(peer, record_ + 28, size - 28, false)) {
      activate(i, record_ + 28, size - 28);
    } else {
      // Stale context or revoked authorization never gains runtime authority.
      if (persist(i, nullptr, 0)) sets_[i].used = false;
    }
  }
}
bool ResourceBindingManager::authorizationReplacing(
    const SupLan::PeerContext &context, const SupLan::AclEntry *expected,
    uint16_t count) {
  if (!loaded_) load();
  uint8_t encoded[27];
  SupLan::encodePeerContext(&context, encoded);
  for (int slot = 0; slot < SUPLAN_MAX_PERSISTENT_PEERS; ++slot) {
    if (!sets_[slot].used) continue;
    SupLan::PeerContext old;
    if (!SupLan::decodePeerContext(sets_[slot].context, &old) ||
        !sameRelation(context, old)) continue;
    uint8_t body[1 + 12 * SUPLAN_MAX_BINDINGS_PER_PEER] = {};
    for (const auto &entry : contributors_) {
      if (!entry.source || entry.set != slot) continue;
      bool allowed = false;
      if (!memcmp(encoded, sets_[slot].context, 27)) {
        for (int j = 0; j < count; ++j) {
          allowed = allowed ||
              ((expected[j].permissions & (SupLan::kPermissionRead |
                                            SupLan::kPermissionControl)) &&
               ((expected[j].resource.type == SupLan::kResourceTypeChannel &&
                 expected[j].resource.id == entry.source) ||
                (expected[j].resource.type == SupLan::kResourceTypeDevice &&
                 expected[j].resource.id == context.source.nodeId)));
        }
      }
      if (allowed) {
        auto wire = body + 1 + 12 * body[0]++;
        wire[0] = 1;
        wire[1] = wire[6] = SupLan::kResourceTypeChannel;
        SupLan::putUint32(wire + 2, entry.source);
        SupLan::putUint32(wire + 7, entry.target);
      }
    }
    record_[0] = 1;
    memcpy(record_ + 1, sets_[slot].context, 27);
    memcpy(record_ + 28, body, 1 + 12 * body[0]);
    if (!persist(slot, record_, body[0] ? 29 + 12 * body[0] : 0)) return false;
    activate(slot, body, 1 + 12 * body[0]);
    sets_[slot].used = body[0] != 0;
  }
  return true;
}
void ResourceBindingManager::addIntent(uint32_t source, uint32_t target) {
  Intent *slot = nullptr;
  for (auto &entry : intents_) {
    if (entry.source == source && entry.target == target) {
      entry.retained = true;
      return;
    }
    if (!entry.source) slot = &entry;
  }
  if (slot) {
    *slot = {};
    slot->source = source;
    slot->target = target;
    slot->retained = true;
  } else {
    intentOverflow_ = true;
  }
}
bool ResourceBindingManager::collectIntents() {
  const auto identity = associations_ ? associations_->identity() : nullptr;
  if (!identity) return false;
  // A rejected/unconfirmed config is not an authoritative empty dependency.
  // Check the whole snapshot before changing any retained intents or routing.
  for (int n = 0; n < 255; ++n) {
    const auto element = Element::getElementByChannelNumber(n);
    const auto hvac = element ? element->getHvacBase() : nullptr;
    if (hvac && !hvac->isChannelConfigDurable()) return false;
  }
  intentOverflow_ = false;
  for (auto &intent : intents_) intent.retained = false;
  for (int n = 0; n < 255; ++n) {
    const auto element = Element::getElementByChannelNumber(n);
    const auto hvac = element ? element->getHvacBase() : nullptr;
    if (!hvac) continue;
    uint32_t source;
    if (!identity->reverse(n, &source)) continue;
    for (uint8_t field = 0; field < 2; ++field) {
      const auto reference = hvac->bindingTarget(field);
      const auto resolved = resolveChannelReference(reference, identity);
      if (resolved.kind == ChannelResolutionKind::kRemote)
        addIntent(source, reference.id);
    }
  }
  for (int i = 0; i < SUPLAN_MAX_RESOURCE_BINDINGS; ++i) {
    if (intents_[i].source && !intents_[i].retained && i != pendingIntent_)
      intents_[i] = {};
  }
  return true;
}
void ResourceBindingManager::ensureShareResult(
    const TSD_SuplaEnsureResourceShareResult &result) {
  if (pendingIntent_ < 0) return;
  auto &intent = intents_[pendingIntent_];
  intent.destination = result.Result == SUPLA_SUPLAN_RESULT_OK &&
      result.DestinationDeviceId > 0 ? result.DestinationDeviceId : 0;
  // A denied relation is BLOCKED, not an indefinitely unresolved route.
  // It cannot contribute a binding or hold unrelated provisioned peers.
  intent.blocked = !intent.destination;
  if (result.Result == SUPLA_SUPLAN_RESULT_PERSISTENCE_ERROR) {
    intent.blocked = false;
    intent.attempted = false;
    intent.nextEnsure = millis() + 1000;
  }
  pendingIntent_ = -1;
}
void ResourceBindingManager::acknowledged(uint8_t peer, uint8_t result) {
  if (result != kInstalled) {
    sent_[peer].attempted = false;
    sent_[peer].time = millis();
  }
}
void ResourceBindingManager::sendDesired(uint32_t now) {
  if (!runtime_ || runtime_->bindingsInFlight() || intentOverflow_) return;
  for (int peer = 0; peer < SUPLAN_MAX_PERSISTENT_PEERS; ++peer) {
    const auto owner = peers_->get(peer);
    if (!owner || owner->destination || !peers_->hasActiveGrants(peer))
      continue;
    SupLan::PeerContext context;
    if (!SupLan::decodePeerContext(owner->contextBytes, &context)) continue;
    uint8_t body[1 + 12 * SUPLAN_MAX_BINDINGS_PER_PEER] = {};
    bool overflow = false;
    // Wait for routing resolution before declaring an authoritative empty set.
    for (const auto &intent : intents_) {
      if (intent.source && intent.retained && !intent.destination &&
          !intent.blocked) return;
      if (!intent.source || !intent.retained || intent.blocked ||
          intent.destination != context.destination.nodeId)
        continue;
      if (!peers_->authorize(peer,
                             {SupLan::kResourceTypeChannel, intent.source},
                             SupLan::kPermissionRead)) {
        overflow = true;
        break;
      }
      if (body[0] == SUPLAN_MAX_BINDINGS_PER_PEER) {
        overflow = true;
        break;
      }
      auto wire = body + 1 + 12 * body[0]++;
      wire[0] = 1;
      wire[1] = wire[6] = SupLan::kResourceTypeChannel;
      SupLan::putUint32(wire + 2, intent.source);
      SupLan::putUint32(wire + 7, intent.target);
    }
    if (overflow) continue;
    uint32_t hash = 2166136261U;
    for (uint8_t byte : owner->contextBytes) hash = (hash ^ byte) * 16777619U;
    const size_t length = 1 + 12 * body[0];
    for (size_t i = 0; i < length; ++i) hash = (hash ^ body[i]) * 16777619U;
    auto &sent = sent_[peer];
    if (!sent.attempted && now - sent.time < 1000) continue;
    if (sent.attempted && sent.hash == hash && now - sent.time < 60000)
      continue;
    if (runtime_->sendBindings(peer, body, length)) {
      sent.hash = hash;
      sent.time = now;
      sent.attempted = true;
      return;
    }
  }
}
void ResourceBindingManager::serverReconnected() {
  if (pendingIntent_ >= 0 && port_) port_->cancelShare();
  pendingIntent_ = -1;
  for (auto &intent : intents_) {
    intent.attempted = false;
    intent.blocked = false;
    intent.nextEnsure = 0;
  }
}

void ResourceBindingManager::iterate(uint32_t now) {
  if (!loaded_) load();
  if (!loaded_) return;
  const auto identity = associations_->identity();
  if (identity && !identity->identityTransition() &&
      static_cast<int32_t>(now - cleanupNextMs_) >= 0) {
    cleanupNextMs_ = now + 1000;
    for (int setIndex = 0; setIndex < SUPLAN_MAX_PERSISTENT_PEERS; ++setIndex) {
      if (!sets_[setIndex].used) continue;
      const auto peer = peerForSet(setIndex);
      uint8_t body[1 + 12 * SUPLAN_MAX_BINDINGS_PER_PEER] = {};
      int oldCount = 0;
      for (const auto &entry : contributors_) {
        if (!entry.source || entry.set != setIndex) continue;
        ++oldCount;
        if (peer < 0 || targetNumber(entry.target) < 0 ||
            !peers_->authorize(peer,
                {SupLan::kResourceTypeChannel, entry.source},
                SupLan::kPermissionRead)) continue;
        auto wire = body + 1 + 12 * body[0]++;
        wire[0] = 1;
        wire[1] = wire[6] = SupLan::kResourceTypeChannel;
        SupLan::putUint32(wire + 2, entry.source);
        SupLan::putUint32(wire + 7, entry.target);
      }
      if (oldCount != body[0] || !oldCount) {
        record_[0] = 1;
        memcpy(record_ + 1, sets_[setIndex].context, 27);
        memcpy(record_ + 28, body, 1 + 12 * body[0]);
        if (persist(setIndex, record_, body[0] ? 29 + 12 * body[0] : 0)) {
          activate(setIndex, body, 1 + 12 * body[0]);
          sets_[setIndex].used = body[0] != 0;
        }
      }
    }
  }
  // Restore intents even without Server; binding persistence is not a value.
  for (int i = 0; i < SUPLAN_MAX_RESOURCE_BINDINGS; ++i) {
    auto &entry = contributors_[i];
    if (!entry.source) continue;
    const auto peer = peerForSet(entry.set);
    if (peer < 0 || targetNumber(entry.target) < 0 ||
        !peers_->authorize(peer, {SupLan::kResourceTypeChannel, entry.source},
                            SupLan::kPermissionRead)) {
      resources_->remove(0x40000 + i);
      entry.active = false;
      continue;
    }
    if (!resources_->consume({static_cast<uint32_t>(0x40000 + i),
        {SupLan::kResourceTypeChannel, entry.source}, SupLan::kPermissionRead}))
      continue;
    const auto state =
        resources_->state({SupLan::kResourceTypeChannel, entry.source}, now);
    THVACValue value;
    if (state.availableFor(ChannelCapability::HvacDemand) &&
        state.hvac(&value)) {
      entry.freshMs = state.receivedMs();
      entry.active = value.Flags & (SUPLA_HVAC_VALUE_FLAG_HEATING |
                                    SUPLA_HVAC_VALUE_FLAG_COOLING);
    }
  }
  if (!identity || !identity->serverSyncComplete()) return;
  if (!collectIntents()) return;
  if (pendingIntent_ >= 0 && static_cast<int32_t>(now - ensureDeadline_) >= 0) {
    if (port_) port_->cancelShare();
    auto &intent = intents_[pendingIntent_];
    intent.attempted = false;
    intent.nextEnsure = now + 1000;
    pendingIntent_ = -1;
  }
  if (pendingIntent_ < 0 && port_) {
    for (int i = 0; i < SUPLAN_MAX_RESOURCE_BINDINGS; ++i) {
      auto &intent = intents_[i];
      if (!intent.source || !intent.retained || intent.destination ||
          intent.attempted || static_cast<int32_t>(now - intent.nextEnsure) < 0)
        continue;
      TDS_SuplaEnsureResourceShare request = {};
      request.SourceResource.ResourceType = SupLan::kResourceTypeChannel;
      request.SourceResource.ResourceId = intent.source;
      request.DestinationResource.ResourceType = SupLan::kResourceTypeChannel;
      request.DestinationResource.ResourceId = intent.target;
      request.Permissions = SupLan::kPermissionRead;
      if (port_->share(request)) {
        pendingIntent_ = i;
        ensureDeadline_ = now + 10000;
        intent.attempted = true;
      } else {
        intent.nextEnsure = now + 1000;
      }
      break;
    }
  }
  sendDesired(now);
}
bool ResourceBindingManager::relayDemand(uint8_t target, bool *configured) {
  bool demand = false;
  if (configured) *configured = false;
  if (!associations_ || !associations_->identity() ||
      associations_->identity()->identityTransition()) return false;
  for (int index = 0; index < SUPLAN_MAX_RESOURCE_BINDINGS; ++index) {
    const auto &entry = contributors_[index];
    if (!entry.source || targetNumber(entry.target) != target) continue;
    const auto peer = peerForSet(entry.set);
    if (peer < 0 || !peers_->authorize(peer,
        {SupLan::kResourceTypeChannel, entry.source}, SupLan::kPermissionRead))
      continue;
    if (configured) *configured = true;
    if (!resources_->hasConsumer(0x40000 + index)) continue;
    const auto state = resources_->state(
        {SupLan::kResourceTypeChannel, entry.source}, millis());
    THVACValue value;
    const bool usable = state.availableFor(ChannelCapability::HvacDemand) &&
                        state.hvac(&value);
    demand = demand || (usable
        ? (value.Flags & (SUPLA_HVAC_VALUE_FLAG_HEATING |
                          SUPLA_HVAC_VALUE_FLAG_COOLING)) != 0
        : entry.active && millis() - entry.freshMs < 900000);
  }
  return demand;
}
}  // namespace Device
}  // namespace Supla
#endif  // !ARDUINO_ARCH_AVR

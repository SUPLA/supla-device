// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef ARDUINO_ARCH_AVR
#include <stdio.h>
#include <inttypes.h>
#include <supla/log_wrapper.h>
#include <string.h>
#include <supla/crypto.h>
#include <supla/channels/channel.h>
#include <supla/crc16.h>
#include <supla/storage/config.h>
#include <supla/suplan/suplan_server_associations.h>
#include <supla/suplan/suplan_server_identity.h>
#include <supla/suplan/suplan_peer_store.h>
#include <suplan/suplan_crypto.h>
#include <suplan/suplan_wire.h>

namespace {
using Supla::SupLan::PeerContext;
using Supla::SupLan::ResourceId;
PeerContext fromWire(const TSuplaSuplanPeerContext &wire) {
  return {wire.AuthorityType,
          wire.AuthorityId,
          {wire.SourceNodeIdNamespace, wire.SourceNodeId},
          {wire.DestinationNodeIdNamespace, wire.DestinationNodeId},
          wire.RootEpoch,
          wire.PeerGeneration};
}
bool sameRelation(const PeerContext &a, const PeerContext &b) {
  return a.authorityType == b.authorityType && a.authorityId == b.authorityId &&
         a.source.nameSpace == b.source.nameSpace &&
         a.source.nodeId == b.source.nodeId &&
         a.destination.nameSpace == b.destination.nameSpace &&
         a.destination.nodeId == b.destination.nodeId;
}
void seal(uint8_t *record, int size) {
  uint16_t crc = calculateCrc16(record, size - 2);
  record[size - 2] = crc;
  record[size - 1] = crc >> 8;
}
bool keyValid(const uint8_t *key) {
  if (!key) return false;
  bool zero = true;
  bool ones = true;
  for (int i = 0; i < 32; ++i) {
    zero = zero && key[i] == 0;
    ones = ones && key[i] == 0xff;
  }
  return !zero && !ones;
}
}  // namespace

namespace Supla {
namespace Device {
using namespace SupLan;  // NOLINT(build/namespaces)

ServerAssociations *ServerAssociations::first_ = nullptr;
void ServerAssociations::scopeChanged(ServerIdentity *identity) {
  for (auto state = first_; state; state = state->next_)
    if (state->identity_ == identity) state->identityChanged();
}
void ServerAssociations::storageChanged(Config *config, bool bindings) {
  for (auto state = first_; state; state = state->next_)
    if (state->config_ == config) state->bindingsStored_ = bindings;
}
ServerAssociations::ServerAssociations(PeerTable *peers, Runtime *runtime)
    : peers_(peers), runtime_(runtime) {
  next_ = first_;
  first_ = this;
  memset(slots_, 0xff, sizeof(slots_));
}
ServerAssociations::~ServerAssociations() {
  auto link = &first_;
  while (*link && *link != this) link = &(*link)->next_;
  if (*link) *link = next_;
  if (peers_) {
    for (uint16_t i = 0; i < SUPLAN_MAX_PERSISTENT_PEERS; ++i) clear(i);
    peers_->setOwnershipCheck(nullptr, nullptr);
  }
  memset(record_, 0, sizeof(record_));
}
void ServerAssociations::clear(uint8_t index) {
  if (slots_[index] != 0xff) {
    runtime_->clearPeer(index);
    peers_->removePeer(index);
    slots_[index] = 0xff;
  }
}

void ServerAssociations::refreshOwnership() {
  const uint32_t seen = identity_ ? identity_->revision() : 0;
  if (peers_ && seen != ownershipSeen_) {
    ownershipSeen_ = seen;
    peers_->ownershipChanged();
  }
}

bool ServerAssociations::owns(void *self, const PeerContext &context,
                              const ResourceId &resource) {
  auto state = static_cast<ServerAssociations *>(self);
  if (context.authorityType != kAuthorityServer) {
    return resource.type == kResourceTypeDevice
               ? resource.id == context.source.nodeId
               : resource.type == kResourceTypeChannel &&
                     (resource.id >> 16) == context.source.nodeId;
  }
  auto identity = state->identity_;
  if (!identity || !identity->identityAvailable() ||
      identity->identityTransition())
    return false;
  auto local = static_cast<uint32_t>(identity->serverDeviceId());
  if (context.source.nodeId == local) {
    if (context.rootEpoch != identity->rootEpoch()) return false;
    if (resource.type == kResourceTypeDevice) return resource.id == local;
    return resource.type == kResourceTypeChannel &&
           identity->resolve(resource.id).location ==
               ServerChannelLocation::kLocal;
  }
  if (context.destination.nodeId != local) return false;
  // Remote ownership is asserted by authenticated Source, whose matcher uses
  // its identity map. Destination additionally enforces its complete Expected.
  return resource.type == kResourceTypeDevice
             ? resource.id == context.source.nodeId
             : resource.type == kResourceTypeChannel && resource.id > 0 &&
                   resource.id <= INT32_MAX &&
                   identity->resolve(resource.id).location ==
                       ServerChannelLocation::kRemote;
}

uint8_t ServerAssociations::validate(const PeerContext &context,
                                     uint32_t revision,
                                     const TSuplaSuplanAclEntry *entries,
                                     uint16_t count, bool destination) const {
  if (context.authorityType != kAuthorityServer) {
    return SUPLA_SUPLAN_RESULT_UNSUPPORTED;
  }
  if (!validPeerContext(&context) || context.authorityId != 0 ||
      context.source.nameSpace != kNodeIdDevice ||
      context.destination.nameSpace != kNodeIdDevice ||
      !context.source.nodeId || !context.destination.nodeId ||
      !context.rootEpoch || context.rootEpoch == UINT32_MAX ||
      !context.peerGeneration || context.source.nodeId > INT32_MAX ||
      context.destination.nodeId > INT32_MAX ||
      context.source.nodeId == context.destination.nodeId || !revision ||
      count > SUPLA_SUPLAN_MAX_ACL_ENTRIES)
    return SUPLA_SUPLAN_RESULT_INVALID_ARGUMENT;
  if (!identity_->identityAvailable()) return SUPLA_SUPLAN_RESULT_NOT_FOUND;
  auto local = static_cast<uint32_t>(identity_->serverDeviceId());
  if ((destination ? context.destination.nodeId : context.source.nodeId) !=
      local)
    return SUPLA_SUPLAN_RESULT_NOT_AUTHORIZED;
  if (!destination && context.rootEpoch != identity_->rootEpoch()) {
    return SUPLA_SUPLAN_RESULT_ROOT_EPOCH_MISMATCH;
  }
  if (count > kMaxEntries) return SUPLA_SUPLAN_RESULT_CAPACITY_EXCEEDED;
  for (uint16_t i = 0; i < count; ++i) {
    const auto &entry = entries[i];
    if (entry.ResourceType != kResourceTypeChannel &&
        entry.ResourceType != kResourceTypeDevice) {
      return SUPLA_SUPLAN_RESULT_UNSUPPORTED;
    }
    if (!entry.ResourceId || entry.ResourceId > INT32_MAX ||
        !entry.Permissions || (entry.Permissions & ~7) ||
        ((entry.Permissions & kPermissionControl) &&
         (entry.Permissions & kPermissionRead))) {
      return SUPLA_SUPLAN_RESULT_INVALID_ARGUMENT;
    }
    if (i && (entries[i - 1].ResourceType > entry.ResourceType ||
              (entries[i - 1].ResourceType == entry.ResourceType &&
               entries[i - 1].ResourceId >= entry.ResourceId))) {
      return SUPLA_SUPLAN_RESULT_INVALID_ARGUMENT;
    }
    if (entry.ResourceType == kResourceTypeDevice) {
      if (entry.ResourceId != context.source.nodeId) {
        return SUPLA_SUPLAN_RESULT_NOT_AUTHORIZED;
      }
    } else if (!destination) {
      // Provisioning may run behind a transition barrier using the NEW map.
      bool found = false;
      for (auto ch = Channel::Begin(); ch; ch = ch->next()) {
        found = found || ch->getServerChannelId() == entry.ResourceId;
      }
      if (!found) return SUPLA_SUPLAN_RESULT_NOT_AUTHORIZED;
    } else {
      for (auto ch = Channel::Begin(); ch; ch = ch->next()) {
        if (ch->getServerChannelId() == entry.ResourceId) {
          return SUPLA_SUPLAN_RESULT_NOT_AUTHORIZED;
        }
      }
    }
  }
  return SUPLA_SUPLAN_RESULT_OK;
}

bool ServerAssociations::persist(int &slot, const uint8_t *record,
                                 int size) {
  SupLanPeerStore store(config_);
  if (!store.load(size ? size + 4 : 0)) return false;
  PeerContext context;
  if (!decodePeerContext(record + 2, &context)) return false;
  const auto type = record[1] == 2 ? SupLanPeerStore::Destination
                                  : SupLanPeerStore::Source;
  if (type == SupLanPeerStore::Destination &&
      !store.prune(context, entries_, record[33])) return false;
  store.discardAssociations(identity_->serverDeviceId(), identity_->rootEpoch(),
                            context);
  if (slot < 0) {
    for (uint16_t candidate = 0; candidate < SUPLAN_MAX_PERSISTENT_PEERS;
         ++candidate) {
      size_t length = 0;
      if (store.get(SupLanPeerStore::Source, candidate, &length)) continue;
      bool used = false;
      for (const auto occupied : slots_) used = used || occupied == candidate;
      if (!used) {
        slot = candidate;
        break;
      }
    }
  }
  if (slot < 0 || !store.replace(type, slot, record, size) || !store.save())
    return false;
  return true;
}

bool ServerAssociations::readRecord(uint8_t slot, PeerContext *context,
                                    uint32_t *revision, uint8_t *count,
                                    bool *destination,
                                    const SupLanPeerStore *loaded) {
  SupLanPeerStore store(config_);
  if (!loaded && !store.load()) return false;
  size_t length = 0;
  const auto bytes = (loaded ? loaded : &store)->get(
      SupLanPeerStore::Source, slot, &length);
  if (!bytes || length > sizeof(record_)) return false;
  memcpy(record_, bytes, length);
  // The store already checked record structure, CRC and profile totals.
  // Keep the live identity/ownership validation below.
  decodePeerContext(record_ + 2, context);
  *destination = record_[1] == 2;
  *revision = getUint32(record_ + 29);
  *count = record_[33];
  const int prefix = *destination ? kDestinationPrefix : kSourcePrefix;
  // Decode entries explicitly; never serialize C++ object memory.
  auto wire = wireEntries_;
  for (uint8_t i = 0; i < *count; ++i) {
    const uint8_t *entry = record_ + prefix + 6 * i;
    wire[i] = {entry[0], getUint32(entry + 1), entry[5]};
    entries_[i] = {{wire[i].ResourceType, wire[i].ResourceId},
                   wire[i].Permissions};
  }
  return validate(*context, *revision, wire, *count, *destination) ==
         SUPLA_SUPLAN_RESULT_OK;
}

void ServerAssociations::load(Config *config, ServerIdentity *identity) {
  for (uint16_t i = 0; i < SUPLAN_MAX_PERSISTENT_PEERS; ++i) clear(i);
  config_ = config;
  identity_ = identity;
  if (peers_) peers_->ownershipChanged();
  storageReady_ = false;
  loadAttempted_ = true;
  loadRetryMs_ = runtime_ ? runtime_->nowMs() : 0;
  if (!available() || !identity_->retryLegacyCleanup()) return;
  peers_->setOwnershipCheck(owns, this);
  runtime_->setLocalAddress(
      {kNodeIdDevice, static_cast<uint32_t>(identity_->serverDeviceId())});
  SupLanPeerStore store(config_);
  if (!store.load()) return;
  storageReady_ = true;
  bindingsStored_ = store.hasBindings();
  if (identity_->fullScopeFenced()) return;
  for (uint16_t slot = 0; slot < SUPLAN_MAX_PERSISTENT_PEERS; ++slot) {
    size_t length = 0;
    if (!store.get(SupLanPeerStore::Source, slot, &length)) continue;
    PeerContext context = {};
    uint32_t revision = 0;
    uint8_t count = 0;
    bool destination = false;
    if (!readRecord(slot, &context, &revision, &count, &destination, &store))
      continue;
    uint8_t index = 0;
    bool added =
        destination
            ? peers_->addPeer(&context, record_ + kSourcePrefix, revision,
                              entries_, count, &index, true)
            : peers_->addPeerFromRoot(&context, identity_->rootKey(), revision,
                                      entries_, count, &index);
    if (added) {
      slots_[index] = slot;
    }
    SUPLA_LOG_INFO("SupLAN association restored peer=%u role=%s revision=%"
        PRIu32 " resources=%u result=%s", index,
        destination ? "Destination" : "Source", revision, count,
        added ? "OK" : "REJECTED");
  }
  memset(record_, 0, sizeof(record_));
}

void ServerAssociations::retryLoad(uint32_t now) {
  if (storageReady_) return;
  // Use the same live monotonic clock as load(), not an older loop timestamp.
  if (runtime_) now = runtime_->nowMs();
  if (!loadAttempted_ || static_cast<uint32_t>(now - loadRetryMs_) >= 1000)
    load(config_, identity_);
}

bool ServerAssociations::reloadSourceRights(ServerIdentity *identity) {
  for (auto state = first_; state; state = state->next_) {
    if (state->identity_ != identity || !state->available()) continue;
    if (state->busy_) return false;
    SupLanPeerStore store(state->config_);
    if (!store.load()) return false;
    for (uint16_t i = 0; i < SUPLAN_MAX_PERSISTENT_PEERS; ++i) {
      const auto peer = state->peers_->get(i);
      if (!peer || state->slots_[i] == 0xff) continue;
      PeerContext context = {};
      uint32_t revision = 0;
      uint8_t count = 0;
      bool destination = false;
      const auto slot = state->slots_[i];
      if (!state->readRecord(slot, &context, &revision, &count, &destination,
                             &store) ||
          revision != peer->aclRevision ||
          memcmp(peer->contextBytes, state->record_ + 2, kPeerContextSize))
        return false;
      if (destination) continue;
      if (state->peers_->sameAcl(i, state->entries_, count)) continue;
      // Cancel pending operations, keeping credential/revision and slot stable.
      state->runtime_->clearPeer(i);
      if (!state->peers_->restrictAcl(i, state->entries_, count)) return false;
    }
    memset(state->record_, 0, sizeof(state->record_));
  }
  return true;
}

void ServerAssociations::identityChanged() {
  if (!available()) return;
  runtime_->setLocalAddress(
      {kNodeIdDevice, static_cast<uint32_t>(identity_->serverDeviceId())});
  for (uint16_t i = 0; i < SUPLAN_MAX_PERSISTENT_PEERS; ++i) {
    auto peer = peers_->get(i);
    if (!peer) continue;
    PeerContext context = {};
    decodePeerContext(peer->contextBytes, &context);
    if (identity_->fullScopeFenced() &&
        context.authorityType == kAuthorityServer) {
      runtime_->clearPeer(i);
      peers_->removePeer(i);
      slots_[i] = 0xff;
      continue;
    }
    if (slots_[i] == 0xff) continue;
    uint32_t local = static_cast<uint32_t>(identity_->serverDeviceId());
    // Authority roles come from the immutable context, not a mutable hint.
    const bool destination = context.destination.nodeId == local;
    if ((destination ? context.destination.nodeId : context.source.nodeId) !=
            local ||
        (!destination && context.rootEpoch != identity_->rootEpoch())) {
      clear(i);
    } else if (!owns(this, context,
                     {kResourceTypeDevice, context.source.nodeId})) {
      runtime_->clearPeer(i);
    }
  }
  if (identity_->fullScopeFenced()) {
    memset(record_, 0, sizeof(record_));
    memset(entries_, 0, sizeof(entries_));
    memset(wireEntries_, 0, sizeof(wireEntries_));
  }
}

uint8_t ServerAssociations::apply(const TSuplaSuplanPeerContext &wire,
                                  uint32_t revision,
                                  const TSuplaSuplanAclEntry *entries,
                                  uint16_t count, bool destination,
                                  const uint8_t *key) {
  if (!available()) return SUPLA_SUPLAN_RESULT_UNSUPPORTED;
  if (!storageReady_) return SUPLA_SUPLAN_RESULT_PERSISTENCE_ERROR;
  if (busy_) return SUPLA_SUPLAN_RESULT_INVALID_ARGUMENT;
  PeerContext context = fromWire(wire);
  uint8_t result = validate(context, revision, entries, count, destination);
  if (result != SUPLA_SUPLAN_RESULT_OK) return result;
  int index = -1;
  bool exact = false;
  for (uint16_t i = 0; i < SUPLAN_MAX_PERSISTENT_PEERS; ++i) {
    const PeerRecord *peer = peers_->get(i);
    PeerContext stored = {};
    if (!peer || slots_[i] == 0xff ||
        !decodePeerContext(peer->contextBytes, &stored) ||
        !sameRelation(context, stored))
      continue;
    index = i;
    if (context.rootEpoch == stored.rootEpoch) {
      if (context.peerGeneration < stored.peerGeneration) {
        return SUPLA_SUPLAN_RESULT_STALE_GENERATION;
      }
      exact = context.peerGeneration == stored.peerGeneration;
    }
    break;
  }
  const PeerRecord *old = index < 0 ? nullptr : peers_->get(index);
  if (exact && revision < old->aclRevision) {
    return SUPLA_SUPLAN_RESULT_STALE_REVISION;
  }
  for (uint16_t i = 0; i < count; ++i) {
    entries_[i] = {{entries[i].ResourceType, entries[i].ResourceId},
                   entries[i].Permissions};
  }
  bool identical = false;
  if (exact && revision == old->aclRevision) {
    identical = peers_->sameAcl(index, entries_, count);
    if (!identical ||
        (destination && key && memcmp(key, old->peerKey, kKeySize))) {
      return SUPLA_SUPLAN_RESULT_REVISION_CONFLICT;
    }
  }
  if (destination && count && !key && !exact) {
    return SUPLA_SUPLAN_RESULT_PEER_KEY_REQUIRED;
  }
  if (destination && count && key && !keyValid(key)) {
    return SUPLA_SUPLAN_RESULT_INVALID_ARGUMENT;
  }
  if (exact && destination && key && memcmp(key, old->peerKey, kKeySize)) {
    return SUPLA_SUPLAN_RESULT_REVISION_CONFLICT;
  }
  if (identical) {
    return SUPLA_SUPLAN_RESULT_OK;
  }
  if (!count && index < 0) return SUPLA_SUPLAN_RESULT_OK;
  if (count + peers_->entryCount(destination) - (old ? old->aclCount : 0) >
      (destination ? SUPLAN_MAX_TOTAL_EXPECTED_ENTRIES
                   : SUPLAN_MAX_TOTAL_ACL_ENTRIES)) {
    return SUPLA_SUPLAN_RESULT_CAPACITY_EXCEEDED;
  }
  if (count && index < 0 && peers_->freeSlot() < 0) {
    return SUPLA_SUPLAN_RESULT_CAPACITY_EXCEEDED;
  }
  int slot = index < 0 ? -1 : slots_[index];
  // Precompute cryptographic material before the persistence boundary.
  PeerMaterial material = {};
  Supla::Crypto::ScopedWipe<PeerMaterial> wipeMaterial(material);
  bool derived = destination ? derivePeerMaterialFromKey(&context,
                                                         key   ? key
                                                         : old ? old->peerKey
                                                               : nullptr,
                                                         &material)
                             : derivePeerMaterial(
                                   &context, identity_->rootKey(), &material);
  if (count && !derived) return SUPLA_SUPLAN_RESULT_PERSISTENCE_ERROR;
  int prefix = destination ? kDestinationPrefix : kSourcePrefix;
  int size = prefix + 6 * count + 2;
  memset(record_, 0, sizeof(record_));
  record_[0] = 1;
  record_[1] = destination ? 2 : 1;
  encodePeerContext(&context, record_ + 2);
  putUint32(record_ + 29, revision);
  record_[33] = count;
  if (destination && count)
    memcpy(record_ + kSourcePrefix, material.peerKey, kKeySize);
  for (uint16_t i = 0; i < count; ++i) {
    uint8_t *entry = record_ + prefix + 6 * i;
    entry[0] = entries[i].ResourceType;
    putUint32(entry + 1, entries[i].ResourceId);
    entry[5] = entries[i].Permissions;
  }
  seal(record_, size);
  busy_ = true;
  bool saved = persist(slot, record_, count ? size : 0);
  memset(record_, 0, sizeof(record_));
  if (!saved) {
    SUPLA_LOG_WARNING("SupLAN association persistence failed role=%s revision=%"
        PRIu32 " resources=%u", destination ? "Destination" : "Source",
        revision, count);
    busy_ = false;
    memset(&material, 0, sizeof(material));
    return SUPLA_SUPLAN_RESULT_PERSISTENCE_ERROR;
  }
  // The complete topology and association are durable together. Only now
  // detach obsolete runtime contributors; no second persistent write.
  if (destination)
    runtime_->authorizationReplacing(context, entries_, count);
  bool activated = true;
  if (!count) {
    clear(index);
  } else if (exact) {
    activated = peers_->replaceAcl(index, revision, entries_, count);
    runtime_->authorizationChanged(index);
  } else {
    if (index >= 0) clear(index);
    uint8_t added = 0;
    activated = peers_->addPeer(&context, material.peerKey, revision, entries_,
                                count, &added, destination);
    if (activated) {
      slots_[added] = slot;
    }
  }
  identityChanged();
  busy_ = false;
  memset(&material, 0, sizeof(material));
  SUPLA_LOG_INFO("SupLAN association/rights changed slot=%d Source=%" PRIu32
      " Destination=%" PRIu32 " role=%s revision=%" PRIu32
      " resources=%u result=%s", slot, context.source.nodeId,
      context.destination.nodeId,
      destination ? "Destination" : "Source", revision, count,
      activated ? "OK" : "ACTIVATION_FAILED");
  return activated ? SUPLA_SUPLAN_RESULT_OK
                   : SUPLA_SUPLAN_RESULT_PERSISTENCE_ERROR;
}

TDS_SuplaSetSuplanSourceAssociationResult ServerAssociations::accept(
    const TSDS_SuplaSetSuplanSourceAssociation &request) {
  TDS_SuplaSetSuplanSourceAssociationResult result = {};
  result.PeerContext = request.PeerContext;
  result.AclRevision = request.AclRevision;
  if ((request.Flags & ~SUPLA_SUPLAN_SOURCE_FLAG_RETURN_PEER_KEY) ||
      (request.Flags && request.AclEntryCount == 0)) {
    result.Result = SUPLA_SUPLAN_RESULT_INVALID_ARGUMENT;
    return result;
  }
  result.Result = apply(request.PeerContext, request.AclRevision, request.Acl,
                        request.AclEntryCount, false, nullptr);
  if (result.Result == SUPLA_SUPLAN_RESULT_OK && request.Flags) {
    PeerContext context = fromWire(request.PeerContext);
    PeerMaterial material = {};
    Supla::Crypto::ScopedWipe<PeerMaterial> wipeMaterial(material);
    if (!derivePeerMaterial(&context, identity_->rootKey(), &material)) {
      result.Result = SUPLA_SUPLAN_RESULT_PERSISTENCE_ERROR;
    } else {
      memcpy(result.PeerKey, material.peerKey, kKeySize);
      result.PeerKeySize = kKeySize;
    }
    memset(&material, 0, sizeof(material));
  }
  return result;
}
TDS_SuplaSetSuplanDestinationAssociationResult ServerAssociations::accept(
    const TSDS_SuplaSetSuplanDestinationAssociation &request) {
  TDS_SuplaSetSuplanDestinationAssociationResult result = {};
  result.PeerContext = request.PeerContext;
  result.AclRevision = request.AclRevision;
  if (request.PeerKeySize != 0 && request.PeerKeySize != kKeySize) {
    result.Result = SUPLA_SUPLAN_RESULT_INVALID_ARGUMENT;
  } else {
    result.Result = apply(request.PeerContext, request.AclRevision,
                          request.Resources, request.ResourceCount, true,
                          request.PeerKeySize ? request.PeerKey : nullptr);
  }
  return result;
}
}  // namespace Device
}  // namespace Supla
#endif  // !ARDUINO_ARCH_AVR

// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef ARDUINO_ARCH_AVR
#include <stdio.h>
#include <string.h>
#include <supla/channels/channel.h>
#include <supla/crc16.h>
#include <supla/storage/config.h>
#include <supla/suplan/suplan_server_associations.h>
#include <supla/suplan/suplan_server_identity.h>
#include <suplan/suplan_crypto.h>
#include <suplan/suplan_wire.h>

namespace {
using Supla::SupLan::PeerContext;
using Supla::SupLan::ResourceId;
void slotKey(uint8_t slot, char (&key)[16]) {
  snprintf(key, sizeof(key), "sl-peer-%u", static_cast<unsigned>(slot));
}
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

ServerAssociations::ServerAssociations(PeerTable *peers, Runtime *runtime)
    : peers_(peers), runtime_(runtime) {
  memset(slots_, 0xff, sizeof(slots_));
}
ServerAssociations::~ServerAssociations() {
  if (peers_) {
    for (uint16_t i = 0; i < SUPLAN_MAX_PERSISTENT_PEERS; ++i) clear(i);
    peers_->setOwnershipCheck(nullptr, nullptr);
  }
  memset(record_, 0, sizeof(record_));
  memset(previous_, 0, sizeof(previous_));
}
void ServerAssociations::clear(uint8_t index) {
  if (slots_[index] != 0xff) {
    runtime_->clearPeer(index);
    peers_->removePeer(index);
    slots_[index] = 0xff;
    durable_[index] = false;
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

bool ServerAssociations::persist(uint8_t slot, const uint8_t *record,
                                 int size) {
  char key[16];
  slotKey(slot, key);
  int oldSize = config_->getBlobSize(key);
  bool saved = oldSize >= 0 && oldSize <= kMaxRecordSize;
  if (saved &&
      !config_->getBlob(key, reinterpret_cast<char *>(previous_), oldSize))
    return false;
  // Empty replacement deletes the complete logical record. No tombstone.
  bool written =
      size ? config_->setBlob(key, reinterpret_cast<const char *>(record), size)
           : config_->eraseKey(key);
  if (written && config_->commit()) {
    memset(previous_, 0, sizeof(previous_));
    return true;
  }
  // Config backends may stage writes. Restore before any unrelated commit.
  if (saved) {
    config_->setBlob(key, reinterpret_cast<const char *>(previous_), oldSize);
  } else {
    config_->eraseKey(key);
  }
  memset(previous_, 0, sizeof(previous_));
  memset(durable_, 0, sizeof(durable_));
  return false;
}

bool ServerAssociations::readRecord(uint8_t slot, PeerContext *context,
                                    uint32_t *revision, uint8_t *count,
                                    bool *destination) {
  char key[16];
  slotKey(slot, key);
  int size = config_->getBlobSize(key);
  if (size < kSourcePrefix + 2 || size > kMaxRecordSize ||
      !config_->getBlob(key, reinterpret_cast<char *>(record_), size) ||
      record_[0] != 1 || (record_[1] != 1 && record_[1] != 2) ||
      calculateCrc16(record_, size - 2) !=
          (record_[size - 2] | (record_[size - 1] << 8)) ||
      !decodePeerContext(record_ + 2, context))
    return false;
  *destination = record_[1] == 2;
  *revision = getUint32(record_ + 29);
  *count = record_[33];
  int prefix = *destination ? kDestinationPrefix : kSourcePrefix;
  if (*count == 0 || *count > kMaxEntries || size != prefix + 6 * *count + 2 ||
      (*destination && !keyValid(record_ + kSourcePrefix)))
    return false;
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
  if (!available()) return;
  peers_->setOwnershipCheck(owns, this);
  runtime_->setLocalAddress(
      {kNodeIdDevice, static_cast<uint32_t>(identity_->serverDeviceId())});
  for (uint16_t slot = 0; slot < SUPLAN_MAX_PERSISTENT_PEERS; ++slot) {
    PeerContext context = {};
    uint32_t revision = 0;
    uint8_t count = 0;
    bool destination = false;
    if (!readRecord(slot, &context, &revision, &count, &destination)) continue;
    uint8_t index = 0;
    bool added =
        destination
            ? peers_->addPeer(&context, record_ + kSourcePrefix, revision,
                              entries_, count, &index, true)
            : peers_->addPeerFromRoot(&context, identity_->rootKey(), revision,
                                      entries_, count, &index);
    if (added) {
      slots_[index] = slot;
      durable_[index] = true;
    }
  }
  memset(record_, 0, sizeof(record_));
}

void ServerAssociations::identityChanged() {
  if (!available()) return;
  runtime_->setLocalAddress(
      {kNodeIdDevice, static_cast<uint32_t>(identity_->serverDeviceId())});
  for (uint16_t i = 0; i < SUPLAN_MAX_PERSISTENT_PEERS; ++i) {
    auto peer = peers_->get(i);
    if (!peer || slots_[i] == 0xff) continue;
    PeerContext context = {};
    decodePeerContext(peer->contextBytes, &context);
    uint32_t local = static_cast<uint32_t>(identity_->serverDeviceId());
    if ((peer->destination ? context.destination.nodeId
                           : context.source.nodeId) != local ||
        (!peer->destination && context.rootEpoch != identity_->rootEpoch())) {
      clear(i);
    } else if (!owns(this, context,
                     {kResourceTypeDevice, context.source.nodeId})) {
      runtime_->clearPeer(i);
    }
  }
}

uint8_t ServerAssociations::apply(const TSuplaSuplanPeerContext &wire,
                                  uint32_t revision,
                                  const TSuplaSuplanAclEntry *entries,
                                  uint16_t count, bool destination,
                                  const uint8_t *key) {
  if (!available()) return SUPLA_SUPLAN_RESULT_UNSUPPORTED;
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
      memset(previous_, 0, sizeof(previous_));
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
  if (identical && durable_[index]) {
    memset(previous_, 0, sizeof(previous_));
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
  if (slot < 0) {
    for (uint16_t candidate = 0; candidate < SUPLAN_MAX_PERSISTENT_PEERS;
         ++candidate) {
      bool used = false;
      for (uint16_t i = 0; i < SUPLAN_MAX_PERSISTENT_PEERS; ++i) {
        used = used || slots_[i] == candidate;
      }
      if (!used) {
        slot = candidate;
        break;
      }
    }
  }
  // Precompute cryptographic material before the persistence boundary.
  PeerMaterial material = {};
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
  // Delete/invalidate dependent topology first. A crash or later association
  // write failure may leave less topology, never unauthorized mixed state.
  if (destination &&
      !runtime_->authorizationReplacing(context, entries_, count)) {
    memset(record_, 0, sizeof(record_));
    memset(&material, 0, sizeof(material));
    return SUPLA_SUPLAN_RESULT_PERSISTENCE_ERROR;
  }
  busy_ = true;
  bool saved = persist(slot, record_, count ? size : 0);
  memset(record_, 0, sizeof(record_));
  if (!saved) {
    busy_ = false;
    memset(&material, 0, sizeof(material));
    return SUPLA_SUPLAN_RESULT_PERSISTENCE_ERROR;
  }
  bool activated = true;
  if (!count) {
    clear(index);
  } else if (exact) {
    activated = peers_->replaceAcl(index, revision, entries_, count);
    durable_[index] = activated;
    runtime_->authorizationChanged(index);
  } else {
    if (index >= 0) clear(index);
    uint8_t added = 0;
    activated = peers_->addPeer(&context, material.peerKey, revision, entries_,
                                count, &added, destination);
    if (activated) {
      slots_[added] = slot;
      durable_[added] = true;
    }
  }
  identityChanged();
  busy_ = false;
  memset(&material, 0, sizeof(material));
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

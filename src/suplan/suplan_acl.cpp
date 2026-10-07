// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef ARDUINO_ARCH_AVR

#include "suplan_acl.h"

#include <string.h>

#include "suplan_crypto.h"
#include "suplan_wire.h"

namespace Supla {
namespace SupLan {

PeerTable::PeerTable() : peerCount_(0), aclEntryCount_(0) {
  memset(peers_, 0, sizeof(peers_));
  memset(acl_, 0, sizeof(acl_));
}

bool PeerTable::validEntries(const AclEntry *entries,
                             uint8_t entryCount) const {
  if ((entries == nullptr && entryCount != 0) ||
      entryCount > 89) {
    return false;
  }
  const uint8_t allowed = kPermissionRead | kPermissionControl |
      kPermissionAction;
  for (uint8_t i = 0; i < entryCount; ++i) {
    if ((entries[i].resource.type != kResourceTypeChannel &&
         entries[i].resource.type != kResourceTypeDevice) ||
        entries[i].permissions == 0 ||
        (entries[i].permissions & static_cast<uint8_t>(~allowed)) != 0) {
      return false;
    }
    for (uint8_t j = 0; j < i; ++j) {
      if (entries[i].resource.type == entries[j].resource.type &&
          entries[i].resource.id == entries[j].resource.id) {
        return false;
      }
    }
  }
  return true;
}

bool PeerTable::sameEntries(const PeerRecord *peer, const AclEntry *entries,
                            uint8_t entryCount) const {
  if (peer == nullptr || peer->aclCount != entryCount) {
    return false;
  }
  const uint8_t peerIndex = static_cast<uint8_t>(peer - peers_);
  uint16_t candidateIndex = 0;
  for (uint8_t i = 0; i < entryCount; ++i) {
    while (candidateIndex < aclEntryCount_ &&
           acl_[candidateIndex].peerIndex != peerIndex) {
      ++candidateIndex;
    }
    if (candidateIndex >= aclEntryCount_ ||
        acl_[candidateIndex].resourceType != entries[i].resource.type ||
        acl_[candidateIndex].resourceId != entries[i].resource.id ||
        acl_[candidateIndex].permissions != entries[i].permissions) {
      return false;
    }
    ++candidateIndex;
  }
  return true;
}

bool PeerTable::setInitialAcl(PeerRecord *peer, uint32_t revision,
                              const AclEntry *entries, uint8_t entryCount) {
  if (peer == nullptr || !validEntries(entries, entryCount) ||
      static_cast<uint16_t>(aclEntryCount_) + entryCount >
          (SUPLAN_MAX_TOTAL_ACL_ENTRIES + SUPLAN_MAX_TOTAL_EXPECTED_ENTRIES)) {
    return false;
  }
  if (entryCount + this->entryCount(peer->destination) >
      (peer->destination ? SUPLAN_MAX_TOTAL_EXPECTED_ENTRIES :
                           SUPLAN_MAX_TOTAL_ACL_ENTRIES)) {
    return false;
  }
  peer->aclRevision = revision;
  peer->aclCount = entryCount;
  const uint8_t peerIndex = static_cast<uint8_t>(peer - peers_);
  for (uint8_t i = 0; i < entryCount; ++i) {
    acl_[aclEntryCount_].resourceId = entries[i].resource.id;
    acl_[aclEntryCount_].resourceType = entries[i].resource.type;
    acl_[aclEntryCount_].permissions = entries[i].permissions;
    acl_[aclEntryCount_].peerIndex = peerIndex;
    ++aclEntryCount_;
  }
  return true;
}

bool PeerTable::addPeer(const PeerContext *context,
                        const uint8_t peerKey[32],
                        uint32_t aclRevision, const AclEntry *entries,
                        uint8_t entryCount, uint8_t *peerIndex,
                        bool destination) {
  if (context == nullptr || peerKey == nullptr ||
      peerIndex == nullptr ||
      !validPeerContext(context) || peerCount_ >= SUPLAN_MAX_PERSISTENT_PEERS ||
      !validEntries(entries, entryCount) ||
      static_cast<uint16_t>(aclEntryCount_) + entryCount >
          (SUPLAN_MAX_TOTAL_ACL_ENTRIES + SUPLAN_MAX_TOTAL_EXPECTED_ENTRIES)) {
    return false;
  }
  uint8_t candidateContext[kPeerContextSize];
  if (!encodePeerContext(context, candidateContext)) {
    return false;
  }
  for (uint16_t i = 0; i < SUPLAN_MAX_PERSISTENT_PEERS; ++i) {
    if (peers_[i].used && memcmp(peers_[i].contextBytes, candidateContext,
               sizeof(candidateContext)) == 0) {
      return false;
    }
  }
  const uint8_t slot = static_cast<uint8_t>(freeSlot());
  PeerRecord *candidate = &peers_[slot];
  memset(candidate, 0, sizeof(*candidate));
  memcpy(candidate->contextBytes, candidateContext, sizeof(candidateContext));
  candidate->destination = destination;
  PeerMaterial material = {};
  if (!derivePeerMaterialFromKey(context, peerKey, &material) ||
      findByLocator(material.peerLocator) != -1 ||
      !setInitialAcl(candidate, aclRevision, entries, entryCount)) {
    memset(candidate, 0, sizeof(*candidate));
    return false;
  }
  memcpy(candidate->peerKey, peerKey, sizeof(candidate->peerKey));
  memcpy(candidate->peerLocator, material.peerLocator,
         sizeof(candidate->peerLocator));
  candidate->used = true;
  *peerIndex = slot;
  ++peerCount_;
  return true;
}

bool PeerTable::addPeerFromRoot(const PeerContext *context,
                                const uint8_t rootKey[32],
                                uint32_t aclRevision,
                                const AclEntry *entries,
                                uint8_t entryCount, uint8_t *peerIndex) {
  if (context == nullptr || rootKey == nullptr ||
      peerIndex == nullptr || peerCount_ >= SUPLAN_MAX_PERSISTENT_PEERS ||
      !validPeerContext(context) || !validEntries(entries, entryCount) ||
      static_cast<uint16_t>(aclEntryCount_) + entryCount >
          (SUPLAN_MAX_TOTAL_ACL_ENTRIES + SUPLAN_MAX_TOTAL_EXPECTED_ENTRIES)) {
    return false;
  }
  uint8_t candidateContext[kPeerContextSize];
  if (!encodePeerContext(context, candidateContext)) {
    return false;
  }
  for (uint16_t i = 0; i < SUPLAN_MAX_PERSISTENT_PEERS; ++i) {
    if (peers_[i].used && memcmp(peers_[i].contextBytes, candidateContext,
               sizeof(candidateContext)) == 0) {
      return false;
    }
  }
  const uint8_t slot = static_cast<uint8_t>(freeSlot());
  PeerRecord *candidate = &peers_[slot];
  memset(candidate, 0, sizeof(*candidate));
  memcpy(candidate->contextBytes, candidateContext, sizeof(candidateContext));
  PeerMaterial material = {};
  if (!derivePeerMaterial(context, rootKey, &material) ||
      findByLocator(material.peerLocator) != -1 ||
      !setInitialAcl(candidate, aclRevision, entries, entryCount)) {
    memset(candidate, 0, sizeof(*candidate));
    return false;
  }
  memcpy(candidate->peerKey, material.peerKey, sizeof(candidate->peerKey));
  memcpy(candidate->peerLocator, material.peerLocator,
         sizeof(candidate->peerLocator));
  candidate->used = true;
  *peerIndex = slot;
  ++peerCount_;
  return true;
}

bool PeerTable::replaceAcl(uint8_t peerIndex, uint32_t revision,
                           const AclEntry *entries, uint8_t entryCount) {
  PeerRecord *peer = get(peerIndex);
  if (peer == nullptr || !validEntries(entries, entryCount)) {
    return false;
  }
  if (revision < peer->aclRevision) {
    return false;
  }
  if (revision == peer->aclRevision) {
    return sameEntries(peer, entries, entryCount);
  }
  const uint16_t updatedTotal = static_cast<uint16_t>(aclEntryCount_ -
      peer->aclCount) + entryCount;
  if (updatedTotal >
      (SUPLAN_MAX_TOTAL_ACL_ENTRIES + SUPLAN_MAX_TOTAL_EXPECTED_ENTRIES)) {
    return false;
  }
  if (this->entryCount(peer->destination) - peer->aclCount + entryCount >
      (peer->destination ? SUPLAN_MAX_TOTAL_EXPECTED_ENTRIES :
                           SUPLAN_MAX_TOTAL_ACL_ENTRIES)) {
    return false;
  }
  // Compact only ACL entries; peer slots and all runtime indexes stay stable.
  uint16_t output = 0;
  for (uint16_t i = 0; i < aclEntryCount_; ++i) {
    if (acl_[i].peerIndex != peerIndex) {
      acl_[output++] = acl_[i];
    }
  }
  for (uint8_t i = 0; i < entryCount; ++i) {
    acl_[output++] = {entries[i].resource.id, entries[i].resource.type,
                      entries[i].permissions, peerIndex};
  }
  memset(acl_ + output, 0, sizeof(acl_) - output * sizeof(acl_[0]));
  peer->aclCount = entryCount;
  peer->aclRevision = revision;
  aclEntryCount_ = updatedTotal;
  return true;
}

int PeerTable::findByLocator(const uint8_t locator[16]) const {
  if (locator == nullptr) {
    return -1;
  }
  int found = -1;
  for (uint16_t i = 0; i < SUPLAN_MAX_PERSISTENT_PEERS; ++i) {
    PeerContext context = {};
    if (!decodePeerContext(peers_[i].contextBytes, &context)) {
      continue;
    }
    if (memcmp(peers_[i].peerLocator, locator, kPeerLocatorSize) == 0) {
      if (found != -1) {
        return -2;
      }
      found = i;
    }
  }
  return found;
}

bool PeerTable::hasActiveGrants(uint8_t peerIndex) const {
  const PeerRecord *peer = get(peerIndex);
  return peer != nullptr && peer->aclCount != 0 && runtimeEligible(peerIndex);
}

bool PeerTable::runtimeEligible(uint8_t peerIndex) const {
  const PeerRecord *peer = get(peerIndex);
  PeerContext context = {};
  return peer && decodePeerContext(peer->contextBytes, &context) &&
      owns(peerIndex, {kResourceTypeDevice, context.source.nodeId});
}

void PeerTable::setOwnershipCheck(OwnershipCheck check, void *context) {
  ownershipCheck_ = check;
  ownershipContext_ = context;
}

bool PeerTable::owns(uint8_t peerIndex, const ResourceId &resource) const {
  const PeerRecord *peer = get(peerIndex);
  PeerContext context = {};
  if (!peer || !decodePeerContext(peer->contextBytes, &context)) {
    return false;
  }
  if (ownershipCheck_) {
    return ownershipCheck_(ownershipContext_, context, resource);
  }
  if (resource.type == kResourceTypeDevice) {
    return resource.id == context.source.nodeId;
  }
  if (resource.type != kResourceTypeChannel) {
    return false;
  }
  return context.authorityType != kAuthorityLocal ||
      (resource.id >> 16) == context.source.nodeId;
}

uint8_t PeerTable::effectivePermissions(uint8_t peerIndex,
                                       const ResourceId &resource) const {
  const PeerRecord *peer = get(peerIndex);
  if (!peer || !owns(peerIndex, resource)) {
    return 0;
  }
  PeerContext context = {};
  if (!decodePeerContext(peer->contextBytes, &context)) {
    return 0;
  }
  uint8_t permissions = 0;
  for (uint16_t i = 0; i < aclEntryCount_; ++i) {
    const PeerAclRecord &entry = acl_[i];
    if (entry.peerIndex == peerIndex &&
        ((entry.resourceType == resource.type &&
          entry.resourceId == resource.id)
         || (resource.type == kResourceTypeChannel &&
             entry.resourceType == kResourceTypeDevice &&
             entry.resourceId == context.source.nodeId))) {
      permissions |= entry.permissions;
    }
  }
  if (permissions & kPermissionControl) {
    permissions |= kPermissionRead;
  }
  return permissions;
}

bool PeerTable::authorize(uint8_t peerIndex, const ResourceId &resource,
                          uint8_t permission) const {
  return permission != 0 &&
      (effectivePermissions(peerIndex, resource) & permission) == permission;
}

int PeerTable::freeSlot() const {
  for (uint16_t i = 0; i < SUPLAN_MAX_PERSISTENT_PEERS; ++i) {
    if (!peers_[i].used) {
      return i;
    }
  }
  return -1;
}

bool PeerTable::removePeer(uint8_t peerIndex) {
  if (!get(peerIndex)) {
    return false;
  }
  uint16_t output = 0;
  for (uint16_t i = 0; i < aclEntryCount_; ++i) {
    if (acl_[i].peerIndex != peerIndex) {
      acl_[output++] = acl_[i];
    }
  }
  memset(acl_ + output, 0, sizeof(acl_) - output * sizeof(acl_[0]));
  aclEntryCount_ = output;
  memset(&peers_[peerIndex], 0, sizeof(peers_[peerIndex]));
  --peerCount_;
  return true;
}

PeerRecord *PeerTable::get(uint8_t peerIndex) {
  if (peerIndex >= SUPLAN_MAX_PERSISTENT_PEERS || !peers_[peerIndex].used) {
    return nullptr;
  }
  return &peers_[peerIndex];
}

const PeerRecord *PeerTable::get(uint8_t peerIndex) const {
  if (peerIndex >= SUPLAN_MAX_PERSISTENT_PEERS || !peers_[peerIndex].used) {
    return nullptr;
  }
  return &peers_[peerIndex];
}

bool PeerTable::materialFor(uint8_t peerIndex,
                            PeerMaterial *material) const {
  const PeerRecord *peer = get(peerIndex);
  PeerContext context = {};
  return peer != nullptr && material != nullptr &&
      hasActiveGrants(peerIndex) &&
      decodePeerContext(peer->contextBytes, &context) &&
      derivePeerMaterialFromKey(&context, peer->peerKey, material);
}

uint16_t PeerTable::entryCount(bool destination) const {
  uint16_t count = 0;
  for (uint16_t i = 0; i < SUPLAN_MAX_PERSISTENT_PEERS; ++i) {
    if (peers_[i].used && peers_[i].destination == destination) {
      count += peers_[i].aclCount;
    }
  }
  return count;
}

uint8_t PeerTable::size() const {
  return peerCount_;
}

uint16_t PeerTable::aclEntryCount() const {
  return aclEntryCount_;
}

}  // namespace SupLan
}  // namespace Supla

#endif  // !ARDUINO_ARCH_AVR

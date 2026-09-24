// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

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
      entryCount > SUPLAN_MAX_TOTAL_ACL_ENTRIES) {
    return false;
  }
  const uint8_t allowed = kPermissionRead | kPermissionControl |
      kPermissionAction;
  for (uint8_t i = 0; i < entryCount; ++i) {
    if (entries[i].resource.type != kResourceTypeChannel ||
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
  uint8_t candidateIndex = 0;
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
          SUPLAN_MAX_TOTAL_ACL_ENTRIES) {
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

bool PeerTable::addPeer(CryptoPort *crypto, const PeerContext *context,
                        const uint8_t peerKey[32],
                        uint32_t aclRevision, const AclEntry *entries,
                        uint8_t entryCount, uint8_t *peerIndex) {
  if (crypto == nullptr || context == nullptr || peerKey == nullptr ||
      peerIndex == nullptr ||
      !validPeerContext(context) || peerCount_ >= SUPLAN_MAX_PERSISTENT_PEERS ||
      !validEntries(entries, entryCount) ||
      static_cast<uint16_t>(aclEntryCount_) + entryCount >
          SUPLAN_MAX_TOTAL_ACL_ENTRIES) {
    return false;
  }
  uint8_t candidateContext[kPeerContextSize];
  if (!encodePeerContext(context, candidateContext)) {
    return false;
  }
  for (uint8_t i = 0; i < peerCount_; ++i) {
    if (memcmp(peers_[i].contextBytes, candidateContext,
               sizeof(candidateContext)) == 0) {
      return false;
    }
  }
  PeerRecord *candidate = &peers_[peerCount_];
  memset(candidate, 0, sizeof(*candidate));
  memcpy(candidate->contextBytes, candidateContext, sizeof(candidateContext));
  PeerMaterial material = {};
  if (!derivePeerMaterialFromKey(crypto, context, peerKey, &material) ||
      findByLocator(material.peerLocator) != -1 ||
      !setInitialAcl(candidate, aclRevision, entries, entryCount)) {
    memset(candidate, 0, sizeof(*candidate));
    return false;
  }
  memcpy(candidate->peerKey, peerKey, sizeof(candidate->peerKey));
  memcpy(candidate->peerLocator, material.peerLocator,
         sizeof(candidate->peerLocator));
  candidate->used = true;
  *peerIndex = peerCount_++;
  return true;
}

bool PeerTable::addPeerFromRoot(CryptoPort *crypto,
                                const PeerContext *context,
                                const uint8_t rootKey[32],
                                uint32_t aclRevision,
                                const AclEntry *entries,
                                uint8_t entryCount, uint8_t *peerIndex) {
  if (crypto == nullptr || context == nullptr || rootKey == nullptr ||
      peerIndex == nullptr || peerCount_ >= SUPLAN_MAX_PERSISTENT_PEERS ||
      !validPeerContext(context) || !validEntries(entries, entryCount) ||
      static_cast<uint16_t>(aclEntryCount_) + entryCount >
          SUPLAN_MAX_TOTAL_ACL_ENTRIES) {
    return false;
  }
  uint8_t candidateContext[kPeerContextSize];
  if (!encodePeerContext(context, candidateContext)) {
    return false;
  }
  for (uint8_t i = 0; i < peerCount_; ++i) {
    if (memcmp(peers_[i].contextBytes, candidateContext,
               sizeof(candidateContext)) == 0) {
      return false;
    }
  }
  PeerRecord *candidate = &peers_[peerCount_];
  memset(candidate, 0, sizeof(*candidate));
  memcpy(candidate->contextBytes, candidateContext, sizeof(candidateContext));
  PeerMaterial material = {};
  if (!derivePeerMaterial(crypto, context, rootKey, &material) ||
      findByLocator(material.peerLocator) != -1 ||
      !setInitialAcl(candidate, aclRevision, entries, entryCount)) {
    memset(candidate, 0, sizeof(*candidate));
    return false;
  }
  memcpy(candidate->peerKey, material.peerKey, sizeof(candidate->peerKey));
  memcpy(candidate->peerLocator, material.peerLocator,
         sizeof(candidate->peerLocator));
  candidate->used = true;
  *peerIndex = peerCount_++;
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
  if (updatedTotal > SUPLAN_MAX_TOTAL_ACL_ENTRIES) {
    return false;
  }
  PeerAclRecord compacted[SUPLAN_MAX_TOTAL_ACL_ENTRIES];
  uint8_t compactedCount = 0;
  for (uint8_t i = 0; i < aclEntryCount_; ++i) {
    if (acl_[i].peerIndex != peerIndex) {
      compacted[compactedCount++] = acl_[i];
    }
  }
  for (uint8_t i = 0; i < entryCount; ++i) {
    compacted[compactedCount].resourceId = entries[i].resource.id;
    compacted[compactedCount].resourceType = entries[i].resource.type;
    compacted[compactedCount].permissions = entries[i].permissions;
    compacted[compactedCount].peerIndex = peerIndex;
    ++compactedCount;
  }
  memcpy(acl_, compacted, compactedCount * sizeof(PeerAclRecord));
  if (compactedCount < sizeof(acl_) / sizeof(acl_[0])) {
    memset(acl_ + compactedCount, 0,
           (sizeof(acl_) / sizeof(acl_[0]) - compactedCount) *
               sizeof(PeerAclRecord));
  }
  peer->aclCount = entryCount;
  peer->aclRevision = revision;
  aclEntryCount_ = static_cast<uint8_t>(updatedTotal);
  return true;
}

int PeerTable::findByLocator(const uint8_t locator[16]) const {
  if (locator == nullptr) {
    return -1;
  }
  int found = -1;
  for (uint8_t i = 0; i < peerCount_; ++i) {
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
  return peer != nullptr && peer->aclCount != 0;
}

bool PeerTable::authorize(uint8_t peerIndex, const ResourceId &resource,
                          uint8_t permission) const {
  if (permission == 0 ||
      (permission & static_cast<uint8_t>(~(kPermissionRead |
                                          kPermissionControl |
                                          kPermissionAction))) != 0) {
    return false;
  }
  const PeerRecord *peer = get(peerIndex);
  if (peer == nullptr) {
    return false;
  }
  for (uint8_t i = 0; i < aclEntryCount_; ++i) {
    if (acl_[i].peerIndex != peerIndex) {
      continue;
    }
    const PeerAclRecord &entry = acl_[i];
    if (entry.resourceType != resource.type ||
        entry.resourceId != resource.id) {
      continue;
    }
    if (permission == kPermissionRead) {
      return (entry.permissions &
              static_cast<uint8_t>(kPermissionRead | kPermissionControl)) != 0;
    }
    return (entry.permissions & permission) == permission;
  }
  return false;
}

PeerRecord *PeerTable::get(uint8_t peerIndex) {
  if (peerIndex >= peerCount_ || !peers_[peerIndex].used) {
    return nullptr;
  }
  return &peers_[peerIndex];
}

const PeerRecord *PeerTable::get(uint8_t peerIndex) const {
  if (peerIndex >= peerCount_ || !peers_[peerIndex].used) {
    return nullptr;
  }
  return &peers_[peerIndex];
}

bool PeerTable::materialFor(CryptoPort *crypto, uint8_t peerIndex,
                            PeerMaterial *material) const {
  const PeerRecord *peer = get(peerIndex);
  PeerContext context = {};
  return crypto != nullptr && peer != nullptr && material != nullptr &&
      decodePeerContext(peer->contextBytes, &context) &&
      derivePeerMaterialFromKey(crypto, &context, peer->peerKey, material);
}

uint8_t PeerTable::size() const {
  return peerCount_;
}

uint8_t PeerTable::aclEntryCount() const {
  return aclEntryCount_;
}

}  // namespace SupLan
}  // namespace Supla

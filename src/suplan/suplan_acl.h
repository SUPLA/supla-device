// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLAN_SUPLAN_ACL_H_
#define SRC_SUPLAN_SUPLAN_ACL_H_

#ifndef ARDUINO_ARCH_AVR

#include <stdint.h>

#include "suplan_types.h"

namespace Supla {
namespace SupLan {

struct AclEntry {
  ResourceId resource;
  uint8_t permissions;
};

struct PeerRecord {
  bool used;
  uint8_t contextBytes[kPeerContextSize];
  uint8_t peerKey[kKeySize];
  uint8_t peerLocator[kPeerLocatorSize];
  uint32_t aclRevision;
  uint32_t lastLocateReplyMs;
  Endpoint endpoint;
  uint8_t aclCount;
  bool hasLocateReplied;
  uint8_t endpointState;
  bool destination;
};

enum PeerEndpointState : uint8_t {
  kPeerEndpointNone = 0,
  kPeerEndpointLocateCandidate = 1,
  kPeerEndpointAuthenticated = 2,
};

struct PeerAclRecord {
  uint32_t resourceId;
  uint8_t resourceType;
  uint8_t permissions;
  uint8_t peerIndex;
};

class PeerTable {
 public:
  PeerTable();
  bool addPeer(const PeerContext *context,
               const uint8_t peerKey[32],
               uint32_t aclRevision, const AclEntry *entries,
               uint8_t entryCount, uint8_t *peerIndex,
               bool destination = false);
  bool addPeerFromRoot(const PeerContext *context,
                       const uint8_t rootKey[32], uint32_t aclRevision,
                       const AclEntry *entries, uint8_t entryCount,
                       uint8_t *peerIndex);
  bool replaceAcl(uint8_t peerIndex, uint32_t revision,
                  const AclEntry *entries, uint8_t entryCount);
  int findByLocator(const uint8_t locator[16]) const;
  bool hasActiveGrants(uint8_t peerIndex) const;
  bool runtimeEligible(uint8_t peerIndex) const;
  bool authorize(uint8_t peerIndex, const ResourceId &resource,
                 uint8_t permission) const;
  PeerRecord *get(uint8_t peerIndex);
  const PeerRecord *get(uint8_t peerIndex) const;
  bool materialFor(uint8_t peerIndex,
                   PeerMaterial *material) const;
  uint8_t size() const;
  uint16_t aclEntryCount() const;
  // Slots are stable. size() counts occupied slots, never bounds iteration.
  // Caller must clear associated Runtime state before removal or slot reuse.
  bool removePeer(uint8_t peerIndex);
  int freeSlot() const;
  bool sameAcl(uint8_t index, const AclEntry *entries, uint8_t count) const {
    return sameEntries(get(index), entries, count);
  }
  uint8_t effectivePermissions(uint8_t peerIndex,
                               const ResourceId &resource) const;
  typedef bool (*OwnershipCheck)(void *, const PeerContext &,
                                 const ResourceId &);
  void setOwnershipCheck(OwnershipCheck check, void *context);
  bool owns(uint8_t peerIndex, const ResourceId &resource) const;
  uint16_t entryCount(bool destination) const;

 private:
  bool setInitialAcl(PeerRecord *peer, uint32_t revision,
                     const AclEntry *entries, uint8_t entryCount);
  bool validEntries(const AclEntry *entries, uint8_t entryCount) const;
  bool sameEntries(const PeerRecord *peer, const AclEntry *entries,
                   uint8_t entryCount) const;

  PeerRecord peers_[SUPLAN_MAX_PERSISTENT_PEERS];
  PeerAclRecord acl_[SUPLAN_MAX_TOTAL_ACL_ENTRIES +
                    SUPLAN_MAX_TOTAL_EXPECTED_ENTRIES];
  uint8_t peerCount_;
  uint16_t aclEntryCount_;
  OwnershipCheck ownershipCheck_ = nullptr;
  void *ownershipContext_ = nullptr;
};

}  // namespace SupLan
}  // namespace Supla

#endif  // !ARDUINO_ARCH_AVR

#endif  // SRC_SUPLAN_SUPLAN_ACL_H_

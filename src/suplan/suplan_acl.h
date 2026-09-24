// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLAN_SUPLAN_ACL_H_
#define SRC_SUPLAN_SUPLAN_ACL_H_

#include <stdint.h>

#include "suplan_ports.h"
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
  uint8_t aclCount;
  uint32_t lastLocateReplyMs;
  bool hasLocateReplied;
  Endpoint endpoint;
  uint8_t endpointState;
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
  bool addPeer(CryptoPort *crypto, const PeerContext *context,
               const uint8_t peerKey[32],
               uint32_t aclRevision, const AclEntry *entries,
               uint8_t entryCount, uint8_t *peerIndex);
  bool addPeerFromRoot(CryptoPort *crypto, const PeerContext *context,
                       const uint8_t rootKey[32], uint32_t aclRevision,
                       const AclEntry *entries, uint8_t entryCount,
                       uint8_t *peerIndex);
  bool replaceAcl(uint8_t peerIndex, uint32_t revision,
                  const AclEntry *entries, uint8_t entryCount);
  int findByLocator(const uint8_t locator[16]) const;
  bool hasActiveGrants(uint8_t peerIndex) const;
  bool authorize(uint8_t peerIndex, const ResourceId &resource,
                 uint8_t permission) const;
  PeerRecord *get(uint8_t peerIndex);
  const PeerRecord *get(uint8_t peerIndex) const;
  bool materialFor(CryptoPort *crypto, uint8_t peerIndex,
                   PeerMaterial *material) const;
  uint8_t size() const;
  uint8_t aclEntryCount() const;

 private:
  bool setInitialAcl(PeerRecord *peer, uint32_t revision,
                     const AclEntry *entries, uint8_t entryCount);
  bool validEntries(const AclEntry *entries, uint8_t entryCount) const;
  bool sameEntries(const PeerRecord *peer, const AclEntry *entries,
                   uint8_t entryCount) const;

  PeerRecord peers_[SUPLAN_MAX_PERSISTENT_PEERS];
  PeerAclRecord acl_[SUPLAN_MAX_TOTAL_ACL_ENTRIES];
  uint8_t peerCount_;
  uint8_t aclEntryCount_;
};

}  // namespace SupLan
}  // namespace Supla

#endif  // SRC_SUPLAN_SUPLAN_ACL_H_

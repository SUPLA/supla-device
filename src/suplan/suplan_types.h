// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLAN_SUPLAN_TYPES_H_
#define SRC_SUPLAN_SUPLAN_TYPES_H_

#include <stdint.h>

#include "suplan_config.h"

namespace Supla {
namespace SupLan {

enum AuthorityType : uint8_t {
  kAuthorityInvalid = 0,
  kAuthorityServer = 1,
  kAuthorityLocal = 2,
};

enum NodeIdNamespace : uint8_t {
  kNodeIdInvalid = 0,
  kNodeIdDevice = 1,
  kNodeIdClient = 2,
  kNodeIdLocal = 3,
};

struct NodeAddress {
  uint8_t nameSpace;
  uint32_t nodeId;
};

struct PeerContext {
  uint8_t authorityType;
  uint64_t authorityId;
  NodeAddress source;
  NodeAddress destination;
  uint32_t rootEpoch;
  uint32_t peerGeneration;
};

struct ResourceId {
  uint8_t type;
  uint32_t id;
};

struct Endpoint {
  uint32_t address;  // Opaque IPv4 address in network byte order.
  uint16_t port;     // Host byte order at the portable-core boundary.
};

enum DataDirection : uint8_t {
  kDirectionInitiatorToResponder = 0,
  kDirectionResponderToInitiator = 1,
};

struct DirectionalKeys {
  uint8_t trafficKey[16];
  uint8_t noncePrefix[kNoncePrefixSize];
};

struct SessionKeys {
  DirectionalKeys initiatorToResponder;
  DirectionalKeys responderToInitiator;
};

struct PeerMaterial {
  uint8_t contextHash[kSha256Size];
  uint8_t peerKey[kKeySize];
  uint8_t peerLocator[kPeerLocatorSize];
  uint8_t locateMacKey[kKeySize];
  uint8_t initMacKey[kKeySize];
  uint8_t acceptMacKey[kKeySize];
};

struct PoolUsage {
  uint16_t used;
  uint16_t maximum;
  uint16_t highWater;
};

struct Diagnostics {
  uint32_t locateTx;
  uint32_t locateRx;
  uint32_t locateReplyTx;
  uint32_t locateReplyRx;
  uint32_t sessionInitTx;
  uint32_t sessionInitRx;
  uint32_t sessionAcceptTx;
  uint32_t sessionAcceptRx;
  uint32_t sessionEstablished;
  uint32_t sessionReplaced;
  uint32_t sessionRejectTx;
  uint32_t sessionRejectRx;
  uint32_t sessionUnknownDrop;
  uint32_t dataTx;
  uint32_t dataRx;
  uint32_t dataAuthFail;
  uint32_t dataReplayDrop;
  uint32_t dataDuplicate;
  uint32_t ackTx;
  uint32_t ackRx;
  uint32_t retryTx;
  uint32_t controlDispatched;
  uint32_t controlDuplicateSuppressed;
  uint32_t readDispatched;
  uint32_t stateNotificationTx;
  uint32_t stateNotificationRx;
  uint32_t actionTx;
  uint32_t actionRx;
  uint32_t fragmentTx;
  uint32_t fragmentRx;
  uint32_t reassemblyStarted;
  uint32_t reassemblyCompleted;
  uint32_t reassemblyExpired;
  uint32_t reassemblyRejected;
  uint32_t aclReject;
  uint32_t resourceNotFound;
  uint32_t invalidLocateDrop;
  uint32_t invalidSessionDrop;
  uint32_t invalidDataDrop;
  uint32_t deferredQueueOverflow;
  uint32_t poolReject;
};

struct PoolDiagnostics {
  PoolUsage peers;
  PoolUsage aclEntries;
  PoolUsage sessions;
  PoolUsage pending;
  PoolUsage locates;
  PoolUsage interests;
  PoolUsage reassembly;
  PoolUsage retries;
  PoolUsage deferredEvents;
  uint32_t workspaceBytes;
};

}  // namespace SupLan
}  // namespace Supla

#endif  // SRC_SUPLAN_SUPLAN_TYPES_H_

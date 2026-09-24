// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLAN_SUPLAN_RUNTIME_H_
#define SRC_SUPLAN_SUPLAN_RUNTIME_H_

#include <stddef.h>
#include <stdint.h>

#include "suplan_acl.h"
#include "suplan_data.h"
#include "suplan_fragment.h"
#include "suplan_session.h"

namespace Supla {
namespace SupLan {

static const uint32_t kSuplaCallDeviceChannelValueChangedC = 103;
static const uint32_t kSuplaCallDeviceChannelExtendedValueChanged = 105;
static const uint32_t kSuplaCallChannelSetValue = 110;
static const uint32_t kSuplaCallActionTrigger = 700;

class ApplicationPort {
 public:
  virtual ~ApplicationPort() {}
  // Returns false and sets eventOnly for resources without readable state.
  virtual bool readResource(const ResourceId &resource, bool *eventOnly,
                            uint8_t *payload, size_t capacity,
                            size_t *payloadLength) = 0;
  virtual uint8_t dispatchControl(
      const ResourceId &resource, uint32_t messageType,
      const uint8_t *payload, size_t payloadLength) = 0;
  virtual void receiveState(uint8_t peerIndex, const ResourceId &resource,
                            uint32_t messageType, const uint8_t *payload,
                            size_t payloadLength) = 0;
  virtual void receiveAction(uint8_t peerIndex, const ResourceId &resource,
                             uint32_t messageType, const uint8_t *payload,
                             size_t payloadLength) = 0;
  virtual void operationAcknowledged(uint8_t peerIndex,
                                     const ResourceId &resource,
                                     uint32_t sequence, uint8_t result) = 0;
  virtual void readInterestRefreshed(uint8_t, const ResourceId &, bool) {}
};

struct RuntimeTestHooks {
  uint32_t maxDatagramPayload;
  uint8_t dropNextAckTx;
  uint8_t dropNextDataTx;
  uint8_t dropNextFragmentTx;
  uint8_t dropNextSessionAcceptTx;
  uint8_t corruptNextDataTagTx;
  uint8_t corruptNextMacTx;
  uint8_t corruptNextSessionMacTx;
  uint16_t dropFragmentNumber;
  uint8_t dropNextAckRx;
  uint8_t dropNextDataRx;
  uint8_t dropNextFragmentRx;
  uint8_t dropNextSessionAcceptRx;
  uint8_t pauseSessionInitRetries;
  uint8_t failNextSessionAllocation;
};

enum TestFloodKind : uint8_t {
  kFloodInvalidLocate = 1,
  kFloodInvalidSession = 2,
  kFloodInvalidData = 3,
};

class Runtime {
 public:
  Runtime(CryptoPort *crypto, RandomPort *random, DatagramPort *datagrams,
          ApplicationPort *application, PeerTable *peers,
          const NodeAddress &localAddress, uint8_t suplaProtoVersion);

  bool requestRead(uint8_t peerIndex, const ResourceId &resource);
  bool sendControl(uint8_t peerIndex, const ResourceId &resource,
                   const uint8_t *suplaPayload, size_t payloadLength);
  bool publishState(uint8_t peerIndex, const ResourceId &resource,
                    uint32_t messageType, const uint8_t *suplaPayload,
                    size_t payloadLength);
  bool publishStateParts(uint8_t peerIndex, const ResourceId &resource,
                         uint32_t messageType, const uint8_t *prefix,
                         size_t prefixLength, const uint8_t *payload,
                         size_t payloadLength);
  bool publishAction(uint8_t peerIndex, const ResourceId &resource,
                     const uint8_t *suplaPayload, size_t payloadLength);
  bool forgetSession(uint8_t peerIndex);
  void clearEndpoint(uint8_t peerIndex);
  bool startFlood(TestFloodKind kind, uint8_t peerIndex, uint16_t count);
  void resetDiagnostics();
  void iterate();

  const Diagnostics &diagnostics() const;
  PoolDiagnostics poolDiagnostics() const;
  RuntimeTestHooks *testHooks();

 private:
  struct SessionEntry {
    bool used;
    uint8_t peerIndex;
    uint64_t sessionId;
    DirectionalKeys transmit;
    DirectionalKeys receive;
    uint32_t nextTransmitSequence;
    ReplayWindow receiveReplay;
    uint32_t lastActivityMs;
    // ReplayWindow accepts duplicates only inside its 64-packet window. The
    // sequence modulo 64 therefore uniquely selects the cached result while
    // the duplicate remains admissible.
    uint8_t controlResults[64];
  };

  struct PendingHandshake {
    bool used;
    bool initiator;
    uint8_t peerIndex;
    Endpoint endpoint;
    uint8_t initFrame[kSessionInitSize];
    uint8_t acceptFrame[kSessionAcceptSize];
    SessionKeys keys;
    ReplayWindow receiveReplay;
    uint64_t sessionId;
    uint32_t lastTransmitMs;
    uint32_t expiresAtMs;
    uint8_t attempts;
  };

  struct OutstandingLocate {
    bool used;
    uint8_t peerIndex;
    uint8_t nonce[kNonceSize];
    uint32_t expiresAtMs;
  };

  struct RuntimeInterest {
    bool used;
    uint8_t peerIndex;
    ResourceId resource;
    uint32_t lastRefreshMs;
  };

  struct RetryEntry {
    bool used;
    uint8_t peerIndex;
    ResourceId resource;
    uint64_t sessionId;
    uint32_t sequence;
    uint32_t lastTransmitMs;
    uint8_t attempts;
    uint16_t frameLength;
    uint8_t frame[SUPLAN_MAX_RETRY_FRAME_BYTES];
  };

  struct DeferredApplication {
    bool used;
    bool mayEstablishSession;
    uint8_t peerIndex;
    uint8_t length;
    uint8_t data[32];
  };

  struct FloodJob {
    bool active;
    TestFloodKind kind;
    uint8_t peerIndex;
    uint16_t remaining;
    uint8_t invalidSessionFrame[kSessionInitSize];
  };

  static void clearSessionEntry(SessionEntry *session);
  static void clearPendingHandshake(PendingHandshake *pending);

  static bool fragmentDatagram(void *context, const uint8_t *data,
                               size_t length);
  static bool knownSession(void *context, uint64_t sessionId);
  bool isLocalSource(const PeerRecord *peer) const;
  bool isLocalDestination(const PeerRecord *peer) const;
  bool sameNode(const NodeAddress &left, const NodeAddress &right) const;
  int findSession(uint64_t sessionId, const Endpoint &endpoint) const;
  int findPending(uint64_t sessionId, const Endpoint &endpoint) const;
  int findPendingForPeer(uint8_t peerIndex, bool initiator) const;
  int allocatePending();
  int allocateSession(uint8_t peerIndex);
  int findRetry(uint8_t peerIndex, uint64_t sessionId,
                uint32_t sequence) const;
  int findFreeRetry() const;
  int findFreeInterest() const;
  void updatePoolHighWater();
  void startLocate(uint8_t peerIndex);
  void startHandshake(uint8_t peerIndex);
  bool sendRaw(const Endpoint &endpoint, const uint8_t *data, size_t length,
               uint8_t frameKind);
  bool sendProtected(uint8_t peerIndex, const DirectionalKeys *transmitKeys,
                     uint64_t sessionId, uint32_t *nextTransmitSequence,
                     uint32_t *lastActivityMs,
                     const uint8_t *applicationData, size_t applicationLength,
                     bool ackRequired, const ResourceId &resource);
  bool sendProtectedToEndpoint(
      uint8_t peerIndex, const Endpoint &endpoint,
      const DirectionalKeys *transmitKeys, uint64_t sessionId,
      uint32_t *nextTransmitSequence, uint32_t *lastActivityMs,
      const uint8_t *applicationData, size_t applicationLength,
      bool ackRequired, const ResourceId &resource);
  bool enqueueApplication(uint8_t peerIndex, const uint8_t *data,
                          size_t length, bool mayEstablishSession);
  void drainDeferred();
  void processDatagram(const Endpoint &source, const uint8_t *data,
                       size_t length);
  void processFlood();
  void processLocate(const Endpoint &source, const uint8_t *data,
                    size_t length);
  void processLocateReply(const Endpoint &source, const uint8_t *data,
                          size_t length);
  void processSessionInit(const Endpoint &source, const uint8_t *data,
                          size_t length);
  void processSessionAccept(const Endpoint &source, const uint8_t *data,
                            size_t length);
  void processProtected(const Endpoint &source, const uint8_t *frame,
                        size_t frameLength);
  void processApplication(SessionEntry *session, uint32_t sequence,
                          const uint8_t *data, size_t length,
                          bool duplicate);
  void sendAck(uint8_t peerIndex, SessionEntry *session, uint32_t sequence,
               uint8_t result);
  bool addInterest(uint8_t peerIndex, const ResourceId &resource);
  bool interested(uint8_t peerIndex, const ResourceId &resource) const;
  bool resourceFromApplication(const ApplicationDataView &application,
                               ResourceId *resource,
                               const uint8_t **payload,
                               size_t *payloadLength) const;
  bool buildResourceApplication(uint8_t messageClass, uint32_t messageType,
                                uint8_t flags,
                                const ResourceId &resource,
                                const uint8_t *payload, size_t payloadLength,
                                size_t *applicationLength);
  void handleNative(uint8_t peerIndex, SessionEntry *session,
                    uint32_t sequence,
                    const ApplicationDataView &application, bool duplicate);
  void handleSuplaCall(uint8_t peerIndex, SessionEntry *session,
                       uint32_t sequence,
                       const ApplicationDataView &application,
                       bool duplicate);

  CryptoPort *crypto_;
  RandomPort *random_;
  DatagramPort *datagrams_;
  ApplicationPort *application_;
  PeerTable *peers_;
  NodeAddress localAddress_;
  uint8_t suplaProtoVersion_;
  bool processing_;
  uint32_t nextFrameId_;
  uint16_t fragmentOrdinal_;
  SessionEntry sessions_[SUPLAN_MAX_ACTIVE_SESSIONS];
  PendingHandshake pending_[SUPLAN_MAX_PENDING_HANDSHAKES];
  OutstandingLocate locates_[SUPLAN_MAX_OUTSTANDING_LOCATES];
  RuntimeInterest interests_[SUPLAN_MAX_RUNTIME_INTERESTS];
  RetryEntry retries_[SUPLAN_MAX_RETRY_SLOTS];
  DeferredApplication deferred_[SUPLAN_MAX_DEFERRED_APP_EVENTS];
  FloodJob flood_;
  FragmentSender fragmentSender_;
  FragmentReassembler fragmentReassembler_;
  Diagnostics diagnostics_;
  PoolDiagnostics poolHighWater_;
  RuntimeTestHooks hooks_;
  uint8_t applicationBuffer_[SUPLAN_MAX_APPLICATION_BYTES];
  // Outbound application frames are bounded by MAX_APPLICATION_BYTES. The
  // larger reassembly limit only applies to received frames.
  uint8_t transmitFrame_[SUPLAN_MAX_APPLICATION_BYTES +
                         kProtectedHeaderSize + kAeadTagSize];
  Endpoint fragmentEndpoint_;
};

}  // namespace SupLan
}  // namespace Supla

#endif  // SRC_SUPLAN_SUPLAN_RUNTIME_H_

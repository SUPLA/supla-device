// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "suplan_runtime.h"

#include <string.h>
#include <supla/crypto.h>

#include "suplan_crypto.h"
#include "suplan_wire.h"

namespace Supla {
namespace SupLan {

namespace {
static const uint8_t kResultNotAllowed = 24;
static const uint8_t kResultChannelNotFound = 25;
static const uint8_t kResultTrue = 3;
static const uint32_t kLocateReplyRateLimitMs = 100;
static const uint8_t kRetryReadRequest = 1U << 0;
static const uint8_t kRetryReadExpectsState = 1U << 1;
static const uint8_t kRetryReadStateReceived = 1U << 2;
static const uint8_t kRetryAwaitingReadState = 1U << 3;
// ADR-008: CONTROL must not survive its foreground recovery deadline.
static const uint32_t kControlLifetimeMs = 5000;
static_assert(kAckMaxAttempts <= 3,
              "RetryEntry attempts field supports up to three attempts");
static_assert(kRetryAwaitingReadState < 16,
              "RetryEntry flags field stores four bits");
static const uint8_t kTxKindAck = 1;
static const uint8_t kTxKindData = 2;
static const uint8_t kTxKindSessionAccept = 4;

static bool endpointEqual(const Endpoint &left, const Endpoint &right) {
  return left.address == right.address && left.port == right.port;
}

static bool equalBytesConstantTime(const uint8_t *left, const uint8_t *right,
                                   size_t length) {
  uint8_t difference = 0;
  for (size_t i = 0; i < length; ++i) {
    difference |= static_cast<uint8_t>(left[i] ^ right[i]);
  }
  return difference == 0;
}

static uint32_t getSuplaUint32(const uint8_t input[4]) {
  return static_cast<uint32_t>(input[0]) |
      (static_cast<uint32_t>(input[1]) << 8) |
      (static_cast<uint32_t>(input[2]) << 16) |
      (static_cast<uint32_t>(input[3]) << 24);
}

static bool isActionApplication(const uint8_t *data, size_t length) {
  ApplicationDataView application = {};
  return decodeApplicationData(data, length, &application) &&
      application.messageClass == kMessageClassSuplaCall &&
      application.messageType == kSuplaCallActionTrigger &&
      application.flags == kAckRequired;
}

static bool isControlApplication(const uint8_t *data, size_t length) {
  ApplicationDataView application = {};
  return decodeApplicationData(data, length, &application) &&
      application.messageClass == kMessageClassSuplaCall &&
      application.messageType == kSuplaCallChannelSetValue &&
      application.flags == kAckRequired;
}

static void putActionDeadline(uint8_t *buffer, size_t capacity,
                              uint32_t expiresAtMs) {
  uint8_t *field = buffer + capacity - sizeof(uint32_t);
  field[0] = static_cast<uint8_t>(expiresAtMs);
  field[1] = static_cast<uint8_t>(expiresAtMs >> 8);
  field[2] = static_cast<uint8_t>(expiresAtMs >> 16);
  field[3] = static_cast<uint8_t>(expiresAtMs >> 24);
}

static uint32_t getActionDeadline(const uint8_t *buffer, size_t capacity) {
  return getSuplaUint32(buffer + capacity - sizeof(uint32_t));
}

static_assert(kApplicationHeaderSize + kResourceHeaderSize + 15 +
                  sizeof(uint32_t) + 1 <= 32,
              "Deferred Action Trigger needs deadline metadata slack");
static_assert(kApplicationHeaderSize + kResourceHeaderSize + 15 +
                  kProtectedHeaderSize + kAeadTagSize + sizeof(uint32_t) + 1 <=
                  SUPLAN_MAX_RETRY_FRAME_BYTES,
              "Retry Action Trigger needs deadline metadata slack");
}  // namespace

Runtime::Runtime(CryptoPort *crypto, DatagramPort *datagrams,
                 ApplicationPort *application,
                 PeerTable *peers, const NodeAddress &localAddress,
                 uint8_t suplaProtoVersion)
    : crypto_(crypto), datagrams_(datagrams),
      application_(application), peers_(peers), localAddress_(localAddress),
      suplaProtoVersion_(suplaProtoVersion), processing_(false),
      nextFrameId_(UINT32_C(0xC3000000)), fragmentOrdinal_(0), sessions_(),
      pending_(), locates_(),
      interests_(), retries_(), deferred_(), recovery_(), dependencies_(),
      flood_(), fragmentSender_(),
      fragmentReassembler_(), diagnostics_(), poolHighWater_(), hooks_(),
      applicationBuffer_(), transmitFrame_(),
      fragmentEndpoint_(), dataTransmitObserver_(nullptr),
      dataTransmitContext_(nullptr) {
  for (uint8_t i = 0; i < SUPLAN_MAX_ACTIVE_SESSIONS; ++i) {
    clearSessionEntry(&sessions_[i]);
  }
  for (uint8_t i = 0; i < SUPLAN_MAX_PENDING_HANDSHAKES; ++i) {
    clearPendingHandshake(&pending_[i]);
  }
  memset(locates_, 0, sizeof(locates_));
  memset(interests_, 0, sizeof(interests_));
  memset(retries_, 0, sizeof(retries_));
  memset(deferred_, 0, sizeof(deferred_));
  memset(&flood_, 0, sizeof(flood_));
  memset(&diagnostics_, 0, sizeof(diagnostics_));
  memset(&poolHighWater_, 0, sizeof(poolHighWater_));
  memset(&hooks_, 0, sizeof(hooks_));
  hooks_.maxDatagramPayload = static_cast<uint32_t>(kMaxDatagramPayload);
  updatePoolHighWater();
}

void Runtime::clearSessionEntry(SessionEntry *session) {
  if (session == nullptr) return;
  session->used = false;
  session->peerIndex = 0;
  session->sessionId = 0;
  session->peerRxMaxReassembledFrame = 0;
  memset(&session->transmit, 0, sizeof(session->transmit));
  memset(&session->receive, 0, sizeof(session->receive));
  session->nextTransmitSequence = 0;
  session->receiveReplay.reset();
  session->lastActivityMs = 0;
  memset(session->ackResults, 0, sizeof(session->ackResults));
}

void Runtime::clearPendingHandshake(PendingHandshake *pending) {
  if (pending == nullptr) return;
  pending->used = false;
  pending->initiator = false;
  pending->peerIndex = 0;
  pending->endpoint = Endpoint();
  memset(pending->initFrame, 0, sizeof(pending->initFrame));
  memset(pending->acceptFrame, 0, sizeof(pending->acceptFrame));
  memset(&pending->keys, 0, sizeof(pending->keys));
  pending->receiveReplay.reset();
  pending->sessionId = 0;
  pending->peerRxMaxReassembledFrame = 0;
  pending->lastTransmitMs = 0;
  pending->expiresAtMs = 0;
  pending->attempts = 0;
}

void Runtime::clearRetryEntry(RetryEntry *retry) {
  if (retry != nullptr) {
    memset(retry, 0, sizeof(*retry));
  }
}

void Runtime::setDataTransmitObserver(DataTransmitObserver observer,
                                      void *context) {
  dataTransmitObserver_ = observer;
  dataTransmitContext_ = context;
}

bool Runtime::sameNode(const NodeAddress &left,
                      const NodeAddress &right) const {
  return left.nameSpace == right.nameSpace && left.nodeId == right.nodeId;
}

bool Runtime::isLocalSource(const PeerRecord *peer) const {
  PeerContext context = {};
  return peer != nullptr && decodePeerContext(peer->contextBytes, &context) &&
      sameNode(context.source, localAddress_);
}

bool Runtime::isLocalDestination(const PeerRecord *peer) const {
  PeerContext context = {};
  return peer != nullptr && decodePeerContext(peer->contextBytes, &context) &&
      sameNode(context.destination, localAddress_);
}

int Runtime::findSession(uint64_t sessionId, const Endpoint &endpoint) const {
  for (uint8_t i = 0; i < SUPLAN_MAX_ACTIVE_SESSIONS; ++i) {
    if (!sessions_[i].used || sessions_[i].sessionId != sessionId) {
      continue;
    }
    const PeerRecord *peer = peers_->get(sessions_[i].peerIndex);
    if (peer != nullptr &&
        peer->endpointState == kPeerEndpointAuthenticated &&
        endpointEqual(peer->endpoint, endpoint)) {
      return i;
    }
  }
  return -1;
}

int Runtime::findPending(uint64_t sessionId,
                         const Endpoint &endpoint) const {
  for (uint8_t i = 0; i < SUPLAN_MAX_PENDING_HANDSHAKES; ++i) {
    if (pending_[i].used && !pending_[i].initiator &&
        pending_[i].sessionId == sessionId &&
        endpointEqual(pending_[i].endpoint, endpoint)) {
      return i;
    }
  }
  return -1;
}

int Runtime::findPendingForPeer(uint8_t peerIndex, bool initiator) const {
  for (uint8_t i = 0; i < SUPLAN_MAX_PENDING_HANDSHAKES; ++i) {
    if (pending_[i].used && pending_[i].peerIndex == peerIndex &&
        pending_[i].initiator == initiator) {
      return i;
    }
  }
  return -1;
}

int Runtime::allocatePending() {
  for (uint8_t i = 0; i < SUPLAN_MAX_PENDING_HANDSHAKES; ++i) {
    if (!pending_[i].used) {
      clearPendingHandshake(&pending_[i]);
      pending_[i].used = true;
      updatePoolHighWater();
      return i;
    }
  }
  ++diagnostics_.poolReject;
  return -1;
}

int Runtime::allocateSession(uint8_t peerIndex) {
  if (hooks_.failNextSessionAllocation != 0) {
    --hooks_.failNextSessionAllocation;
    ++diagnostics_.poolReject;
    return -1;
  }
  int selected = -1;
  for (uint8_t i = 0; i < SUPLAN_MAX_ACTIVE_SESSIONS; ++i) {
    if (sessions_[i].used && sessions_[i].peerIndex == peerIndex) {
      selected = i;
      ++diagnostics_.sessionReplaced;
      for (uint8_t r = 0; r < SUPLAN_MAX_RETRY_SLOTS; ++r) {
        if (retries_[r].isUsed() &&
            retries_[r].sessionId == sessions_[i].sessionId) {
          clearRetryEntry(&retries_[r]);
        }
      }
      break;
    }
    if (!sessions_[i].used && selected < 0) {
      selected = i;
    }
  }
  if (selected < 0) {
    uint32_t oldestAge = 0;
    const uint32_t now = datagrams_->nowMs();
    for (uint8_t i = 0; i < SUPLAN_MAX_ACTIVE_SESSIONS; ++i) {
      bool busy = false;
      for (uint8_t r = 0; r < SUPLAN_MAX_RETRY_SLOTS; ++r) {
        if (retries_[r].isUsed() &&
            retries_[r].sessionId == sessions_[i].sessionId) {
          busy = true;
        }
      }
      const uint32_t age = static_cast<uint32_t>(now -
                                                 sessions_[i].lastActivityMs);
      if (sessions_[i].used && !busy && (selected < 0 || age > oldestAge)) {
        selected = i;
        oldestAge = age;
      }
    }
  }
  if (selected < 0) {
    ++diagnostics_.poolReject;
    return -1;
  }
  if (sessions_[selected].used && sessions_[selected].peerIndex != peerIndex) {
    const uint64_t evictedSession = sessions_[selected].sessionId;
    for (uint8_t r = 0; r < SUPLAN_MAX_RETRY_SLOTS; ++r) {
      if (retries_[r].isUsed() &&
          retries_[r].sessionId == evictedSession) {
        clearRetryEntry(&retries_[r]);
      }
    }
  }
  clearSessionEntry(&sessions_[selected]);
  sessions_[selected].used = true;
  sessions_[selected].peerIndex = peerIndex;
  updatePoolHighWater();
  return selected;
}

int Runtime::findRetry(uint8_t peerIndex, uint64_t sessionId,
                       uint32_t sequence) const {
  for (uint8_t i = 0; i < SUPLAN_MAX_RETRY_SLOTS; ++i) {
    if (retries_[i].isUsed() && retries_[i].peerIndex == peerIndex &&
        retries_[i].sessionId == sessionId &&
        retries_[i].sequence == sequence) {
      return i;
    }
  }
  return -1;
}

int Runtime::findFreeRetry() const {
  for (uint8_t i = 0; i < SUPLAN_MAX_RETRY_SLOTS; ++i) {
    if (!retries_[i].isUsed()) {
      return i;
    }
  }
  return -1;
}

int Runtime::findFreeInterest() const {
  for (uint8_t i = 0; i < SUPLAN_MAX_RUNTIME_INTERESTS; ++i) {
    if (!interests_[i].used) {
      return i;
    }
  }
  return -1;
}

void Runtime::updatePoolHighWater() {
  if (peers_ == nullptr) {
    return;
  }
  const PoolUsage current[] = {
      {peers_->size(), SUPLAN_MAX_PERSISTENT_PEERS, 0},
      {peers_->aclEntryCount(), SUPLAN_MAX_TOTAL_ACL_ENTRIES, 0},
      {0, SUPLAN_MAX_ACTIVE_SESSIONS, 0},
      {0, SUPLAN_MAX_PENDING_HANDSHAKES, 0},
      {0, SUPLAN_MAX_OUTSTANDING_LOCATES, 0},
      {0, SUPLAN_MAX_RUNTIME_INTERESTS, 0},
      {static_cast<uint16_t>(fragmentReassembler_.active() ? 1 : 0),
       SUPLAN_MAX_REASSEMBLY_SLOTS, 0},
      {0, SUPLAN_MAX_RETRY_SLOTS, 0},
      {0, SUPLAN_MAX_DEFERRED_APP_EVENTS, 0}};
  PoolUsage *high[] = {&poolHighWater_.peers, &poolHighWater_.aclEntries,
                       &poolHighWater_.sessions, &poolHighWater_.pending,
                       &poolHighWater_.locates, &poolHighWater_.interests,
                       &poolHighWater_.reassembly, &poolHighWater_.retries,
                       &poolHighWater_.deferredEvents};
  uint16_t used[9] = {};
  used[0] = current[0].used;
  used[1] = current[1].used;
  for (uint8_t i = 0; i < SUPLAN_MAX_ACTIVE_SESSIONS; ++i) {
    used[2] += sessions_[i].used ? 1 : 0;
  }
  for (uint8_t i = 0; i < SUPLAN_MAX_PENDING_HANDSHAKES; ++i) {
    used[3] += pending_[i].used ? 1 : 0;
  }
  for (uint8_t i = 0; i < SUPLAN_MAX_OUTSTANDING_LOCATES; ++i) {
    used[4] += locates_[i].used ? 1 : 0;
  }
  for (uint8_t i = 0; i < SUPLAN_MAX_RUNTIME_INTERESTS; ++i) {
    used[5] += interests_[i].used ? 1 : 0;
  }
  used[6] = fragmentReassembler_.active() ? 1 : 0;
  for (uint8_t i = 0; i < SUPLAN_MAX_RETRY_SLOTS; ++i) {
    used[7] += retries_[i].isUsed() ? 1 : 0;
  }
  for (uint8_t i = 0; i < SUPLAN_MAX_DEFERRED_APP_EVENTS; ++i) {
    used[8] += deferred_[i].used ? 1 : 0;
  }
  for (uint8_t i = 0; i < 9; ++i) {
    high[i]->used = used[i];
    high[i]->maximum = current[i].maximum;
    if (used[i] > high[i]->highWater) {
      high[i]->highWater = used[i];
    }
  }
}

PoolDiagnostics Runtime::poolDiagnostics() const {
  PoolDiagnostics result = poolHighWater_;
  result.peers.used = peers_ == nullptr ? 0 : peers_->size();
  result.peers.maximum = SUPLAN_MAX_PERSISTENT_PEERS;
  result.aclEntries.used = peers_ == nullptr ? 0 : peers_->aclEntryCount();
  result.aclEntries.maximum = SUPLAN_MAX_TOTAL_ACL_ENTRIES;
  result.sessions.used = 0;
  result.pending.used = 0;
  result.locates.used = 0;
  result.interests.used = 0;
  result.reassembly.used = fragmentReassembler_.active() ? 1 : 0;
  result.retries.used = 0;
  result.deferredEvents.used = 0;
  for (uint8_t i = 0; i < SUPLAN_MAX_ACTIVE_SESSIONS; ++i) {
    result.sessions.used += sessions_[i].used ? 1 : 0;
  }
  for (uint8_t i = 0; i < SUPLAN_MAX_PENDING_HANDSHAKES; ++i) {
    result.pending.used += pending_[i].used ? 1 : 0;
  }
  for (uint8_t i = 0; i < SUPLAN_MAX_OUTSTANDING_LOCATES; ++i) {
    result.locates.used += locates_[i].used ? 1 : 0;
  }
  for (uint8_t i = 0; i < SUPLAN_MAX_RUNTIME_INTERESTS; ++i) {
    result.interests.used += interests_[i].used ? 1 : 0;
  }
  for (uint8_t i = 0; i < SUPLAN_MAX_RETRY_SLOTS; ++i) {
    result.retries.used += retries_[i].isUsed() ? 1 : 0;
  }
  for (uint8_t i = 0; i < SUPLAN_MAX_DEFERRED_APP_EVENTS; ++i) {
    result.deferredEvents.used += deferred_[i].used ? 1 : 0;
  }
  result.workspaceBytes = static_cast<uint32_t>(sizeof(*this) +
                                                 sizeof(PeerTable));
  return result;
}

const Diagnostics &Runtime::diagnostics() const {
  return diagnostics_;
}

RuntimeTestHooks *Runtime::testHooks() {
  return &hooks_;
}

bool Runtime::sendRaw(const Endpoint &endpoint, const uint8_t *data,
                      size_t length, uint8_t frameKind) {
  if (datagrams_ == nullptr || data == nullptr || length == 0) {
    return false;
  }
  if ((frameKind == kTxKindAck && hooks_.dropNextAckTx != 0) ||
      (frameKind == kTxKindData && hooks_.dropNextDataTx != 0) ||
      (frameKind == kTxKindSessionAccept &&
       hooks_.dropNextSessionAcceptTx != 0)) {
    if (frameKind == kTxKindAck) {
      --hooks_.dropNextAckTx;
    } else if (frameKind == kTxKindData) {
      --hooks_.dropNextDataTx;
    } else {
      --hooks_.dropNextSessionAcceptTx;
    }
    return true;
  }
  uint8_t scratch[kSessionAcceptSize];
  if (length == kSessionInitSize || length == kSessionAcceptSize) {
    memcpy(scratch, data, length);
    if (hooks_.corruptNextMacTx != 0 ||
        hooks_.corruptNextSessionMacTx != 0) {
      scratch[length - 1] ^= 1;
      if (hooks_.corruptNextMacTx != 0) {
        --hooks_.corruptNextMacTx;
      } else {
        --hooks_.corruptNextSessionMacTx;
      }
    }
    return datagrams_->sendUnicast(endpoint, scratch, length);
  }
  return datagrams_->sendUnicast(endpoint, data, length);
}

bool Runtime::fragmentDatagram(void *context, const uint8_t *data,
                               size_t length) {
  Runtime *runtime = static_cast<Runtime *>(context);
  const bool isFragment = data[0] == kAdaptationFragment;
  if (isFragment) {
    ++runtime->fragmentOrdinal_;
    if (runtime->hooks_.dropFragmentNumber != 0 &&
        runtime->fragmentOrdinal_ == runtime->hooks_.dropFragmentNumber) {
      runtime->hooks_.dropFragmentNumber = 0;
      ++runtime->diagnostics_.fragmentTx;
      return true;
    }
  }
  if (isFragment && runtime->hooks_.dropNextFragmentTx != 0) {
    --runtime->hooks_.dropNextFragmentTx;
    ++runtime->diagnostics_.fragmentTx;
    return true;
  }
  if (isFragment) {
    ++runtime->diagnostics_.fragmentTx;
  }
  if (!runtime->datagrams_->sendUnicast(runtime->fragmentEndpoint_, data,
                                        length)) {
    return false;
  }
  return true;
}

bool Runtime::knownSession(void *context, uint64_t sessionId) {
  const Runtime *runtime = static_cast<const Runtime *>(context);
  for (uint8_t i = 0; i < SUPLAN_MAX_ACTIVE_SESSIONS; ++i) {
    if (runtime->sessions_[i].used &&
        runtime->sessions_[i].sessionId == sessionId) {
      return true;
    }
  }
  for (uint8_t i = 0; i < SUPLAN_MAX_PENDING_HANDSHAKES; ++i) {
    if (runtime->pending_[i].used && !runtime->pending_[i].initiator &&
        runtime->pending_[i].sessionId == sessionId) {
      return true;
    }
  }
  return false;
}

int Runtime::findReadDependency(uint8_t peerIndex,
                                const ResourceId &resource) const {
  for (uint8_t i = 0; i < SUPLAN_MAX_READ_DEPENDENCIES; ++i) {
    const ReadDependency &entry = dependencies_[i];
    if (entry.used && entry.peerIndex == peerIndex &&
        entry.resourceType == resource.type &&
        entry.resourceId == resource.id) {
      return i;
    }
  }
  return -1;
}

bool Runtime::readNeedsRefresh(uint8_t peerIndex,
                               const ResourceId &resource) const {
  const int slot = findReadDependency(peerIndex, resource);
  return slot >= 0 && dependencies_[slot].needsRefresh;
}

Runtime::RecoveryStatus Runtime::recoveryStatus(uint8_t peerIndex) const {
  RecoveryStatus result = {};
  if (peerIndex < SUPLAN_MAX_PERSISTENT_PEERS) {
    const PeerRecovery &peer = recovery_[peerIndex];
    result.active = peer.active;
    result.sleeping = peer.sleeping;
    result.locateAttempts = peer.locateAttempts;
    result.backgroundStage = peer.backgroundStage;
    result.deadlineMs = peer.deadlineMs;
    result.nextRefreshMs = peer.nextRefreshMs;
  }
  return result;
}

void Runtime::setPeerSleeping(uint8_t peerIndex, bool sleeping) {
  if (peerIndex < SUPLAN_MAX_PERSISTENT_PEERS) {
    recovery_[peerIndex].sleeping = sleeping;
  }
}

void Runtime::beginRecovery(uint8_t peerIndex) {
  PeerRecovery &peer = recovery_[peerIndex];
  if (!peer.active) {
    peer.active = true;
    peer.scheduled = false;
    peer.locateAttempts = 0;
    peer.deadlineMs = datagrams_->nowMs() + kControlLifetimeMs;
  }
}

void Runtime::finishRecovery(uint8_t peerIndex) {
  PeerRecovery &peer = recovery_[peerIndex];
  peer.active = false;
  bool refresh = false;
  for (uint8_t i = 0; i < SUPLAN_MAX_READ_DEPENDENCIES; ++i) {
    ReadDependency &entry = dependencies_[i];
    if (entry.used && entry.peerIndex == peerIndex) {
      if (entry.pending) {
        entry.pending = false;
        entry.needsRefresh = true;
      }
      refresh = refresh || entry.needsRefresh;
    }
  }
  for (uint8_t i = 0; i < SUPLAN_MAX_DEFERRED_APP_EVENTS; ++i) {
    if (deferred_[i].used && deferred_[i].peerIndex == peerIndex) {
      deferred_[i].used = false;
    }
  }
  for (uint8_t i = 0; i < SUPLAN_MAX_RETRY_SLOTS; ++i) {
    if (retries_[i].isUsed() && retries_[i].peerIndex == peerIndex) {
      clearRetryEntry(&retries_[i]);
    }
  }
  cancelPeerRecoveryIfIdle(peerIndex);
  PeerRecord *record = peers_->get(peerIndex);
  if (record != nullptr &&
      record->endpointState == kPeerEndpointLocateCandidate) {
    record->endpoint = Endpoint();
    record->endpointState = kPeerEndpointNone;
  }
  peer.scheduled = refresh;
  if (refresh) {
    static const uint32_t delays[] = {5000, 15000, 60000, 300000};
    const uint32_t base = delays[peer.backgroundStage];
    uint8_t jitter[4] = {};
    // Random failure keeps the hard minimum; it never removes the backoff.
    (void)Supla::Crypto::fillRandom(jitter, sizeof(jitter));
    peer.nextRefreshMs = datagrams_->nowMs() + base +
        getUint32(jitter) % (base / 5 + 1);
    if (peer.backgroundStage < 3) {
      ++peer.backgroundStage;
    }
  }
}

void Runtime::completeRead(uint8_t peerIndex, const ResourceId &resource) {
  const int slot = findReadDependency(peerIndex, resource);
  if (slot >= 0) {
    dependencies_[slot].pending = false;
    dependencies_[slot].needsRefresh = false;
  }
  recovery_[peerIndex].backgroundStage = 0;
}

void Runtime::noteReachability(uint8_t peerIndex) {
  PeerRecovery &peer = recovery_[peerIndex];
  peer.backgroundStage = 0;
  if (!peer.active && peer.scheduled) {
    peer.nextRefreshMs = datagrams_->nowMs();
  }
}

void Runtime::pruneReadDependencies() {
  for (uint8_t i = 0; i < SUPLAN_MAX_READ_DEPENDENCIES; ++i) {
    ReadDependency &entry = dependencies_[i];
    if (!entry.used) {
      continue;
    }
    const uint8_t peerIndex = entry.peerIndex;
    const ResourceId resource = {entry.resourceType, entry.resourceId};
    if (isLocalDestination(peers_->get(peerIndex)) &&
        (peers_->authorize(peerIndex, resource, kPermissionRead) ||
         peers_->authorize(peerIndex, resource, kPermissionAction))) {
      continue;
    }
    // A revoked dependency must not retain queued or retrying READ work.
    for (uint8_t d = 0; d < SUPLAN_MAX_DEFERRED_APP_EVENTS; ++d) {
      DeferredApplication &deferred = deferred_[d];
      ApplicationDataView application = {};
      ResourceDataView body = {};
      if (deferred.used && deferred.peerIndex == peerIndex &&
          decodeApplicationData(deferred.data, deferred.length, &application) &&
          application.messageClass == kMessageClassNative &&
          application.messageType == kNativeReadResource &&
          decodeResourceData(&application, &body) &&
          body.resource.type == resource.type &&
          body.resource.id == resource.id) {
        deferred.used = false;
      }
    }
    for (uint8_t r = 0; r < SUPLAN_MAX_RETRY_SLOTS; ++r) {
      RetryEntry &retry = retries_[r];
      if (retry.isUsed() && retry.peerIndex == peerIndex &&
          (retry.flags & kRetryReadRequest) != 0 &&
          retry.resource.type == resource.type &&
          retry.resource.id == resource.id) {
        clearRetryEntry(&retry);
      }
    }
    entry = ReadDependency();
    cancelPeerRecoveryIfIdle(peerIndex);
    bool refresh = false;
    for (uint8_t d = 0; d < SUPLAN_MAX_READ_DEPENDENCIES; ++d) {
      refresh = refresh || (dependencies_[d].used &&
          dependencies_[d].peerIndex == peerIndex &&
          dependencies_[d].needsRefresh);
    }
    if (!refresh) {
      recovery_[peerIndex].scheduled = false;
    }
  }
}

void Runtime::iterateRecovery(uint32_t now) {
  pruneReadDependencies();
  for (uint8_t peerIndex = 0; peerIndex < peers_->size(); ++peerIndex) {
    PeerRecovery &peer = recovery_[peerIndex];
    bool handshake = false;
    for (uint8_t i = 0; i < SUPLAN_MAX_PENDING_HANDSHAKES; ++i) {
      handshake = handshake || (pending_[i].used && pending_[i].initiator &&
                                 pending_[i].peerIndex == peerIndex);
    }
    bool locate = false;
    for (uint8_t i = 0; i < SUPLAN_MAX_OUTSTANDING_LOCATES; ++i) {
      locate = locate || (locates_[i].used &&
                            locates_[i].peerIndex == peerIndex);
    }
    const PeerRecord *record = peers_->get(peerIndex);
    if (peer.active &&
        (static_cast<int32_t>(now - peer.deadlineMs) >= 0 ||
         (peer.locateAttempts >= 3 && !locate && !handshake &&
          record->endpointState == kPeerEndpointNone))) {
      finishRecovery(peerIndex);
    }
    if (!peer.active && peer.scheduled && !peer.sleeping &&
        static_cast<int32_t>(now - peer.nextRefreshMs) >= 0) {
      beginRecovery(peerIndex);
    }
    if (peer.active) {
      for (uint8_t i = 0; i < SUPLAN_MAX_READ_DEPENDENCIES; ++i) {
        ReadDependency &entry = dependencies_[i];
        if (!entry.used || entry.peerIndex != peerIndex ||
            !entry.needsRefresh || entry.pending) {
          continue;
        }
        const ResourceId resource = {entry.resourceType, entry.resourceId};
        (void)queueRead(peerIndex, resource);
      }
    }
  }
}

void Runtime::startLocate(uint8_t peerIndex) {
  PeerRecord *peer = peers_->get(peerIndex);
  if (peer == nullptr || !peers_->hasActiveGrants(peerIndex)) {
    return;
  }
  if (peer->endpointState != kPeerEndpointNone) {
    startHandshake(peerIndex);
    return;
  }
  for (uint8_t i = 0; i < SUPLAN_MAX_OUTSTANDING_LOCATES; ++i) {
    if (locates_[i].used && locates_[i].peerIndex == peerIndex) {
      return;
    }
  }
  int slot = -1;
  for (uint8_t i = 0; i < SUPLAN_MAX_OUTSTANDING_LOCATES; ++i) {
    if (!locates_[i].used) {
      slot = i;
      break;
    }
  }
  if (slot < 0) {
    ++diagnostics_.poolReject;
    return;
  }
  const uint32_t now = datagrams_->nowMs();
  PeerRecovery &recovery = recovery_[peerIndex];
  if (!recovery.active || recovery.locateAttempts >= 3 ||
      (recovery.locateAttempts != 0 &&
       static_cast<uint32_t>(now - recovery.lastLocateMs) <
           kLocateReplyWindowMs)) {
    return;
  }
  // Scheduling counts attempts even if RNG, MAC or the local send fails.
  ++recovery.locateAttempts;
  recovery.lastLocateMs = now;
  PeerMaterial material = {};
  if (!peers_->materialFor(peerIndex, &material)) {
    return;
  }
  uint8_t nonce[kNonceSize];
  uint8_t mac[kPeerLocatorSize];
  uint8_t frame[50];
  if (!Supla::Crypto::fillRandom(nonce, sizeof(nonce)) ||
      !locateQueryMac(&material, kVersion, kFrameLocate,
                      nonce, mac) ||
      !encodeLocate(frame, material.peerLocator, nonce, mac)) {
    return;
  }
  if (hooks_.corruptNextMacTx != 0) {
    frame[sizeof(frame) - 1] ^= 1;
    --hooks_.corruptNextMacTx;
  }
  locates_[slot].used = true;
  locates_[slot].peerIndex = peerIndex;
  memcpy(locates_[slot].nonce, nonce, sizeof(nonce));
  locates_[slot].expiresAtMs = now + kLocateReplyWindowMs;
  if (datagrams_->sendLocateMulticast(frame, sizeof(frame))) {
    ++diagnostics_.locateTx;
  }
  memset(nonce, 0, sizeof(nonce));
  memset(mac, 0, sizeof(mac));
  updatePoolHighWater();
}

void Runtime::startHandshake(uint8_t peerIndex) {
  PeerRecord *peer = peers_->get(peerIndex);
  if (peer == nullptr || !peers_->hasActiveGrants(peerIndex) ||
      datagrams_ == nullptr || crypto_ == nullptr ||
      !recovery_[peerIndex].active) {
    return;
  }
  for (uint8_t i = 0; i < SUPLAN_MAX_ACTIVE_SESSIONS; ++i) {
    if (sessions_[i].used && sessions_[i].peerIndex == peerIndex) {
      return;
    }
  }
  if (findPendingForPeer(peerIndex, true) >= 0) {
    return;
  }
  if (peer->endpointState == kPeerEndpointNone) {
    startLocate(peerIndex);
    return;
  }
  PeerMaterial material = {};
  if (!peers_->materialFor(peerIndex, &material)) {
    return;
  }
  const Endpoint endpoint = peer->endpoint;
  const int slot = allocatePending();
  if (slot < 0) {
    return;
  }
  PendingHandshake *attempt = &pending_[slot];
  attempt->initiator = true;
  attempt->peerIndex = peerIndex;
  attempt->endpoint = endpoint;
  SessionInit init = {};
  init.suplaProtoVersionMax = suplaProtoVersion_;
  init.rxMaxReassembledFrame = SUPLAN_RX_MAX_REASSEMBLED_FRAME;
  init.featureBits = 0;
  if (!Supla::Crypto::fillRandom(init.ni, sizeof(init.ni))) {
    attempt->used = false;
    return;
  }
  memcpy(init.peerLocator, material.peerLocator,
         kPeerLocatorSize);
  if (!encodeSessionInit(material.initMacKey, &init,
                         attempt->initFrame)) {
    attempt->used = false;
    return;
  }
  attempt->lastTransmitMs = datagrams_->nowMs();
  attempt->expiresAtMs = attempt->lastTransmitMs +
      kSessionInitRetryMs * kSessionInitMaxAttempts +
      kPendingHandshakeTimeoutMs;
  attempt->attempts = 1;
  if (sendRaw(endpoint, attempt->initFrame, sizeof(attempt->initFrame), 0)) {
    ++diagnostics_.sessionInitTx;
  }
  updatePoolHighWater();
}

void Runtime::recoverSession(uint8_t peerIndex, uint64_t sessionId) {
  bool retryAfterHandshake = false;
  for (uint8_t i = 0; i < SUPLAN_MAX_RETRY_SLOTS; ++i) {
    RetryEntry *retry = &retries_[i];
    if (!retry->isUsed() || retry->peerIndex != peerIndex ||
        retry->sessionId != sessionId) {
      continue;
    }
    if (retry->actionDelivery) {
      // An Action Trigger that has made its first protected DATA attempt is
      // scoped to this SESSION and must never cross into a replacement one.
      clearRetryEntry(retry);
      continue;
    }
    SessionEntry *session = nullptr;
    for (uint8_t s = 0; s < SUPLAN_MAX_ACTIVE_SESSIONS; ++s) {
      if (sessions_[s].used && sessions_[s].peerIndex == peerIndex &&
          sessions_[s].sessionId == sessionId) {
        session = &sessions_[s];
        break;
      }
    }
    if (retry->sessionRecoveryAttempts != 0 || session == nullptr ||
        retry->frameLength > sizeof(retry->frame)) {
      clearRetryEntry(retry);
      continue;
    }
    ReplayWindow replay;
    size_t applicationLength = 0;
    uint64_t decodedSessionId = 0;
    uint32_t decodedSequence = 0;
    const ProtectedDataResult result = decodeProtectedData(
        crypto_, &session->transmit, retry->frame, retry->frameLength,
        &replay, applicationBuffer_, sizeof(applicationBuffer_),
        &decodedSessionId, &decodedSequence, &applicationLength);
    ApplicationDataView application = {};
    if ((result != kProtectedDataOk && result != kProtectedDataDuplicate) ||
        decodedSessionId != retry->sessionId ||
        decodedSequence != retry->sequence ||
        applicationLength > sizeof(retry->frame) ||
        !decodeApplicationData(applicationBuffer_, applicationLength,
                               &application) ||
        application.flags != kAckRequired) {
      clearRetryEntry(retry);
      continue;
    }
    const uint16_t oldFrameLength = retry->frameLength;
    memcpy(retry->frame, applicationBuffer_, applicationLength);
    if (oldFrameLength > applicationLength) {
      memset(retry->frame + applicationLength, 0,
             oldFrameLength - applicationLength);
    }
    retry->frameLength = static_cast<uint16_t>(applicationLength);
    retry->sessionRecoveryAttempts = 1;
    retry->awaitingSession = true;
    retryAfterHandshake = true;
  }
  for (uint8_t i = 0; i < SUPLAN_MAX_ACTIVE_SESSIONS; ++i) {
    if (sessions_[i].used && sessions_[i].peerIndex == peerIndex &&
        sessions_[i].sessionId == sessionId) {
      clearSessionEntry(&sessions_[i]);
    }
  }
  if (retryAfterHandshake) {
    startHandshake(peerIndex);
  }
  updatePoolHighWater();
}

void Runtime::cancelPeerRecoveryIfIdle(uint8_t peerIndex) {
  for (uint8_t i = 0; i < SUPLAN_MAX_DEFERRED_APP_EVENTS; ++i) {
    if (deferred_[i].used && deferred_[i].peerIndex == peerIndex) {
      return;
    }
  }
  for (uint8_t i = 0; i < SUPLAN_MAX_RETRY_SLOTS; ++i) {
    if (retries_[i].isUsed() && retries_[i].peerIndex == peerIndex) {
      return;
    }
  }
  if (recovery_[peerIndex].active) {
    for (uint8_t i = 0; i < SUPLAN_MAX_READ_DEPENDENCIES; ++i) {
      if (dependencies_[i].used && dependencies_[i].peerIndex == peerIndex &&
          dependencies_[i].needsRefresh) {
        return;
      }
    }
  }
  for (uint8_t i = 0; i < SUPLAN_MAX_OUTSTANDING_LOCATES; ++i) {
    if (locates_[i].used && locates_[i].peerIndex == peerIndex) {
      locates_[i].used = false;
    }
  }
  for (uint8_t i = 0; i < SUPLAN_MAX_PENDING_HANDSHAKES; ++i) {
    if (pending_[i].used && pending_[i].initiator &&
        pending_[i].peerIndex == peerIndex) {
      clearPendingHandshake(&pending_[i]);
    }
  }
  recovery_[peerIndex].active = false;
  updatePoolHighWater();
}

bool Runtime::requestRead(uint8_t peerIndex, const ResourceId &resource) {
  const PeerRecord *peer = peers_->get(peerIndex);
  const bool readAuthorized = peers_->authorize(peerIndex, resource,
                                                 kPermissionRead);
  const bool actionAuthorized = peers_->authorize(peerIndex, resource,
                                                   kPermissionAction);
  if (!isLocalDestination(peer) || (!readAuthorized && !actionAuthorized)) {
    ++diagnostics_.aclReject;
    return false;
  }
  pruneReadDependencies();
  int slot = findReadDependency(peerIndex, resource);
  if (slot < 0) {
    for (uint8_t i = 0; i < SUPLAN_MAX_READ_DEPENDENCIES; ++i) {
      if (!dependencies_[i].used) {
        slot = i;
        dependencies_[i].used = true;
        dependencies_[i].peerIndex = peerIndex;
        dependencies_[i].resourceType = resource.type;
        dependencies_[i].resourceId = resource.id;
        break;
      }
    }
  }
  if (slot < 0) {
    ++diagnostics_.poolReject;
    return false;
  }
  if (dependencies_[slot].pending) {
    return true;
  }
  beginRecovery(peerIndex);
  return queueRead(peerIndex, resource);
}

bool Runtime::queueRead(uint8_t peerIndex, const ResourceId &resource) {
  const int slot = findReadDependency(peerIndex, resource);
  if (slot < 0) {
    return false;
  }
  dependencies_[slot].needsRefresh = true;
  dependencies_[slot].pending = true;
  size_t length = kApplicationHeaderSize + kResourceHeaderSize;
  applicationBuffer_[0] = kMessageClassNative;
  putUint32(applicationBuffer_ + 1, kNativeReadResource);
  applicationBuffer_[5] = kAckRequired;
  if (!encodeResourceId(resource.type, resource.id,
                        applicationBuffer_ + kApplicationHeaderSize)) {
    return false;
  }
  if (!enqueueApplication(peerIndex, applicationBuffer_, length, true)) {
    dependencies_[slot].pending = false;
    return false;
  }
  return true;
}

bool Runtime::sendControl(uint8_t peerIndex, const ResourceId &resource,
                          const uint8_t *suplaPayload,
                          size_t payloadLength) {
  const PeerRecord *peer = peers_->get(peerIndex);
  if (!isLocalDestination(peer) ||
      !peers_->authorize(peerIndex, resource, kPermissionControl)) {
    ++diagnostics_.aclReject;
    return false;
  }
  if (suplaPayload == nullptr || payloadLength == 0 ||
      !buildResourceApplication(kMessageClassSuplaCall,
                                kSuplaCallChannelSetValue, kAckRequired,
                                resource, suplaPayload, payloadLength,
                                nullptr)) {
    return false;
  }
  return enqueueApplication(peerIndex, applicationBuffer_,
                            kApplicationHeaderSize + kResourceHeaderSize +
                                payloadLength, true);
}

bool Runtime::publishState(uint8_t peerIndex, const ResourceId &resource,
                           uint32_t messageType,
                           const uint8_t *suplaPayload,
                           size_t payloadLength) {
  return publishStateParts(peerIndex, resource, messageType, nullptr, 0,
                           suplaPayload, payloadLength);
}

bool Runtime::publishStateParts(uint8_t peerIndex, const ResourceId &resource,
                                uint32_t messageType, const uint8_t *prefix,
                                size_t prefixLength, const uint8_t *payload,
                                size_t payloadLength) {
  const PeerRecord *peer = peers_->get(peerIndex);
  if (!isLocalSource(peer) || !interested(peerIndex, resource) ||
      !peers_->authorize(peerIndex, resource, kPermissionRead)) {
    return false;
  }
  const size_t totalLength = kApplicationHeaderSize + kResourceHeaderSize +
      prefixLength + payloadLength;
  if ((prefix == nullptr && prefixLength != 0) ||
      (payload == nullptr && payloadLength != 0) ||
      totalLength > sizeof(applicationBuffer_)) {
    return false;
  }
  applicationBuffer_[0] = kMessageClassSuplaCall;
  putUint32(applicationBuffer_ + 1, messageType);
  applicationBuffer_[5] = 0;
  if (!encodeResourceId(resource.type, resource.id,
                        applicationBuffer_ + kApplicationHeaderSize)) {
    return false;
  }
  uint8_t *body = applicationBuffer_ + kApplicationHeaderSize +
      kResourceHeaderSize;
  if (prefixLength != 0) {
    memmove(body, prefix, prefixLength);
  }
  if (payloadLength != 0) {
    memmove(body + prefixLength, payload, payloadLength);
  }
  if (totalLength <= sizeof(deferred_[0].data)) {
    return enqueueApplication(peerIndex, applicationBuffer_, totalLength, true);
  }
  if (processing_) {
    ++diagnostics_.deferredQueueOverflow;
    return false;
  }
  for (uint8_t i = 0; i < SUPLAN_MAX_ACTIVE_SESSIONS; ++i) {
    if (sessions_[i].used && sessions_[i].peerIndex == peerIndex) {
      SessionEntry *session = &sessions_[i];
      return sendProtected(peerIndex, &session->transmit, session->sessionId,
                           &session->nextTransmitSequence,
                           &session->lastActivityMs, applicationBuffer_,
                           totalLength, false, resource);
    }
  }
  return false;
}

bool Runtime::publishAction(uint8_t peerIndex, const ResourceId &resource,
                            const uint8_t *suplaPayload,
                            size_t payloadLength) {
  const PeerRecord *peer = peers_->get(peerIndex);
  if (!isLocalSource(peer) || !interested(peerIndex, resource) ||
      !peers_->authorize(peerIndex, resource, kPermissionAction) ||
      suplaPayload == nullptr || payloadLength != 15) {
    return false;
  }
  if (!buildResourceApplication(kMessageClassSuplaCall,
                                kSuplaCallActionTrigger, kAckRequired, resource,
                                suplaPayload, payloadLength, nullptr)) {
    return false;
  }
  return enqueueApplication(peerIndex, applicationBuffer_,
                            kApplicationHeaderSize + kResourceHeaderSize +
                                payloadLength, true, true);
}

bool Runtime::forgetSession(uint8_t peerIndex) {
  bool removed = false;
  for (uint8_t i = 0; i < SUPLAN_MAX_ACTIVE_SESSIONS; ++i) {
    if (sessions_[i].used && sessions_[i].peerIndex == peerIndex) {
      const uint64_t sessionId = sessions_[i].sessionId;
      clearSessionEntry(&sessions_[i]);
      for (uint8_t r = 0; r < SUPLAN_MAX_RETRY_SLOTS; ++r) {
        if (retries_[r].isUsed() && retries_[r].sessionId == sessionId) {
          if ((retries_[r].flags & kRetryReadRequest) != 0) {
            const int slot =
                findReadDependency(peerIndex, retries_[r].resource);
            if (slot >= 0) {
              dependencies_[slot].pending = false;
              dependencies_[slot].needsRefresh = true;
            }
          }
          clearRetryEntry(&retries_[r]);
        }
      }
      removed = true;
    }
  }
  for (uint8_t i = 0; i < SUPLAN_MAX_PENDING_HANDSHAKES; ++i) {
    if (pending_[i].used && pending_[i].peerIndex == peerIndex) {
      clearPendingHandshake(&pending_[i]);
      removed = true;
    }
  }
  return removed;
}

void Runtime::clearEndpoint(uint8_t peerIndex) {
  PeerRecord *peer = peers_->get(peerIndex);
  if (peer != nullptr) {
    (void)forgetSession(peerIndex);
    for (uint8_t i = 0; i < SUPLAN_MAX_OUTSTANDING_LOCATES; ++i) {
      if (locates_[i].used && locates_[i].peerIndex == peerIndex) {
        locates_[i].used = false;
      }
    }
    peer->endpoint = Endpoint();
    peer->endpointState = kPeerEndpointNone;
  }
}

bool Runtime::startFlood(TestFloodKind kind, uint8_t peerIndex,
                         uint16_t count) {
  if (flood_.active || count > 1000 ||
      kind < kFloodInvalidLocate || kind > kFloodInvalidData ||
      (kind != kFloodInvalidLocate && peers_->get(peerIndex) == nullptr)) {
    return false;
  }
  if (kind == kFloodInvalidSession || kind == kFloodInvalidData) {
    const PeerRecord *peer = peers_->get(peerIndex);
    if (peer == nullptr ||
        peer->endpointState != kPeerEndpointAuthenticated) {
      return false;
    }
  }
  if (kind == kFloodInvalidData) {
    bool hasSession = false;
    for (uint8_t i = 0; i < SUPLAN_MAX_ACTIVE_SESSIONS; ++i) {
      hasSession = hasSession ||
          (sessions_[i].used && sessions_[i].peerIndex == peerIndex);
    }
    if (!hasSession) {
      return false;
    }
  }
  memset(&flood_, 0, sizeof(flood_));
  flood_.kind = kind;
  flood_.peerIndex = peerIndex;
  flood_.remaining = count;
  flood_.active = count != 0;
  if (kind == kFloodInvalidSession && count != 0) {
    PeerMaterial material = {};
    SessionInit init = {};
    if (!peers_->materialFor(peerIndex, &material)) {
      memset(&flood_, 0, sizeof(flood_));
      return false;
    }
    memcpy(init.peerLocator, material.peerLocator, kPeerLocatorSize);
    init.suplaProtoVersionMax = suplaProtoVersion_;
    init.rxMaxReassembledFrame = SUPLAN_RX_MAX_REASSEMBLED_FRAME;
    if (!Supla::Crypto::fillRandom(init.ni, sizeof(init.ni)) ||
        !encodeSessionInit(material.initMacKey, &init,
                           flood_.invalidSessionFrame)) {
      memset(&flood_, 0, sizeof(flood_));
      return false;
    }
    flood_.invalidSessionFrame[kSessionInitSize - 1] ^= 1;
  }
  return true;
}

void Runtime::resetDiagnostics() {
  memset(&diagnostics_, 0, sizeof(diagnostics_));
}

bool Runtime::enqueueApplication(uint8_t peerIndex, const uint8_t *data,
                                 size_t length, bool mayEstablishSession,
                                 bool actionDelivery) {
  if (data == nullptr || length == 0 || length > sizeof(deferred_[0].data) ||
      peers_->get(peerIndex) == nullptr) {
    ++diagnostics_.deferredQueueOverflow;
    return false;
  }
  if (length >= kApplicationHeaderSize + kResourceHeaderSize &&
      data[0] == kMessageClassSuplaCall && data[5] == 0) {
    for (uint8_t i = 0; i < SUPLAN_MAX_DEFERRED_APP_EVENTS; ++i) {
      DeferredApplication *queued = &deferred_[i];
      if (queued->used && queued->peerIndex == peerIndex &&
          queued->data[0] == kMessageClassSuplaCall && queued->data[5] == 0 &&
          memcmp(queued->data + kApplicationHeaderSize,
                 data + kApplicationHeaderSize, kResourceHeaderSize) == 0) {
        memcpy(queued->data, data, length);
        queued->length = static_cast<uint8_t>(length);
        return true;
      }
    }
  }
  int slot = -1;
  for (uint8_t i = 0; i < SUPLAN_MAX_DEFERRED_APP_EVENTS; ++i) {
    if (!deferred_[i].used) {
      slot = i;
      break;
    }
  }
  if (slot < 0) {
    ++diagnostics_.deferredQueueOverflow;
    return false;
  }
  DeferredApplication *queued = &deferred_[slot];
  queued->used = true;
  queued->mayEstablishSession = mayEstablishSession;
  queued->peerIndex = peerIndex;
  queued->length = static_cast<uint8_t>(length);
  memcpy(queued->data, data, length);
  queued->controlExpiresAtMs = datagrams_->nowMs() + kControlLifetimeMs;
  if (actionDelivery && isActionApplication(queued->data, length)) {
    // Action Trigger applications are 26 bytes; keep the absolute deadline in
    // the unused tail of this fixed 32-byte deferred slot.
    putActionDeadline(queued->data, sizeof(queued->data),
                      datagrams_->nowMs() + kActionDeliveryLifetimeMs);
  }
  beginRecovery(peerIndex);
  updatePoolHighWater();
  if (!processing_) {
    drainDeferred();
  }
  return true;
}

void Runtime::expireControls(uint32_t now) {
  // Cancel recovery only after its last queued operation has gone away.
  for (uint8_t i = 0; i < SUPLAN_MAX_DEFERRED_APP_EVENTS; ++i) {
    DeferredApplication *queued = &deferred_[i];
    if (queued->used && !isActionApplication(queued->data, queued->length) &&
        queued->data[0] == kMessageClassSuplaCall &&
        static_cast<int32_t>(now - queued->controlExpiresAtMs) >= 0) {
      queued->used = false;
      cancelPeerRecoveryIfIdle(queued->peerIndex);
    }
  }
  for (uint8_t i = 0; i < SUPLAN_MAX_RETRY_SLOTS; ++i) {
    RetryEntry *retry = &retries_[i];
    if (retry->isUsed() && retry->controlDelivery &&
        static_cast<int32_t>(now - retry->controlExpiresAtMs) >= 0) {
      const uint8_t peerIndex = retry->peerIndex;
      clearRetryEntry(retry);
      cancelPeerRecoveryIfIdle(peerIndex);
    }
  }
}

void Runtime::drainDeferred() {
  if (processing_) {
    return;
  }
  const uint32_t now = datagrams_->nowMs();
  expireControls(now);
  for (uint8_t i = 0; i < SUPLAN_MAX_RETRY_SLOTS; ++i) {
    RetryEntry *retry = &retries_[i];
    if (!retry->isUsed() || !retry->awaitingSession) {
      continue;
    }
    if (retry->actionDelivery &&
        static_cast<int32_t>(now - getActionDeadline(
            retry->frame, sizeof(retry->frame))) >= 0) {
      const uint8_t peerIndex = retry->peerIndex;
      clearRetryEntry(retry);
      cancelPeerRecoveryIfIdle(peerIndex);
      continue;
    }
    int sessionIndex = -1;
    for (uint8_t s = 0; s < SUPLAN_MAX_ACTIVE_SESSIONS; ++s) {
      if (sessions_[s].used &&
          sessions_[s].peerIndex == retry->peerIndex) {
        sessionIndex = s;
        break;
      }
    }
    if (sessionIndex < 0) {
      startHandshake(retry->peerIndex);
      continue;
    }
    SessionEntry *session = &sessions_[sessionIndex];
    const uint8_t peerIndex = retry->peerIndex;
    const uint16_t applicationLength = retry->frameLength;
    const uint8_t recoveryAttempts = retry->sessionRecoveryAttempts;
    const bool controlDelivery = retry->controlDelivery;
    const uint32_t controlExpiresAtMs = retry->controlExpiresAtMs;
    const uint8_t readFlags = retry->flags &
        static_cast<uint8_t>(kRetryReadRequest | kRetryReadExpectsState);
    const ResourceId resource = retry->resource;
    const uint64_t sessionId = session->sessionId;
    const uint32_t sequence = session->nextTransmitSequence;
    if (applicationLength == 0 ||
        applicationLength > sizeof(applicationBuffer_)) {
      clearRetryEntry(retry);
      continue;
    }
    memcpy(applicationBuffer_, retry->frame, applicationLength);
    clearRetryEntry(retry);
    if (!sendProtected(peerIndex, &session->transmit, sessionId,
                       &session->nextTransmitSequence,
                       &session->lastActivityMs, applicationBuffer_,
                       applicationLength, true, resource)) {
      retry->peerIndex = peerIndex;
      retry->resource = resource;
      retry->sessionRecoveryAttempts = recoveryAttempts;
      retry->controlDelivery = controlDelivery;
      retry->controlExpiresAtMs = controlExpiresAtMs;
      retry->flags = readFlags;
      retry->awaitingSession = true;
      retry->frameLength = applicationLength;
      memcpy(retry->frame, applicationBuffer_, applicationLength);
      continue;
    }
    const int newRetryIndex = findRetry(peerIndex, sessionId, sequence);
    if (newRetryIndex >= 0) {
      retries_[newRetryIndex].sessionRecoveryAttempts = recoveryAttempts;
      retries_[newRetryIndex].awaitingSession = false;
      retries_[newRetryIndex].controlDelivery = controlDelivery;
      retries_[newRetryIndex].controlExpiresAtMs = controlExpiresAtMs;
    }
  }
  for (uint8_t i = 0; i < SUPLAN_MAX_DEFERRED_APP_EVENTS; ++i) {
    DeferredApplication *queued = &deferred_[i];
    if (!queued->used) {
      continue;
    }
    const bool actionDelivery =
        isActionApplication(queued->data, queued->length);
    const uint32_t actionExpiresAtMs = actionDelivery
        ? getActionDeadline(queued->data, sizeof(queued->data)) : 0;
    if (queued->data[0] == kMessageClassSuplaCall &&
        queued->data[5] == 0) {
      const ResourceId stateResource = {
          queued->data[kApplicationHeaderSize],
          getUint32(queued->data + kApplicationHeaderSize + 1)};
      if (!interested(queued->peerIndex, stateResource) ||
          !peers_->authorize(queued->peerIndex, stateResource,
                             kPermissionRead)) {
        queued->used = false;
        cancelPeerRecoveryIfIdle(queued->peerIndex);
        continue;
      }
    }
    if (actionDelivery) {
      ApplicationDataView actionApplication = {};
      ResourceId actionResource = {};
      const uint8_t *actionPayload = nullptr;
      size_t actionPayloadLength = 0;
      const bool validAction =
          decodeApplicationData(queued->data, queued->length,
                                &actionApplication) &&
          actionApplication.messageClass == kMessageClassSuplaCall &&
          actionApplication.messageType == kSuplaCallActionTrigger &&
          actionApplication.flags == kAckRequired &&
          resourceFromApplication(actionApplication, &actionResource,
                                  &actionPayload, &actionPayloadLength);
      (void)actionPayload;
      (void)actionPayloadLength;
      if (!validAction ||
          static_cast<int32_t>(now - actionExpiresAtMs) >= 0 ||
          !interested(queued->peerIndex, actionResource) ||
          !isLocalSource(peers_->get(queued->peerIndex)) ||
          !peers_->authorize(queued->peerIndex, actionResource,
                             kPermissionAction)) {
        const uint8_t peerIndex = queued->peerIndex;
        queued->used = false;
        cancelPeerRecoveryIfIdle(peerIndex);
        continue;
      }
    }
    int sessionIndex = -1;
    for (uint8_t s = 0; s < SUPLAN_MAX_ACTIVE_SESSIONS; ++s) {
      if (sessions_[s].used && sessions_[s].peerIndex == queued->peerIndex) {
        sessionIndex = s;
        break;
      }
    }
    if (sessionIndex < 0) {
      if (queued->mayEstablishSession) {
        startHandshake(queued->peerIndex);
      } else {
        queued->used = false;
      }
      continue;
    }
    ApplicationDataView app = {};
    ResourceId resource = {};
    const bool parsed = decodeApplicationData(queued->data, queued->length,
                                              &app);
    if (parsed) {
      if (app.messageClass == kMessageClassSuplaCall) {
        const uint8_t *payload = nullptr;
        size_t payloadLength = 0;
        if (resourceFromApplication(app, &resource, &payload,
                                   &payloadLength)) {
          (void)payload;
          (void)payloadLength;
        }
      } else if (app.messageClass == kMessageClassNative &&
                 app.messageType == kNativeReadResource &&
                 app.bodyLength == kResourceHeaderSize) {
        resource.type = app.body[0];
        resource.id = getUint32(app.body + 1);
      }
    }
    SessionEntry *session = &sessions_[sessionIndex];
    if (actionDelivery && findFreeRetry() < 0) {
      // Capacity exhaustion is a per-Destination best-effort loss. Do not
      // leave this Action Trigger in the deferred queue to send it later when
      // a shared READ/CONTROL retry slot becomes free.
      queued->used = false;
      ++diagnostics_.poolReject;
      cancelPeerRecoveryIfIdle(queued->peerIndex);
      continue;
    }
    const uint32_t sequence = session->nextTransmitSequence;
    if (sendProtected(queued->peerIndex, &session->transmit,
                      session->sessionId, &session->nextTransmitSequence,
                      &session->lastActivityMs, queued->data, queued->length,
                      parsed && app.flags == kAckRequired, resource,
                      actionDelivery, actionExpiresAtMs)) {
      const int retryIndex = findRetry(queued->peerIndex, session->sessionId,
                                       sequence);
      if (retryIndex >= 0 &&
          isControlApplication(queued->data, queued->length)) {
        retries_[retryIndex].controlDelivery = true;
        retries_[retryIndex].controlExpiresAtMs = queued->controlExpiresAtMs;
      }
      queued->used = false;
    } else if (parsed && app.flags == 0) {
      queued->used = false;
      cancelPeerRecoveryIfIdle(queued->peerIndex);
    }
  }
}

bool Runtime::sendProtected(uint8_t peerIndex,
                            const DirectionalKeys *transmitKeys,
                            uint64_t sessionId,
                            uint32_t *nextTransmitSequence,
                            uint32_t *lastActivityMs,
                            const uint8_t *applicationData,
                            size_t applicationLength, bool ackRequired,
                            const ResourceId &resource, bool actionDelivery,
                            uint32_t actionExpiresAtMs) {
  PeerRecord *peer = peers_->get(peerIndex);
  if (peer == nullptr ||
      peer->endpointState != kPeerEndpointAuthenticated) {
    return false;
  }
  return sendProtectedToEndpoint(
      peerIndex, peer->endpoint, transmitKeys, sessionId,
      nextTransmitSequence, lastActivityMs, applicationData,
      applicationLength, ackRequired, resource, actionDelivery,
      actionExpiresAtMs);
}

bool Runtime::sendProtectedToEndpoint(
    uint8_t peerIndex, const Endpoint &endpoint,
    const DirectionalKeys *transmitKeys, uint64_t sessionId,
    uint32_t *nextTransmitSequence, uint32_t *lastActivityMs,
    const uint8_t *applicationData, size_t applicationLength,
    bool ackRequired, const ResourceId &resource, bool actionDelivery,
    uint32_t actionExpiresAtMs) {
  if (transmitKeys == nullptr || nextTransmitSequence == nullptr ||
      *nextTransmitSequence == UINT32_MAX ||
      applicationLength > SUPLAN_MAX_APPLICATION_BYTES) {
    return false;
  }
  // PROTO-014: admission applies before encryption, sequence consumption or TX.
  // Include responder-pending keys used for a protected admission rejection.
  uint16_t peerReceiveLimit = 0;
  for (uint8_t i = 0; i < SUPLAN_MAX_ACTIVE_SESSIONS; ++i) {
    if (sessions_[i].used && sessions_[i].peerIndex == peerIndex &&
        sessions_[i].sessionId == sessionId) {
      peerReceiveLimit = sessions_[i].peerRxMaxReassembledFrame;
      break;
    }
  }
  if (peerReceiveLimit == 0) {
    for (uint8_t i = 0; i < SUPLAN_MAX_PENDING_HANDSHAKES; ++i) {
      if (pending_[i].used && pending_[i].peerIndex == peerIndex &&
          pending_[i].sessionId == sessionId) {
        peerReceiveLimit = pending_[i].peerRxMaxReassembledFrame;
        break;
      }
    }
  }
  if (applicationLength + kProtectedHeaderSize + kAeadTagSize >
      peerReceiveLimit) {
    return false;
  }
  if (actionDelivery &&
      (!ackRequired || !isLocalSource(peers_->get(peerIndex)) ||
       !peers_->authorize(peerIndex, resource, kPermissionAction) ||
       static_cast<int32_t>(datagrams_->nowMs() - actionExpiresAtMs) >= 0)) {
    return false;
  }
  int retryIndex = -1;
  if (ackRequired) {
    retryIndex = findFreeRetry();
    if (retryIndex < 0) {
      ++diagnostics_.poolReject;
      return false;
    }
  }
  size_t frameLength = 0;
  const uint32_t sequence = (*nextTransmitSequence)++;
  if (!encodeProtectedData(crypto_, transmitKeys, sessionId,
                           sequence, applicationData, applicationLength,
                           transmitFrame_, sizeof(transmitFrame_),
                           &frameLength)) {
    return false;
  }
  if (peers_->get(peerIndex) == nullptr) {
    return false;
  }
  if (ackRequired && frameLength > sizeof(retries_[0].frame)) {
    ++diagnostics_.poolReject;
    return false;
  }
  uint8_t retryFlags = 0;
  if (ackRequired && applicationLength >= kApplicationHeaderSize &&
      applicationData[0] == kMessageClassNative &&
      getUint32(applicationData + 1) == kNativeReadResource) {
    retryFlags |= kRetryReadRequest;
    if (peers_->authorize(peerIndex, resource, kPermissionRead)) {
      retryFlags |= kRetryReadExpectsState;
    }
  }
  uint8_t frameKind = kTxKindData;
  if (applicationData[0] == kMessageClassNative &&
      getUint32(applicationData + 1) == 1) {
    frameKind = kTxKindAck;
  }
  if (frameKind == kTxKindData && hooks_.corruptNextDataTagTx != 0 &&
      frameLength != 0) {
    transmitFrame_[frameLength - 1] ^= 1;
    --hooks_.corruptNextDataTagTx;
  }
  fragmentEndpoint_ = endpoint;
  const size_t maxPayload = hooks_.maxDatagramPayload <
          datagrams_->maxDatagramPayload()
      ? hooks_.maxDatagramPayload : datagrams_->maxDatagramPayload();
  const uint32_t frameId = UINT32_C(0xC3000000) |
      (nextFrameId_++ & UINT32_C(0x00FFFFFF));
  fragmentOrdinal_ = 0;
  bool sent = true;
  if (frameKind == kTxKindData && dataTransmitObserver_ != nullptr) {
    dataTransmitObserver_(dataTransmitContext_, endpoint,
                          transmitFrame_, frameLength);
  }
  if (frameKind == kTxKindData && hooks_.failNextDataTx != 0) {
    --hooks_.failNextDataTx;
    sent = false;
  } else if ((frameKind == kTxKindAck && hooks_.dropNextAckTx != 0) ||
      (frameKind == kTxKindData && hooks_.dropNextDataTx != 0)) {
    if (frameKind == kTxKindAck) {
      --hooks_.dropNextAckTx;
    } else {
      --hooks_.dropNextDataTx;
    }
  } else {
    sent = fragmentSender_.send(transmitFrame_, frameLength, maxPayload,
                                frameId, fragmentDatagram, this);
  }
  if (!sent && !ackRequired) {
    return false;
  }
  if (sent) {
    ++diagnostics_.dataTx;
  }
  if (applicationData[0] == kMessageClassSuplaCall) {
    const uint32_t messageType = getUint32(applicationData + 1);
    if (messageType == kSuplaCallDeviceChannelValueChangedC ||
        messageType == kSuplaCallDeviceChannelExtendedValueChanged) {
      ++diagnostics_.stateNotificationTx;
    } else if (messageType == kSuplaCallActionTrigger) {
      ++diagnostics_.actionTx;
    }
  }
  if (sent && lastActivityMs != nullptr) {
    *lastActivityMs = datagrams_->nowMs();
  }
  if (ackRequired) {
    RetryEntry *retry = &retries_[retryIndex];
    retry->peerIndex = peerIndex;
    retry->resource = resource;
    retry->sessionId = sessionId;
    retry->sequence = sequence;
    retry->lastTransmitMs = datagrams_->nowMs();
    retry->attempts = 1;
    retry->sessionRecoveryAttempts = 0;
    retry->flags = retryFlags;
    retry->awaitingSession = false;
    retry->actionDelivery = actionDelivery;
    retry->frameLength = static_cast<uint16_t>(frameLength);
    memcpy(retry->frame, transmitFrame_, frameLength);
    if (actionDelivery) {
      // Keep action metadata outside the transmitted protected frame.
      putActionDeadline(retry->frame, sizeof(retry->frame), actionExpiresAtMs);
    }
    updatePoolHighWater();
  }
  return true;
}

bool Runtime::buildResourceApplication(uint8_t messageClass,
                                       uint32_t messageType, uint8_t flags,
                                       const ResourceId &resource,
                                       const uint8_t *payload,
                                       size_t payloadLength,
                                       size_t *applicationLength) {
  const size_t total = kApplicationHeaderSize + kResourceHeaderSize +
      payloadLength;
  if ((payload == nullptr && payloadLength != 0) ||
      total > sizeof(applicationBuffer_) || messageType == 0 ||
      (messageClass != kMessageClassSuplaCall &&
       messageClass != kMessageClassNative)) {
    return false;
  }
  applicationBuffer_[0] = messageClass;
  putUint32(applicationBuffer_ + 1, messageType);
  applicationBuffer_[5] = flags;
  if (!encodeResourceId(resource.type, resource.id,
                        applicationBuffer_ + kApplicationHeaderSize)) {
    return false;
  }
  if (payloadLength != 0) {
    memmove(applicationBuffer_ + kApplicationHeaderSize + kResourceHeaderSize,
            payload, payloadLength);
  }
  if (applicationLength != nullptr) {
    *applicationLength = total;
  }
  return true;
}

bool Runtime::resourceFromApplication(const ApplicationDataView &application,
                                      ResourceId *resource,
                                      const uint8_t **payload,
                                      size_t *payloadLength) const {
  ResourceDataView view = {};
  if (resource == nullptr || payload == nullptr || payloadLength == nullptr ||
      !decodeResourceData(&application, &view)) {
    return false;
  }
  *resource = view.resource;
  *payload = view.payload;
  *payloadLength = view.payloadLength;
  return true;
}

void Runtime::processDatagram(const Endpoint &source, const uint8_t *data,
                              size_t length) {
  if (data == nullptr || length < 2) {
    ++diagnostics_.invalidDataDrop;
    return;
  }
  if (data[0] == kVersion && data[1] == kFrameLocate && length == 50) {
    processLocate(source, data, length);
    return;
  }
  if (data[0] == kVersion && data[1] == kFrameLocateReply && length == 34) {
    processLocateReply(source, data, length);
    return;
  }
  if (data[0] == kVersion && data[1] == kFrameSessionInit &&
      length == kSessionInitSize) {
    processSessionInit(source, data, length);
    return;
  }
  if (data[0] == kVersion && data[1] == kFrameSessionAccept &&
      length == kSessionAcceptSize) {
    processSessionAccept(source, data, length);
    return;
  }
  const bool wasActive = fragmentReassembler_.active();
  const bool fragment = data[0] == kAdaptationFragment;
  if (fragment && hooks_.dropNextFragmentRx != 0) {
    --hooks_.dropNextFragmentRx;
    return;
  }
  if (fragment) {
    ++diagnostics_.fragmentRx;
  }
  AdaptedFrameView adapted = {};
  const AdaptationResult result = fragmentReassembler_.accept(
      source, data, length, datagrams_->nowMs(), kReassemblyTimeoutMs,
      knownSession, this, &adapted);
  if (!wasActive && fragmentReassembler_.active()) {
    ++diagnostics_.reassemblyStarted;
  }
  if (fragment && result != kAdaptationPending &&
      result != kAdaptationComplete) {
    ++diagnostics_.reassemblyRejected;
  }
  if (result == kAdaptationComplete) {
    if (fragment) {
      ++diagnostics_.reassemblyCompleted;
    }
    if (adapted.length >= 2 && adapted.data[0] == kVersion &&
        adapted.data[1] == kFrameData && hooks_.dropNextDataRx != 0) {
      // Model a transport receive loss before authentication/replay handling.
      // A retry must therefore arrive as a fresh protected DATA frame.
      --hooks_.dropNextDataRx;
      return;
    }
    processProtected(source, adapted.data, adapted.length);
  }
}

void Runtime::processLocate(const Endpoint &source, const uint8_t *data,
                            size_t length) {
  uint8_t locator[kPeerLocatorSize];
  uint8_t nonce[kNonceSize];
  uint8_t mac[kPeerLocatorSize];
  if (!decodeLocate(data, length, locator, nonce, mac)) {
    ++diagnostics_.invalidLocateDrop;
    return;
  }
  const int peerIndex = peers_->findByLocator(locator);
  if (peerIndex < 0 || !peers_->hasActiveGrants(peerIndex)) {
    ++diagnostics_.invalidLocateDrop;
    return;
  }
  PeerRecord *peer = peers_->get(static_cast<uint8_t>(peerIndex));
  // Either endpoint may need to rediscover the other one. This is required
  // when a Source sends an Action Trigger after losing its cached endpoint.
  // A valid grant is still required, and SESSION remains the endpoint proof.
  if (peer == nullptr ||
      (!isLocalSource(peer) && !isLocalDestination(peer))) {
    ++diagnostics_.invalidLocateDrop;
    return;
  }
  PeerMaterial material = {};
  if (!peers_->materialFor(static_cast<uint8_t>(peerIndex),
                           &material)) {
    ++diagnostics_.invalidLocateDrop;
    return;
  }
  uint8_t expected[kPeerLocatorSize];
  if (peer == nullptr ||
      !locateQueryMac(&material, kVersion, kFrameLocate,
                      nonce, expected) ||
      !equalBytesConstantTime(expected, mac, sizeof(expected))) {
    ++diagnostics_.invalidLocateDrop;
    return;
  }
  // Multicast may loop the request back to this process. Match the fresh
  // nonce of an outstanding local query so it cannot install our own socket
  // endpoint as the remote candidate. Authenticate the packet before using
  // this suppression rule.
  for (uint8_t i = 0; i < SUPLAN_MAX_OUTSTANDING_LOCATES; ++i) {
    if (locates_[i].used && locates_[i].peerIndex == peerIndex &&
        memcmp(locates_[i].nonce, nonce, sizeof(nonce)) == 0) {
      return;
    }
  }
  ++diagnostics_.locateRx;
  const uint32_t now = datagrams_->nowMs();
  if (peer->hasLocateReplied &&
      static_cast<uint32_t>(now - peer->lastLocateReplyMs) <
          kLocateReplyRateLimitMs) {
    return;
  }
  uint8_t replyMac[kPeerLocatorSize];
  uint8_t reply[34];
  if (!locateReplyMac(&material, kVersion, kFrameLocateReply,
                      nonce, replyMac) ||
      !encodeLocateReply(reply, nonce, replyMac)) {
    return;
  }
  if (sendRaw(source, reply, sizeof(reply), 0)) {
    ++diagnostics_.locateReplyTx;
    peer->lastLocateReplyMs = now;
    peer->hasLocateReplied = true;
  }
}

void Runtime::processLocateReply(const Endpoint &source, const uint8_t *data,
                                size_t length) {
  uint8_t nonce[kNonceSize];
  uint8_t mac[kPeerLocatorSize];
  if (!decodeLocateReply(data, length, nonce, mac)) {
    ++diagnostics_.invalidLocateDrop;
    return;
  }
  const uint32_t now = datagrams_->nowMs();
  for (uint8_t i = 0; i < SUPLAN_MAX_OUTSTANDING_LOCATES; ++i) {
    OutstandingLocate *locate = &locates_[i];
    if (!locate->used || static_cast<int32_t>(now - locate->expiresAtMs) >= 0 ||
        memcmp(locate->nonce, nonce, sizeof(nonce)) != 0) {
      continue;
    }
    PeerRecord *peer = peers_->get(locate->peerIndex);
    PeerMaterial material = {};
    uint8_t expected[kPeerLocatorSize];
    if (peer == nullptr ||
        !peers_->materialFor(locate->peerIndex, &material) ||
        !locateReplyMac(&material, kVersion,
                        kFrameLocateReply, nonce, expected) ||
        !equalBytesConstantTime(expected, mac, sizeof(expected))) {
      ++diagnostics_.invalidLocateDrop;
      return;
    }
    ++diagnostics_.locateReplyRx;
    const uint8_t peerIndex = locate->peerIndex;
    locate->used = false;
    // A concurrent inbound SESSION may have confirmed the endpoint while
    // this LOCATE was outstanding. Discovery cannot demote that endpoint.
    if (peer->endpointState != kPeerEndpointAuthenticated) {
      peer->endpoint = source;
      peer->endpointState = kPeerEndpointLocateCandidate;
      startHandshake(peerIndex);
    }
    return;
  }
  ++diagnostics_.invalidLocateDrop;
}

void Runtime::processSessionInit(const Endpoint &source, const uint8_t *data,
                                 size_t length) {
  if (data == nullptr || length != kSessionInitSize) {
    ++diagnostics_.invalidSessionDrop;
    return;
  }
  const int selected = peers_->findByLocator(data + 2);
  if (selected < 0 || !peers_->hasActiveGrants(selected)) {
    ++diagnostics_.sessionUnknownDrop;
    return;
  }
  const uint8_t peerIndex = static_cast<uint8_t>(selected);
  PeerRecord *peer = peers_->get(peerIndex);
  PeerMaterial material = {};
  SessionInit init = {};
  if (peer == nullptr ||
      !peers_->materialFor(peerIndex, &material) ||
      !decodeSessionInit(material.initMacKey, data, length, &init)) {
    ++diagnostics_.invalidSessionDrop;
    return;
  }
  ++diagnostics_.sessionInitRx;
  if (!isLocalSource(peer) && !isLocalDestination(peer)) {
    ++diagnostics_.invalidSessionDrop;
    return;
  }
  const int existingInitiator = findPendingForPeer(peerIndex, true);
  if (existingInitiator >= 0) {
    if (isLocalSource(peer)) {
      // The immutable Source-initiated attempt wins simultaneous initiation.
      return;
    }
    clearPendingHandshake(&pending_[existingInitiator]);
  }
  const int existingResponder = findPendingForPeer(peerIndex, false);
  if (existingResponder >= 0) {
    PendingHandshake *old = &pending_[existingResponder];
    if (endpointEqual(old->endpoint, source) &&
        memcmp(old->initFrame, data, kSessionInitSize) == 0) {
      if (sendRaw(source, old->acceptFrame, sizeof(old->acceptFrame),
                  kTxKindSessionAccept)) {
        ++diagnostics_.sessionAcceptTx;
      }
      return;
    }
    clearPendingHandshake(old);
  }
  const int slot = allocatePending();
  if (slot < 0) {
    return;
  }
  PendingHandshake *response = &pending_[slot];
  response->initiator = false;
  response->peerIndex = peerIndex;
  response->endpoint = source;
  response->peerRxMaxReassembledFrame = init.rxMaxReassembledFrame;
  memcpy(response->initFrame, data, sizeof(response->initFrame));
  SessionAccept accept = {};
  memcpy(accept.peerLocator, init.peerLocator, kPeerLocatorSize);
  accept.selectedSuplaProtoVersion =
      init.suplaProtoVersionMax < suplaProtoVersion_
      ? init.suplaProtoVersionMax : suplaProtoVersion_;
  accept.responderRxMaxReassembledFrame =
      SUPLAN_RX_MAX_REASSEMBLED_FRAME;
  accept.selectedFeatureBits = 0;
  uint8_t sessionIdBytes[8];
  if (!Supla::Crypto::fillRandom(accept.nr, sizeof(accept.nr)) ||
      !Supla::Crypto::fillRandom(sessionIdBytes, sizeof(sessionIdBytes))) {
    clearPendingHandshake(response);
    return;
  }
  accept.sessionId = getUint64(sessionIdBytes);
  if (accept.sessionId == 0) {
    accept.sessionId = 1;
  }
  response->sessionId = accept.sessionId;
  if (!encodeSessionAccept(material.acceptMacKey,
                           response->initFrame, &accept,
                           response->acceptFrame) ||
      !deriveSessionKeys(material.peerKey,
                         material.contextHash, init.ni,
                         accept.nr, response->initFrame,
                         sizeof(response->initFrame), response->acceptFrame,
                         sizeof(response->acceptFrame), response->sessionId,
                         &response->keys,
                         transmitFrame_)) {
    clearPendingHandshake(response);
    return;
  }
  response->expiresAtMs = datagrams_->nowMs() + kPendingHandshakeTimeoutMs;
  if (sendRaw(source, response->acceptFrame, sizeof(response->acceptFrame),
              kTxKindSessionAccept)) {
    ++diagnostics_.sessionAcceptTx;
  }
  updatePoolHighWater();
}

void Runtime::processSessionAccept(const Endpoint &source, const uint8_t *data,
                                   size_t length) {
  if (hooks_.dropNextSessionAcceptRx != 0) {
    --hooks_.dropNextSessionAcceptRx;
    return;
  }
  if (data == nullptr || length != kSessionAcceptSize) {
    ++diagnostics_.invalidSessionDrop;
    return;
  }
  int slot = -1;
  for (uint8_t i = 0; i < SUPLAN_MAX_PENDING_HANDSHAKES; ++i) {
    if (pending_[i].used && pending_[i].initiator &&
        endpointEqual(pending_[i].endpoint, source) &&
        memcmp(pending_[i].initFrame + 2, data + 2,
               kPeerLocatorSize) == 0) {
      slot = i;
      break;
    }
  }
  if (slot < 0) {
    ++diagnostics_.sessionUnknownDrop;
    return;
  }
  PendingHandshake *attempt = &pending_[slot];
  PeerRecord *peer = peers_->get(attempt->peerIndex);
  PeerMaterial material = {};
  SessionInit init = {};
  SessionAccept accept = {};
  if (peer == nullptr ||
      !peers_->materialFor(attempt->peerIndex, &material) ||
      !decodeSessionInit(material.initMacKey, attempt->initFrame,
                         sizeof(attempt->initFrame), &init) ||
      !decodeSessionAccept(material.acceptMacKey,
                           attempt->initFrame, &init, data, length,
                           &accept)) {
    ++diagnostics_.invalidSessionDrop;
    return;
  }
  memcpy(attempt->acceptFrame, data, sizeof(attempt->acceptFrame));
  attempt->sessionId = accept.sessionId;
  attempt->peerRxMaxReassembledFrame = accept.responderRxMaxReassembledFrame;
  if (!deriveSessionKeys(material.peerKey,
                         material.contextHash, init.ni,
                         accept.nr, attempt->initFrame,
                         sizeof(attempt->initFrame), attempt->acceptFrame,
                         sizeof(attempt->acceptFrame), attempt->sessionId,
                         &attempt->keys,
                         transmitFrame_)) {
    ++diagnostics_.invalidSessionDrop;
    clearPendingHandshake(attempt);
    return;
  }
  peer->endpoint = source;
  peer->endpointState = kPeerEndpointAuthenticated;
  noteReachability(attempt->peerIndex);
  ++diagnostics_.sessionAcceptRx;
  const uint8_t peerIndex = attempt->peerIndex;
  const int sessionIndex = allocateSession(peerIndex);
  if (sessionIndex < 0) {
    clearPendingHandshake(attempt);
    return;
  }
  SessionEntry *session = &sessions_[sessionIndex];
  session->sessionId = attempt->sessionId;
  session->peerRxMaxReassembledFrame = attempt->peerRxMaxReassembledFrame;
  session->transmit = attempt->keys.initiatorToResponder;
  session->receive = attempt->keys.responderToInitiator;
  session->nextTransmitSequence = 0;
  session->lastActivityMs = datagrams_->nowMs();
  clearPendingHandshake(attempt);
  ++diagnostics_.sessionEstablished;
  drainDeferred();
}

void Runtime::processProtected(const Endpoint &source, const uint8_t *frame,
                              size_t frameLength) {
  if (frame == nullptr || frameLength < kProtectedHeaderSize) {
    ++diagnostics_.invalidDataDrop;
    return;
  }
  const uint64_t sessionId = getUint64(frame + 2);
  const uint32_t sequence = getUint32(frame + 10);
  int sessionIndex = findSession(sessionId, source);
  PendingHandshake *pendingSession = nullptr;
  if (sessionIndex < 0) {
    const int pendingIndex = findPending(sessionId, source);
    if (pendingIndex >= 0) {
      pendingSession = &pending_[pendingIndex];
    }
  }
  if (sessionIndex < 0 && pendingSession == nullptr) {
    ++diagnostics_.sessionUnknownDrop;
    ++diagnostics_.invalidDataDrop;
    return;
  }
  const uint8_t peerIndex = sessionIndex >= 0
      ? sessions_[sessionIndex].peerIndex : pendingSession->peerIndex;
  PeerRecord *peer = peers_->get(peerIndex);
  DirectionalKeys *receiveKeys = nullptr;
  ReplayWindow *replay = nullptr;
  if (sessionIndex >= 0) {
    receiveKeys = &sessions_[sessionIndex].receive;
    replay = &sessions_[sessionIndex].receiveReplay;
  } else {
    receiveKeys = pendingSession->initiator
        ? &pendingSession->keys.responderToInitiator
        : &pendingSession->keys.initiatorToResponder;
    replay = &pendingSession->receiveReplay;
  }
  size_t plaintextLength = 0;
  uint64_t decodedSessionId = 0;
  uint32_t decodedSequence = 0;
  const ProtectedDataResult result = decodeProtectedData(
      crypto_, receiveKeys, frame, frameLength, replay, applicationBuffer_,
      sizeof(applicationBuffer_), &decodedSessionId, &decodedSequence,
      &plaintextLength);
  if (result == kProtectedDataAuthenticationFailed) {
    ++diagnostics_.dataAuthFail;
    ++diagnostics_.invalidDataDrop;
    return;
  }
  if (result == kProtectedDataTooOld) {
    ++diagnostics_.dataReplayDrop;
    ++diagnostics_.invalidDataDrop;
    return;
  }
  if (result == kProtectedDataMalformed ||
      result == kProtectedDataCapacityExceeded) {
    ++diagnostics_.invalidDataDrop;
    return;
  }
  if (peer == nullptr || decodedSessionId != sessionId ||
      decodedSequence != sequence) {
    ++diagnostics_.invalidDataDrop;
    return;
  }
  if (sessionIndex < 0) {
    // Key possession is proven. Admission happens before application dispatch.
    const int admitted = allocateSession(peerIndex);
    if (admitted < 0) {
      if (!pendingSession->initiator) {
        uint32_t rejectSequence = 0;
        uint8_t reason = kSessionRejectResourceLimit;
        size_t rejectLength = 0;
        const ResourceId noResource = {};
        if (encodeApplicationData(kMessageClassNative,
                                  kNativeSessionReject, 0, &reason,
                                  sizeof(reason), applicationBuffer_,
                                  sizeof(applicationBuffer_), &rejectLength) &&
            sendProtectedToEndpoint(
                peerIndex, pendingSession->endpoint,
                &pendingSession->keys.responderToInitiator,
                pendingSession->sessionId, &rejectSequence, nullptr,
                applicationBuffer_, rejectLength, false, noResource)) {
          ++diagnostics_.sessionRejectTx;
        }
        clearPendingHandshake(pendingSession);
      }
      memset(applicationBuffer_, 0, plaintextLength);
      return;
    }
    SessionEntry *session = &sessions_[admitted];
    session->sessionId = pendingSession->sessionId;
    session->peerRxMaxReassembledFrame =
        pendingSession->peerRxMaxReassembledFrame;
    session->transmit = pendingSession->keys.responderToInitiator;
    session->receive = pendingSession->keys.initiatorToResponder;
    session->receiveReplay = pendingSession->receiveReplay;
    session->nextTransmitSequence = 0;
    session->lastActivityMs = datagrams_->nowMs();
    peer->endpoint = source;
    peer->endpointState = kPeerEndpointAuthenticated;
    clearPendingHandshake(pendingSession);
    sessionIndex = admitted;
    ++diagnostics_.sessionEstablished;
  }
  noteReachability(peerIndex);
  SessionEntry *session = &sessions_[sessionIndex];
  session->lastActivityMs = datagrams_->nowMs();
  if (result == kProtectedDataDuplicate) {
    ++diagnostics_.dataDuplicate;
  } else {
    // A fresh authenticated sequence owns this replay-window cache slot,
    // even if application validation or dispatch produces no ACK.
    session->ackResults[static_cast<uint8_t>(sequence & 63U)] = 0;
    ++diagnostics_.dataRx;
  }
  processing_ = true;
  processApplication(session, sequence, applicationBuffer_, plaintextLength,
                     result == kProtectedDataDuplicate);
  processing_ = false;
  drainDeferred();
}

void Runtime::processApplication(SessionEntry *session, uint32_t sequence,
                                 const uint8_t *data, size_t length,
                                 bool duplicate) {
  ApplicationDataView app = {};
  if (!decodeApplicationData(data, length, &app)) {
    ++diagnostics_.invalidDataDrop;
    return;
  }
  if (app.messageClass == kMessageClassNative && app.messageType == 1 &&
      hooks_.dropNextAckRx != 0) {
    --hooks_.dropNextAckRx;
    return;
  }
  if (app.messageClass == kMessageClassNative) {
    handleNative(session->peerIndex, session, sequence, app, duplicate);
  } else if (app.messageClass == kMessageClassSuplaCall) {
    handleSuplaCall(session->peerIndex, session, sequence, app, duplicate);
  } else {
    ++diagnostics_.invalidDataDrop;
  }
}

void Runtime::pruneInterests(uint32_t now) {
  for (uint8_t i = 0; i < SUPLAN_MAX_RUNTIME_INTERESTS; ++i) {
    RuntimeInterest &interest = interests_[i];
    if (interest.used &&
        (static_cast<uint32_t>(now - interest.lastRefreshMs) >=
             kRuntimeInterestTimeoutMs ||
         !isLocalSource(peers_->get(interest.peerIndex)) ||
         (!peers_->authorize(interest.peerIndex, interest.resource,
                             kPermissionRead) &&
          !peers_->authorize(interest.peerIndex, interest.resource,
                             kPermissionAction)))) {
      memset(&interest, 0, sizeof(interest));
    }
  }
}

bool Runtime::addInterest(uint8_t peerIndex, const ResourceId &resource) {
  pruneInterests(datagrams_->nowMs());
  for (uint8_t i = 0; i < SUPLAN_MAX_RUNTIME_INTERESTS; ++i) {
    RuntimeInterest *interest = &interests_[i];
    if (interest->used && interest->peerIndex == peerIndex &&
        interest->resource.type == resource.type &&
        interest->resource.id == resource.id) {
      interest->lastRefreshMs = datagrams_->nowMs();
      updatePoolHighWater();
      return true;
    }
  }
  const int slot = findFreeInterest();
  if (slot < 0) {
    ++diagnostics_.poolReject;
    return false;
  }
  interests_[slot].used = true;
  interests_[slot].peerIndex = peerIndex;
  interests_[slot].resource = resource;
  interests_[slot].lastRefreshMs = datagrams_->nowMs();
  updatePoolHighWater();
  return true;
}

bool Runtime::interested(uint8_t peerIndex, const ResourceId &resource) const {
  for (uint8_t i = 0; i < SUPLAN_MAX_RUNTIME_INTERESTS; ++i) {
    const RuntimeInterest &interest = interests_[i];
    if (interest.used && interest.peerIndex == peerIndex &&
        interest.resource.type == resource.type &&
        interest.resource.id == resource.id) {
      return true;
    }
  }
  return false;
}

void Runtime::handleNative(uint8_t peerIndex, SessionEntry *session,
                           uint32_t sequence,
                           const ApplicationDataView &application,
                           bool duplicate) {
  if (application.messageType == 1) {
    if (duplicate || application.flags != 0 || application.bodyLength != 5) {
      ++diagnostics_.invalidDataDrop;
      return;
    }
    const uint32_t ackedSequence = getUint32(application.body);
    const uint8_t result = application.body[4];
    const int retryIndex = findRetry(peerIndex, session->sessionId,
                                     ackedSequence);
    if (retryIndex >= 0) {
      RetryEntry *retry = &retries_[retryIndex];
      if ((retry->flags & kRetryAwaitingReadState) == 0) {
        const ResourceId resource = retry->resource;
        const bool expectsState =
            (retry->flags & kRetryReadExpectsState) != 0;
        const bool stateReceived =
            (retry->flags & kRetryReadStateReceived) != 0;
        if (expectsState && result == kResultTrue && !stateReceived) {
          retry->flags |= kRetryAwaitingReadState;
          retry->lastTransmitMs = datagrams_->nowMs();
        } else {
          if ((retry->flags & kRetryReadRequest) != 0) {
            completeRead(peerIndex, resource);
          }
          clearRetryEntry(retry);
        }
        ++diagnostics_.ackRx;
        if (application_ != nullptr) {
          application_->operationAcknowledged(peerIndex, resource,
                                              ackedSequence, result);
        }
        updatePoolHighWater();
      }
    }
    return;
  }
  if (application.messageType == kNativeSessionReject) {
    if (duplicate || application.flags != 0 || application.bodyLength != 1 ||
        application.body[0] != kSessionRejectResourceLimit) {
      ++diagnostics_.invalidDataDrop;
      return;
    }
    ++diagnostics_.sessionRejectRx;
    (void)forgetSession(peerIndex);
    return;
  }
  if (application.messageType != kNativeReadResource ||
      application.flags != kAckRequired ||
      application.bodyLength != kResourceHeaderSize) {
    ++diagnostics_.invalidDataDrop;
    return;
  }
  ResourceId resource = {};
  resource.type = application.body[0];
  resource.id = getUint32(application.body + 1);
  const PeerRecord *peer = peers_->get(peerIndex);
  const bool readAuthorized = peers_->authorize(peerIndex, resource,
                                                 kPermissionRead);
  const bool actionAuthorized = peers_->authorize(peerIndex, resource,
                                                   kPermissionAction);
  if (duplicate) {
    const uint8_t cacheIndex = static_cast<uint8_t>(sequence & 63U);
    const uint8_t result = session->ackResults[cacheIndex];
    if (result != 0 && isLocalSource(peer) &&
        (readAuthorized || actionAuthorized)) {
      sendAck(peerIndex, session, sequence, result);
    }
    return;
  }
  if (!isLocalSource(peer) || (!readAuthorized && !actionAuthorized)) {
    ++diagnostics_.aclReject;
    return;
  }
  bool eventOnly = false;
  size_t payloadLength = 0;
  const size_t payloadOffset = kApplicationHeaderSize + kResourceHeaderSize;
  const bool hasState = application_ != nullptr && application_->readResource(
      resource, &eventOnly, applicationBuffer_ + payloadOffset,
      sizeof(applicationBuffer_) - payloadOffset, &payloadLength);
  if (eventOnly) {
    if ((readAuthorized || actionAuthorized) &&
        addInterest(peerIndex, resource)) {
      ++diagnostics_.readDispatched;
      if (application_ != nullptr) {
        application_->readInterestRefreshed(peerIndex, resource, true);
      }
      session->ackResults[static_cast<uint8_t>(sequence & 63U)] =
          kResultTrue;
      sendAck(peerIndex, session, sequence, kResultTrue);
    }
    return;
  }
  if (!readAuthorized) {
    ++diagnostics_.aclReject;
    return;
  }
  if (!hasState || payloadLength > sizeof(applicationBuffer_) - payloadOffset) {
    ++diagnostics_.resourceNotFound;
    return;
  }
  if (!addInterest(peerIndex, resource)) {
    return;
  }
  ++diagnostics_.readDispatched;
  if (application_ != nullptr) {
    application_->readInterestRefreshed(peerIndex, resource, false);
  }
  session->ackResults[static_cast<uint8_t>(sequence & 63U)] = kResultTrue;
  size_t responseLength = 0;
  if (buildResourceApplication(kMessageClassSuplaCall,
                               kSuplaCallDeviceChannelValueChangedC, 0,
                               resource, applicationBuffer_ + payloadOffset,
                               payloadLength, &responseLength)) {
    (void)enqueueApplication(peerIndex, applicationBuffer_, responseLength,
                             true);
  }
  sendAck(peerIndex, session, sequence, kResultTrue);
}

void Runtime::handleSuplaCall(uint8_t peerIndex, SessionEntry *session,
                              uint32_t sequence,
                              const ApplicationDataView &application,
                              bool duplicate) {
  ResourceId resource = {};
  const uint8_t *payload = nullptr;
  size_t payloadLength = 0;
  if (!resourceFromApplication(application, &resource, &payload,
                               &payloadLength)) {
    ++diagnostics_.invalidDataDrop;
    return;
  }
  const PeerRecord *peer = peers_->get(peerIndex);
  if (application.messageType == kSuplaCallChannelSetValue) {
    if (application.flags != kAckRequired || payloadLength != 17 ||
        payload[4] != kChannelNumberUnresolved) {
      ++diagnostics_.invalidDataDrop;
      return;
    }
    uint8_t result = kResultNotAllowed;
    if (isLocalSource(peer) &&
        peers_->authorize(peerIndex, resource, kPermissionControl)) {
      if (duplicate) {
        ++diagnostics_.controlDuplicateSuppressed;
        const uint8_t cacheIndex = static_cast<uint8_t>(sequence & 63U);
        result = session->ackResults[cacheIndex];
        sendAck(peerIndex, session, sequence, result);
        return;
      }
      result = application_ == nullptr ? kResultChannelNotFound
          : application_->dispatchControl(resource, application.messageType,
                                           payload, payloadLength);
      ++diagnostics_.controlDispatched;
    } else {
      ++diagnostics_.aclReject;
      if (duplicate) {
        ++diagnostics_.controlDuplicateSuppressed;
        const uint8_t cacheIndex = static_cast<uint8_t>(sequence & 63U);
        result = session->ackResults[cacheIndex];
        sendAck(peerIndex, session, sequence, result);
        return;
      }
    }
    const uint8_t cacheIndex = static_cast<uint8_t>(sequence & 63U);
    session->ackResults[cacheIndex] = result;
    sendAck(peerIndex, session, sequence, result);
    return;
  }
  if (application.messageType == kSuplaCallDeviceChannelValueChangedC) {
    if (application.flags != 0 || payloadLength != 14 ||
        payload[0] != kChannelNumberUnresolved || duplicate ||
        !isLocalDestination(peer) ||
        !peers_->authorize(peerIndex, resource, kPermissionRead)) {
      ++diagnostics_.invalidDataDrop;
      return;
    }
    noteReadState(peerIndex, resource);
    if (application_ != nullptr) {
      application_->receiveState(peerIndex, resource, application.messageType,
                                 payload, payloadLength);
    }
    ++diagnostics_.stateNotificationRx;
    return;
  }
  if (application.messageType ==
      kSuplaCallDeviceChannelExtendedValueChanged) {
    if (application.flags != 0 || payloadLength < 6 ||
        payload[0] != kChannelNumberUnresolved || duplicate ||
        getSuplaUint32(payload + 2) != payloadLength - 6 ||
        !isLocalDestination(peer) ||
        !peers_->authorize(peerIndex, resource, kPermissionRead)) {
      ++diagnostics_.invalidDataDrop;
      return;
    }
    noteReadState(peerIndex, resource);
    if (application_ != nullptr) {
      application_->receiveState(peerIndex, resource, application.messageType,
                                 payload, payloadLength);
    }
    ++diagnostics_.stateNotificationRx;
    return;
  }
  if (application.messageType == kSuplaCallActionTrigger) {
    if (application.flags != kAckRequired || payloadLength != 15 ||
        payload[0] != kChannelNumberUnresolved ||
        !isLocalDestination(peer) ||
        !peers_->authorize(peerIndex, resource, kPermissionAction)) {
      ++diagnostics_.invalidDataDrop;
      return;
    }
    const uint8_t cacheIndex = static_cast<uint8_t>(sequence & 63U);
    if (duplicate) {
      const uint8_t result = session->ackResults[cacheIndex];
      if (result != 0) {
        sendAck(peerIndex, session, sequence, result);
      }
      return;
    }
    if (application_ != nullptr) {
      application_->receiveAction(peerIndex, resource, application.messageType,
                                  payload, payloadLength);
    }
    ++diagnostics_.actionRx;
    session->ackResults[cacheIndex] = kResultTrue;
    sendAck(peerIndex, session, sequence, kResultTrue);
    return;
  }
  ++diagnostics_.invalidDataDrop;
}

void Runtime::sendAck(uint8_t peerIndex, SessionEntry *session,
                      uint32_t sequence, uint8_t result) {
  if (session == nullptr || !session->used) {
    return;
  }
  putUint32(applicationBuffer_ + 6, sequence);
  applicationBuffer_[10] = result;
  applicationBuffer_[0] = kMessageClassNative;
  putUint32(applicationBuffer_ + 1, 1);
  applicationBuffer_[5] = 0;
  if (sendProtected(peerIndex, &session->transmit, session->sessionId,
                    &session->nextTransmitSequence,
                    &session->lastActivityMs, applicationBuffer_, 11, false,
                    ResourceId())) {
    ++diagnostics_.ackTx;
  }
}

void Runtime::noteReadState(uint8_t peerIndex, const ResourceId &resource) {
  for (uint8_t i = 0; i < SUPLAN_MAX_RETRY_SLOTS; ++i) {
    RetryEntry *retry = &retries_[i];
    if (!retry->isUsed() || retry->peerIndex != peerIndex ||
        (retry->flags & (kRetryReadRequest | kRetryReadExpectsState)) !=
            (kRetryReadRequest | kRetryReadExpectsState) ||
        retry->resource.type != resource.type ||
        retry->resource.id != resource.id) {
      continue;
    }
    retry->flags |= kRetryReadStateReceived;
    if ((retry->flags & kRetryAwaitingReadState) != 0) {
      completeRead(peerIndex, resource);
      clearRetryEntry(retry);
    }
  }
}

void Runtime::processFlood() {
  for (uint8_t sent = 0; flood_.active && sent < kMaxDatagramsPerIterate;
       ++sent) {
    bool attempted = false;
    if (flood_.kind == kFloodInvalidLocate) {
      uint8_t frame[50] = {};
      frame[0] = kVersion;
      frame[1] = kFrameLocate;
      attempted = datagrams_->sendLocateMulticast(frame, sizeof(frame));
    } else if (flood_.kind == kFloodInvalidSession) {
      const PeerRecord *peer = peers_->get(flood_.peerIndex);
      if (peer != nullptr &&
          peer->endpointState == kPeerEndpointAuthenticated) {
        attempted = datagrams_->sendUnicast(peer->endpoint,
                                            flood_.invalidSessionFrame,
                                            sizeof(flood_.invalidSessionFrame));
      }
    } else if (flood_.kind == kFloodInvalidData) {
      SessionEntry *session = nullptr;
      for (uint8_t i = 0; i < SUPLAN_MAX_ACTIVE_SESSIONS; ++i) {
        if (sessions_[i].used &&
            sessions_[i].peerIndex == flood_.peerIndex) {
          session = &sessions_[i];
          break;
        }
      }
      if (session != nullptr) {
        applicationBuffer_[0] = kMessageClassNative;
        putUint32(applicationBuffer_ + 1, kNativeReadResource);
        applicationBuffer_[5] = 0;
        applicationBuffer_[6] = kResourceTypeChannel;
        putUint32(applicationBuffer_ + 7, 0);
        hooks_.corruptNextDataTagTx = 1;
        attempted = sendProtected(
            flood_.peerIndex, &session->transmit, session->sessionId,
            &session->nextTransmitSequence, &session->lastActivityMs,
            applicationBuffer_, 11, false, ResourceId());
      }
    }
    --flood_.remaining;
    if (!attempted) {
      ++diagnostics_.invalidDataDrop;
    }
    if (flood_.remaining == 0) {
      flood_.active = false;
    }
  }
}

void Runtime::iterate() {
  if (crypto_ == nullptr || datagrams_ == nullptr ||
      peers_ == nullptr || application_ == nullptr || processing_) {
    return;
  }
  const uint32_t now = datagrams_->nowMs();
  iterateRecovery(now);
  expireControls(now);
  processFlood();
  if (fragmentReassembler_.expire(now, kReassemblyTimeoutMs)) {
    ++diagnostics_.reassemblyExpired;
  }
  for (uint8_t i = 0; i < SUPLAN_MAX_OUTSTANDING_LOCATES; ++i) {
    if (locates_[i].used &&
        static_cast<int32_t>(now - locates_[i].expiresAtMs) >= 0) {
      locates_[i].used = false;
    }
  }
  for (uint8_t i = 0; i < SUPLAN_MAX_PENDING_HANDSHAKES; ++i) {
    PendingHandshake *attempt = &pending_[i];
    if (!attempt->used) {
      continue;
    }
    if (attempt->initiator && !hooks_.pauseSessionInitRetries &&
        attempt->attempts < kSessionInitMaxAttempts &&
        static_cast<uint32_t>(now - attempt->lastTransmitMs) >=
            kSessionInitRetryMs) {
      if (sendRaw(attempt->endpoint, attempt->initFrame,
                  sizeof(attempt->initFrame), 0)) {
        ++diagnostics_.sessionInitTx;
      }
      ++attempt->attempts;
      attempt->lastTransmitMs = now;
    }
    if (static_cast<int32_t>(now - attempt->expiresAtMs) >= 0) {
      const bool initiator = attempt->initiator;
      const uint8_t peerIndex = attempt->peerIndex;
      clearPendingHandshake(attempt);
      if (initiator) {
        PeerRecord *peer = peers_->get(peerIndex);
        if (peer != nullptr) {
          peer->endpoint = Endpoint();
          peer->endpointState = kPeerEndpointNone;
        }
      }
    }
  }
  iterateRecovery(now);
  pruneInterests(now);
  for (uint8_t i = 0; i < SUPLAN_MAX_RETRY_SLOTS; ++i) {
    RetryEntry *retry = &retries_[i];
    const bool actionDelivery =
        retry->actionDelivery;
    if (retry->isUsed() && actionDelivery &&
        (!isLocalSource(peers_->get(retry->peerIndex)) ||
         !peers_->authorize(retry->peerIndex, retry->resource,
                            kPermissionAction) ||
         static_cast<int32_t>(now - getActionDeadline(
             retry->frame, sizeof(retry->frame))) >= 0)) {
      const uint8_t peerIndex = retry->peerIndex;
      clearRetryEntry(retry);
      cancelPeerRecoveryIfIdle(peerIndex);
      continue;
    }
    if (retry->isUsed() &&
        (retry->flags & kRetryAwaitingReadState) != 0) {
      if (static_cast<uint32_t>(now - retry->lastTransmitMs) >=
          kReadStateResponseTimeoutMs) {
        const uint8_t peerIndex = retry->peerIndex;
        const ResourceId resource = retry->resource;
        clearRetryEntry(retry);
        const int slot = findReadDependency(peerIndex, resource);
        if (slot >= 0) {
          dependencies_[slot].pending = false;
          (void)queueRead(peerIndex, resource);
        }
      }
      continue;
    }
    if (retry->isUsed() && retry->awaitingSession) {
      bool sessionActive = false;
      for (uint8_t s = 0; s < SUPLAN_MAX_ACTIVE_SESSIONS; ++s) {
        if (sessions_[s].used &&
            sessions_[s].peerIndex == retry->peerIndex) {
          sessionActive = true;
          break;
        }
      }
      if (!sessionActive) {
        startHandshake(retry->peerIndex);
      }
      continue;
    }
    if (!retry->isUsed() ||
        static_cast<uint32_t>(now - retry->lastTransmitMs) <
            kAckRetryMs) {
      continue;
    }
    if (retry->attempts >= kAckMaxAttempts) {
      if (actionDelivery) {
        clearRetryEntry(retry);
      } else {
        const uint8_t peerIndex = retry->peerIndex;
        const uint64_t sessionId = retry->sessionId;
        recoverSession(peerIndex, sessionId);
      }
      continue;
    }
    PeerRecord *peer = peers_->get(retry->peerIndex);
    if (peer != nullptr &&
        peer->endpointState == kPeerEndpointAuthenticated) {
      fragmentEndpoint_ = peer->endpoint;
      const size_t maxPayload = hooks_.maxDatagramPayload <
              datagrams_->maxDatagramPayload()
          ? hooks_.maxDatagramPayload : datagrams_->maxDatagramPayload();
      fragmentOrdinal_ = 0;
      if (dataTransmitObserver_ != nullptr) {
        dataTransmitObserver_(dataTransmitContext_, peer->endpoint,
                              retry->frame, retry->frameLength);
      }
      if (hooks_.failNextDataTx != 0) {
        --hooks_.failNextDataTx;
      } else if (fragmentSender_.send(
                     retry->frame, retry->frameLength, maxPayload,
                     UINT32_C(0xC3000000) |
                         (nextFrameId_++ & UINT32_C(0x00FFFFFF)),
                     fragmentDatagram, this)) {
        ++diagnostics_.dataTx;
      }
      ++retry->attempts;
      retry->lastTransmitMs = now;
      ++diagnostics_.retryTx;
    }
  }

  for (uint8_t i = 0; i < kMaxDatagramsPerIterate; ++i) {
    Endpoint source = {};
    const int length = datagrams_->pollReceive(transmitFrame_,
                                               sizeof(transmitFrame_), &source);
    if (length <= 0) {
      break;
    }
    processDatagram(source, transmitFrame_, static_cast<size_t>(length));
  }
  drainDeferred();
  for (uint8_t i = 0; i < peers_->size(); ++i) {
    cancelPeerRecoveryIfIdle(i);
  }
  updatePoolHighWater();
}

}  // namespace SupLan
}  // namespace Supla

// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "suplan_runtime.h"

#include <string.h>

#include "suplan_crypto.h"
#include "suplan_wire.h"

namespace Supla {
namespace SupLan {

namespace {
static const uint8_t kResultNotAllowed = 24;
static const uint8_t kResultChannelNotFound = 25;
static const uint32_t kLocateReplyRateLimitMs = 100;
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
}  // namespace

Runtime::Runtime(CryptoPort *crypto, RandomPort *random,
                 DatagramPort *datagrams, ApplicationPort *application,
                 PeerTable *peers, const NodeAddress &localAddress,
                 uint8_t suplaProtoVersion)
    : crypto_(crypto), random_(random), datagrams_(datagrams),
      application_(application), peers_(peers), localAddress_(localAddress),
      suplaProtoVersion_(suplaProtoVersion), processing_(false),
      nextFrameId_(UINT32_C(0xC3000000)), fragmentOrdinal_(0), sessions_(),
      pending_(), locates_(),
      interests_(), retries_(), deferred_(), flood_(), fragmentSender_(),
      fragmentReassembler_(), diagnostics_(), poolHighWater_(), hooks_(),
      applicationBuffer_(), transmitFrame_(),
      fragmentEndpoint_() {
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
  memset(&session->transmit, 0, sizeof(session->transmit));
  memset(&session->receive, 0, sizeof(session->receive));
  session->nextTransmitSequence = 0;
  session->receiveReplay.reset();
  session->lastActivityMs = 0;
  memset(session->controlResults, 0, sizeof(session->controlResults));
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
  pending->lastTransmitMs = 0;
  pending->expiresAtMs = 0;
  pending->attempts = 0;
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
        if (retries_[r].used &&
            retries_[r].sessionId == sessions_[i].sessionId) {
          retries_[r].used = false;
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
        if (retries_[r].used &&
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
      if (retries_[r].used && retries_[r].sessionId == evictedSession) {
        retries_[r].used = false;
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
    if (retries_[i].used && retries_[i].peerIndex == peerIndex &&
        retries_[i].sessionId == sessionId &&
        retries_[i].sequence == sequence) {
      return i;
    }
  }
  return -1;
}

int Runtime::findFreeRetry() const {
  for (uint8_t i = 0; i < SUPLAN_MAX_RETRY_SLOTS; ++i) {
    if (!retries_[i].used) {
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
    used[7] += retries_[i].used ? 1 : 0;
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
    result.retries.used += retries_[i].used ? 1 : 0;
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

void Runtime::startLocate(uint8_t peerIndex) {
  PeerRecord *peer = peers_->get(peerIndex);
  if (peer == nullptr || !peers_->hasActiveGrants(peerIndex)) {
    return;
  }
  PeerMaterial material = {};
  if (!peers_->materialFor(crypto_, peerIndex, &material)) {
    return;
  }
  if (peer->endpointState != kPeerEndpointNone) {
    startHandshake(peerIndex);
    return;
  }
  const uint32_t now = datagrams_->nowMs();
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
  uint8_t nonce[kNonceSize];
  uint8_t mac[kPeerLocatorSize];
  uint8_t frame[50];
  if (!random_->fillRandom(nonce, sizeof(nonce)) ||
      !locateQueryMac(crypto_, &material, kVersion, kFrameLocate,
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
  } else {
    locates_[slot].used = false;
  }
  memset(nonce, 0, sizeof(nonce));
  memset(mac, 0, sizeof(mac));
  updatePoolHighWater();
}

void Runtime::startHandshake(uint8_t peerIndex) {
  PeerRecord *peer = peers_->get(peerIndex);
  if (peer == nullptr || !peers_->hasActiveGrants(peerIndex) ||
      datagrams_ == nullptr || random_ == nullptr || crypto_ == nullptr) {
    return;
  }
  PeerMaterial material = {};
  if (!peers_->materialFor(crypto_, peerIndex, &material)) {
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
  if (!random_->fillRandom(init.ni, sizeof(init.ni))) {
    attempt->used = false;
    return;
  }
  memcpy(init.peerLocator, material.peerLocator,
         kPeerLocatorSize);
  if (!encodeSessionInit(crypto_, material.initMacKey, &init,
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
  size_t length = kApplicationHeaderSize + kResourceHeaderSize;
  applicationBuffer_[0] = kMessageClassNative;
  putUint32(applicationBuffer_ + 1, kNativeReadResource);
  applicationBuffer_[5] = 0;
  if (!encodeResourceId(resource.type, resource.id,
                        applicationBuffer_ + kApplicationHeaderSize)) {
    return false;
  }
  return enqueueApplication(peerIndex, applicationBuffer_, length, true);
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
      !peers_->authorize(peerIndex, resource, kPermissionAction)) {
    return false;
  }
  if (!buildResourceApplication(kMessageClassSuplaCall,
                                kSuplaCallActionTrigger, 0, resource,
                                suplaPayload, payloadLength, nullptr)) {
    return false;
  }
  return enqueueApplication(peerIndex, applicationBuffer_,
                            kApplicationHeaderSize + kResourceHeaderSize +
                                payloadLength, false);
}

bool Runtime::forgetSession(uint8_t peerIndex) {
  bool removed = false;
  for (uint8_t i = 0; i < SUPLAN_MAX_ACTIVE_SESSIONS; ++i) {
    if (sessions_[i].used && sessions_[i].peerIndex == peerIndex) {
      const uint64_t sessionId = sessions_[i].sessionId;
      clearSessionEntry(&sessions_[i]);
      for (uint8_t r = 0; r < SUPLAN_MAX_RETRY_SLOTS; ++r) {
        if (retries_[r].used && retries_[r].sessionId == sessionId) {
          retries_[r].used = false;
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
    if (!peers_->materialFor(crypto_, peerIndex, &material)) {
      memset(&flood_, 0, sizeof(flood_));
      return false;
    }
    memcpy(init.peerLocator, material.peerLocator, kPeerLocatorSize);
    init.suplaProtoVersionMax = suplaProtoVersion_;
    init.rxMaxReassembledFrame = SUPLAN_RX_MAX_REASSEMBLED_FRAME;
    if (!random_->fillRandom(init.ni, sizeof(init.ni)) ||
        !encodeSessionInit(crypto_, material.initMacKey, &init,
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
                                 size_t length, bool mayEstablishSession) {
  if (data == nullptr || length == 0 || length > sizeof(deferred_[0].data) ||
      peers_->get(peerIndex) == nullptr) {
    ++diagnostics_.deferredQueueOverflow;
    return false;
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
  updatePoolHighWater();
  if (!processing_) {
    drainDeferred();
  }
  return true;
}

void Runtime::drainDeferred() {
  if (processing_) {
    return;
  }
  for (uint8_t i = 0; i < SUPLAN_MAX_DEFERRED_APP_EVENTS; ++i) {
    DeferredApplication *queued = &deferred_[i];
    if (!queued->used) {
      continue;
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
    if (parsed && app.messageClass == kMessageClassSuplaCall) {
      const uint8_t *payload = nullptr;
      size_t payloadLength = 0;
      if (resourceFromApplication(app, &resource, &payload, &payloadLength)) {
        (void)payload;
        (void)payloadLength;
      }
    }
    SessionEntry *session = &sessions_[sessionIndex];
    if (sendProtected(queued->peerIndex, &session->transmit,
                      session->sessionId, &session->nextTransmitSequence,
                      &session->lastActivityMs, queued->data, queued->length,
                      parsed && app.flags == kAckRequired, resource)) {
      queued->used = false;
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
                            const ResourceId &resource) {
  PeerRecord *peer = peers_->get(peerIndex);
  if (peer == nullptr ||
      peer->endpointState != kPeerEndpointAuthenticated) {
    return false;
  }
  return sendProtectedToEndpoint(
      peerIndex, peer->endpoint, transmitKeys, sessionId,
      nextTransmitSequence, lastActivityMs, applicationData,
      applicationLength, ackRequired, resource);
}

bool Runtime::sendProtectedToEndpoint(
    uint8_t peerIndex, const Endpoint &endpoint,
    const DirectionalKeys *transmitKeys, uint64_t sessionId,
    uint32_t *nextTransmitSequence, uint32_t *lastActivityMs,
    const uint8_t *applicationData, size_t applicationLength,
    bool ackRequired, const ResourceId &resource) {
  if (transmitKeys == nullptr || nextTransmitSequence == nullptr ||
      *nextTransmitSequence == UINT32_MAX ||
      applicationLength > SUPLAN_MAX_APPLICATION_BYTES) {
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
  if ((frameKind == kTxKindAck && hooks_.dropNextAckTx != 0) ||
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
  if (!sent) {
    return false;
  }
  ++diagnostics_.dataTx;
  if (applicationData[0] == kMessageClassSuplaCall) {
    const uint32_t messageType = getUint32(applicationData + 1);
    if (messageType == kSuplaCallDeviceChannelValueChangedC ||
        messageType == kSuplaCallDeviceChannelExtendedValueChanged) {
      ++diagnostics_.stateNotificationTx;
    } else if (messageType == kSuplaCallActionTrigger) {
      ++diagnostics_.actionTx;
    }
  }
  if (lastActivityMs != nullptr) {
    *lastActivityMs = datagrams_->nowMs();
  }
  if (ackRequired) {
    RetryEntry *retry = &retries_[retryIndex];
    retry->used = true;
    retry->peerIndex = peerIndex;
    retry->resource = resource;
    retry->sessionId = sessionId;
    retry->sequence = sequence;
    retry->lastTransmitMs = datagrams_->nowMs();
    retry->attempts = 1;
    retry->frameLength = static_cast<uint16_t>(frameLength);
    memcpy(retry->frame, transmitFrame_, frameLength);
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
  // A LOCATE query is sent by the immutable Destination role to discover its
  // Source. Ignore looped-back queries and queries sent by the Source itself.
  // This also prevents multicast loopback from replacing the remote endpoint
  // with the local socket address.
  if (peer == nullptr || !isLocalSource(peer)) {
    ++diagnostics_.invalidLocateDrop;
    return;
  }
  PeerMaterial material = {};
  if (!peers_->materialFor(crypto_, static_cast<uint8_t>(peerIndex),
                           &material)) {
    ++diagnostics_.invalidLocateDrop;
    return;
  }
  uint8_t expected[kPeerLocatorSize];
  if (peer == nullptr ||
      !locateQueryMac(crypto_, &material, kVersion, kFrameLocate,
                      nonce, expected) ||
      !equalBytesConstantTime(expected, mac, sizeof(expected))) {
    ++diagnostics_.invalidLocateDrop;
    return;
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
  if (!locateReplyMac(crypto_, &material, kVersion, kFrameLocateReply,
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
        !peers_->materialFor(crypto_, locate->peerIndex, &material) ||
        !locateReplyMac(crypto_, &material, kVersion,
                        kFrameLocateReply, nonce, expected) ||
        !equalBytesConstantTime(expected, mac, sizeof(expected))) {
      ++diagnostics_.invalidLocateDrop;
      return;
    }
    peer->endpoint = source;
    peer->endpointState = kPeerEndpointLocateCandidate;
    ++diagnostics_.locateReplyRx;
    const uint8_t peerIndex = locate->peerIndex;
    locate->used = false;
    startHandshake(peerIndex);
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
      !peers_->materialFor(crypto_, peerIndex, &material) ||
      !decodeSessionInit(crypto_, material.initMacKey, data, length, &init)) {
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
  if (!random_->fillRandom(accept.nr, sizeof(accept.nr)) ||
      !random_->fillRandom(sessionIdBytes, sizeof(sessionIdBytes))) {
    clearPendingHandshake(response);
    return;
  }
  accept.sessionId = getUint64(sessionIdBytes);
  if (accept.sessionId == 0) {
    accept.sessionId = 1;
  }
  response->sessionId = accept.sessionId;
  if (!encodeSessionAccept(crypto_, material.acceptMacKey,
                           response->initFrame, &accept,
                           response->acceptFrame) ||
      !deriveSessionKeys(crypto_, material.peerKey,
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
      !peers_->materialFor(crypto_, attempt->peerIndex, &material) ||
      !decodeSessionInit(crypto_, material.initMacKey, attempt->initFrame,
                         sizeof(attempt->initFrame), &init) ||
      !decodeSessionAccept(crypto_, material.acceptMacKey,
                           attempt->initFrame, &init, data, length,
                           &accept)) {
    ++diagnostics_.invalidSessionDrop;
    return;
  }
  memcpy(attempt->acceptFrame, data, sizeof(attempt->acceptFrame));
  attempt->sessionId = accept.sessionId;
  if (!deriveSessionKeys(crypto_, material.peerKey,
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
  ++diagnostics_.sessionAcceptRx;
  const uint8_t peerIndex = attempt->peerIndex;
  const int sessionIndex = allocateSession(peerIndex);
  if (sessionIndex < 0) {
    clearPendingHandshake(attempt);
    return;
  }
  SessionEntry *session = &sessions_[sessionIndex];
  session->sessionId = attempt->sessionId;
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
  SessionEntry *session = &sessions_[sessionIndex];
  session->lastActivityMs = datagrams_->nowMs();
  if (result == kProtectedDataDuplicate) {
    ++diagnostics_.dataDuplicate;
  } else {
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
  if (hooks_.dropNextDataRx != 0) {
    --hooks_.dropNextDataRx;
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

bool Runtime::addInterest(uint8_t peerIndex, const ResourceId &resource) {
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
  (void)sequence;
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
      const ResourceId resource = retries_[retryIndex].resource;
      retries_[retryIndex].used = false;
      ++diagnostics_.ackRx;
      if (application_ != nullptr) {
        application_->operationAcknowledged(peerIndex, resource,
                                            ackedSequence, result);
      }
      updatePoolHighWater();
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
  if (application.messageType != kNativeReadResource || duplicate ||
      application.flags != 0 || application.bodyLength != kResourceHeaderSize) {
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
  size_t responseLength = 0;
  if (buildResourceApplication(kMessageClassSuplaCall,
                               kSuplaCallDeviceChannelValueChangedC, 0,
                               resource, applicationBuffer_ + payloadOffset,
                               payloadLength, &responseLength)) {
    (void)enqueueApplication(peerIndex, applicationBuffer_, responseLength,
                             true);
  }
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
        result = session->controlResults[cacheIndex];
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
        result = session->controlResults[cacheIndex];
        sendAck(peerIndex, session, sequence, result);
        return;
      }
    }
    const uint8_t cacheIndex = static_cast<uint8_t>(sequence & 63U);
    session->controlResults[cacheIndex] = result;
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
    if (application_ != nullptr) {
      application_->receiveState(peerIndex, resource, application.messageType,
                                 payload, payloadLength);
    }
    ++diagnostics_.stateNotificationRx;
    return;
  }
  if (application.messageType == kSuplaCallActionTrigger) {
    if (application.flags != 0 || payloadLength != 15 ||
        payload[0] != kChannelNumberUnresolved || duplicate ||
        !isLocalDestination(peer) ||
        !peers_->authorize(peerIndex, resource, kPermissionAction)) {
      ++diagnostics_.invalidDataDrop;
      return;
    }
    if (application_ != nullptr) {
      application_->receiveAction(peerIndex, resource, application.messageType,
                                  payload, payloadLength);
    }
    ++diagnostics_.actionRx;
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
  if (crypto_ == nullptr || random_ == nullptr || datagrams_ == nullptr ||
      peers_ == nullptr || application_ == nullptr || processing_) {
    return;
  }
  const uint32_t now = datagrams_->nowMs();
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
      clearPendingHandshake(attempt);
    }
  }
  for (uint8_t i = 0; i < SUPLAN_MAX_RUNTIME_INTERESTS; ++i) {
    if (interests_[i].used &&
        static_cast<uint32_t>(now - interests_[i].lastRefreshMs) >=
            kRuntimeInterestTimeoutMs) {
      memset(&interests_[i], 0, sizeof(interests_[i]));
    }
  }
  for (uint8_t i = 0; i < SUPLAN_MAX_RETRY_SLOTS; ++i) {
    RetryEntry *retry = &retries_[i];
    if (!retry->used || static_cast<uint32_t>(now - retry->lastTransmitMs) <
            kAckRetryMs) {
      continue;
    }
    if (retry->attempts >= kAckMaxAttempts) {
      const uint8_t peerIndex = retry->peerIndex;
      (void)forgetSession(peerIndex);
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
      (void)fragmentSender_.send(
          retry->frame, retry->frameLength, maxPayload,
          UINT32_C(0xC3000000) |
              (nextFrameId_++ & UINT32_C(0x00FFFFFF)),
          fragmentDatagram, this);
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
  updatePoolHighWater();
}

}  // namespace SupLan
}  // namespace Supla

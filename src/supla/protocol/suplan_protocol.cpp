// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright 2026 AC SOFTWARE SP. Z O.O.

#ifndef ARDUINO_ARCH_AVR

#include <supla/protocol/suplan_protocol.h>

#include <string.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <supla/debug/debug_log.h>
#include <supla/log_wrapper.h>
#include <supla/time.h>
#include <supla/crypto.h>
#include <supla/channels/channel_state.h>
#include <supla/channels/channel_handle.h>

#include <SuplaDevice.h>
#include <supla/at_channel.h>
#include <supla/channels/channel.h>
#include <supla/element.h>
#include <supla/protocol/supla_srpc.h>
#include <supla/storage/storage.h>
#include <supla/suplan/suplan_server_associations.h>
#include <supla/suplan/bootstrap.h>
#include <supla/suplan/suplan_server_identity.h>

#include <new>

namespace Supla {
namespace Protocol {
namespace {
// Runtime diagnostics must not allocate, including on Arduino where the
// general flash-format logger uses String/heap. Reuse the existing log sinks.
#ifndef SUPLA_DEVICE_ESP32
#ifdef ARDUINO
using DiagnosticFormat = const __FlashStringHelper *;
#else
using DiagnosticFormat = const char *;
#endif
void diagnosticLog(int priority, DiagnosticFormat format, ...) {
#ifdef SUPLA_DISABLE_LOGS
  (void)priority;
  (void)format;
#else
  if (!SUPLA_LOG_IS_ENABLED(priority)) return;
  char message[192];
  va_list args;
  va_start(args, format);
#ifdef ARDUINO
  const int result = vsnprintf_P(message, sizeof(message),
      reinterpret_cast<PGM_P>(format), args);
#else
  const int result = vsnprintf(message, sizeof(message), format, args);
#endif
  va_end(args);
  if (result < 0) return;
  supla_vlog(priority, message);
  supla_debug_log_write(priority, message);
#endif
}
#define SUPLAN_DIAG_DEBUG(format, ...) \
  do { \
    if (SUPLA_LOG_IS_ENABLED(LOG_DEBUG)) \
      diagnosticLog(LOG_DEBUG, F(format), ##__VA_ARGS__); \
  } while (0)
#define SUPLAN_DIAG_INFO(format, ...) \
  do { \
    if (SUPLA_LOG_IS_ENABLED(LOG_INFO)) \
      diagnosticLog(LOG_INFO, F(format), ##__VA_ARGS__); \
  } while (0)
#define SUPLAN_DIAG_WARNING(format, ...) \
  do { \
    if (SUPLA_LOG_IS_ENABLED(LOG_WARNING)) \
      diagnosticLog(LOG_WARNING, F(format), ##__VA_ARGS__); \
  } while (0)
#else
// The IDF SDK adapter already uses a fixed stack buffer.
#define SUPLAN_DIAG_DEBUG SUPLA_LOG_DEBUG
#define SUPLAN_DIAG_INFO SUPLA_LOG_INFO
#define SUPLAN_DIAG_WARNING SUPLA_LOG_WARNING
#endif

// Only the D2D egress boundary substitutes validity. The common builder keeps
// D2S/local semantics. READ observes acquisition state; never calls a sensor.
bool buildD2dSnapshot(Channel *channel, TDS_SuplaDeviceChannel_E *snapshot) {
  if (!buildChannelSnapshot(channel, snapshot)) return false;
  snapshot->ValueValidityTimeSec = channel->getSupLanValidityTimeSec();
  const ChannelState view(snapshot, true);
  if (view.availableFor(ChannelCapability::Temperature) &&
      !isfinite(view.getValueDouble()))
    snapshot->Offline = SUPLA_CHANNEL_OFFLINE_FLAG_ONLINE_BUT_NOT_AVAILABLE;
  return true;
}
bool sourceControlAvailable(Element *element) {
  TDS_SuplaDeviceChannel_E snapshot = {};
  return element && buildChannelSnapshot(element->getChannel(), &snapshot) &&
         snapshot.Default != SUPLA_CHANNELFNC_NONE;
}
}  // namespace

SupLan::SupLan(SuplaDeviceClass *sdc, Supla::SupLan::PeerTable *peers,
               SupLanApplicationEventHandler eventHandler,
               void *eventContext)
    : ProtocolLayer(sdc), peers_(peers), runtime_(nullptr), transport_(nullptr),
      eventHandler_(eventHandler), eventContext_(eventContext), enabled_(true),
      transportWasOpen_(false) {
  configEmpty = false;
}

#ifdef SUPLAN_TESTING
SupLan::SupLan(SuplaDeviceClass *sdc, Supla::SupLan::PeerTable *peers,
               const SupLanResourceMapping *mappings, uint8_t mappingCount,
               SupLanApplicationEventHandler eventHandler, void *eventContext)
    : SupLan(sdc, peers, eventHandler, eventContext) {
  mappings_ = mappings;
  mappingCount_ = mappingCount;
}
#endif  // SUPLAN_TESTING

SupLan::~SupLan() {
  if (resources_) delete resources_->bindings;
  delete resources_;
}

void SupLan::attachRuntime(Supla::SupLan::Runtime *runtime) {
  runtime_ = runtime;
  if (resources_) resources_->attachRuntime(runtime);
  if (resources_ && resources_->bindings)
    resources_->bindings->attachRuntime(runtime);
}

void SupLan::attachTransportLifecycle(
    SupLanTransportLifecycle *transport) {
  if (transport_ != nullptr && transport_ != transport &&
      transport_->isOpen()) {
    transport_->close();
  }
  transport_ = transport;
  if (transport_ != nullptr) {
    transport_->resetOpenRetry();
  }
  transportWasOpen_ = transport_ != nullptr && transport_->isOpen();
}

void SupLan::setEnabled(bool enabled) {
  enabled_ = enabled;
}

void SupLan::onInit() {
  if (associations()) {
    resources_->presence().resetPending_ = true;
    resources_->presence().presenceCursor_ = 0;
    uint8_t jitter = 0;
    (void)Supla::Crypto::fillRandom(&jitter, sizeof(jitter));
    resources_->presence().nextPresenceMs_ = millis() + 100 + jitter;
  }
  if (associations() && sdc && sdc->getSrpcLayer()) {
    associations()->load(Supla::Storage::ConfigInstance(),
                         &sdc->getSrpcLayer()->serverIdentity());
  }
}

void SupLan::attachServerAssociations(
    Supla::Device::ServerAssociations *state) {
  serverAssociationRequired_ = state != nullptr;
  if (state && !resources_) {
    resources_ = Device::allocateSupLan<Device::RemoteResourceManager>(
        peers_, runtime_, this);
  }
  if (resources_) resources_->associations = state;
  if (state && resources_ && !resources_->bindings) {
    resources_->bindings = Device::allocateSupLan<
        Device::ResourceBindingManager>(peers_, runtime_, resources_, this);
  }
  if (resources_ && resources_->bindings)
    resources_->bindings->attachAssociations(state);
}

bool SupLan::setSuplanSourceAssociation(
    const TSDS_SuplaSetSuplanSourceAssociation &request,
    TDS_SuplaSetSuplanSourceAssociationResult *result) {
  if (!associations() || !enabled_) return false;
  *result = associations()->accept(request);
  return true;
}

bool SupLan::setSuplanDestinationAssociation(
    const TSDS_SuplaSetSuplanDestinationAssociation &request,
    TDS_SuplaSetSuplanDestinationAssociationResult *result) {
  if (!associations() || !enabled_) return false;
  *result = associations()->accept(request);
  if (result->Result == SUPLA_SUPLAN_RESULT_OK && resources_) {
    resources_->authorizationChanged();
  }
  return true;
}

void SupLan::suplanIdentityChanged() {
  if (!associations()) return;
  associations()->identityChanged();
  const auto identity = associations()->identity();
  if (identity && identity->fullScopeFenced() && resources_) {
    if (resources_->bindings) resources_->bindings->invalidateServerScope();
    resources_->invalidateServerScope();
  } else if (identity && identity->identityTransition() && resources_) {
    // Selective transitions keep demand/credentials, but invalidate borrowed
    // D2D snapshots immediately rather than waiting for a subsequent getter.
    resources_->authorizationChanged();
  }
}

bool SupLan::isSupLanServerCapable() const {
  return enabled_ && peers_ && runtime_ && runtime_->initialized() &&
      associations() && resources_->bindings && mapIsValid();
}

void SupLan::onFactoryReset() {
  disconnect();
  if (!associations() || !sdc || !sdc->getSrpcLayer()) return;
  if (resources_->bindings) resources_->bindings->attachAssociations(nullptr);
  associations()->load(Supla::Storage::ConfigInstance(),
                       &sdc->getSrpcLayer()->serverIdentity());
  if (resources_->bindings)
    resources_->bindings->attachAssociations(associations());
  resources_->authorizationChanged();
}

bool SupLan::onLoadConfig() {
  return true;
}

bool SupLan::verifyConfig() {
  return mapIsValid() && peers_ != nullptr && runtime_ != nullptr &&
         (!serverAssociationRequired_ || (resources_ && associations()));
}

bool SupLan::isEnabled() {
  return enabled_ && runtime_ != nullptr;
}

void SupLan::disconnect() {
  if (transport_ != nullptr && transport_->isOpen()) {
    transport_->close();
  }
  if (transport_ != nullptr) {
    transport_->resetOpenRetry();
  }
  transportWasOpen_ = false;
  clearPeerTransportState();
}

void SupLan::clearPeerTransportState() {
  if (peers_ == nullptr || runtime_ == nullptr) {
    return;
  }
  for (uint8_t i = 0; i < SUPLAN_MAX_PERSISTENT_PEERS; ++i) {
    (void)runtime_->forgetSession(i);
    runtime_->clearEndpoint(i);
  }
}

bool SupLan::isConfigEmpty() {
  return false;
}

bool SupLan::iterate(uint32_t nowMs) {
  if (!isEnabled()) {
    return false;
  }

  // Resource orchestration, especially SRPC STREAM, is independent of the
  // UDP transport being open. D2D refresh remains in its own backend.
  // Full-scope invalidation runs synchronously at migration notifications.
  // While fenced no SERVER peer can be admitted; repeating teardown here
  // cannot add safety. Keep ordinary live identity reconciliation otherwise.
  const auto identity = associations() ? associations()->identity() : nullptr;
  if (associations() && (!identity || !identity->fullScopeFenced()))
    associations()->identityChanged();
  if (resources_) resources_->iterate(nowMs);
  if (transport_ != nullptr) {
    if (!transport_->networkReady(nowMs)) {
      if (transport_->isOpen() || transportWasOpen_) {
        SUPLA_LOG_INFO("SupLAN UDP closed: network unavailable");
        transport_->close();
        clearPeerTransportState();
      }
      transportWasOpen_ = false;
      transport_->resetOpenRetry();
      return false;
    }

    if (!transport_->isOpen()) {
      if (transportWasOpen_) {
        clearPeerTransportState();
        transportWasOpen_ = false;
      }
      if (transport_->openIfDue(nowMs)) {
        transportWasOpen_ = transport_->isOpen();
      }
      if (!transport_->isOpen()) {
        return false;
      }
    }
    transportWasOpen_ = true;
  }

  if (resources_ && resources_->bindings) resources_->bindings->iterate(nowMs);
  if (resources_ &&
      (resources_->presence().resetPending_ ||
       resources_->presence().awakePending_) &&
      static_cast<int32_t>(nowMs - resources_->presence().nextPresenceMs_) >=
          0) {
    if (resources_->presence().presenceCursor_ < SUPLAN_MAX_PERSISTENT_PEERS) {
      const auto index = resources_->presence().presenceCursor_++;
      if (resources_->presence().resetPending_)
        runtime_->announcePresence(index, 0, true);
      if (resources_->presence().awakePending_) {
        if (runtime_->announcePresence(
                index, resources_->presence().awakeWindowMs_, false)) {
          resources_->presence().awakeUntilMs_ =
              nowMs + resources_->presence().awakeWindowMs_;
        }
        runtime_->publishCurrentInterests(index);
      }
      resources_->presence().nextPresenceMs_ = nowMs + 10;
    } else {
      resources_->presence().resetPending_ =
          resources_->presence().awakePending_ = false;
    }
  }
  runtime_->iterate();
  return true;
}

bool SupLan::isNetworkRestartRequested() {
  return false;
}

bool SupLan::protectsNetworkFromPeerRestart() {
  return transport_ != nullptr && isRegisteredAndReady();
}

uint32_t SupLan::getConnectionFailTime() {
  return 0;
}

bool SupLan::isRegisteredAndReady() {
  return isEnabled() && mapIsValid() && isTransportOpen();
}

bool SupLan::isTransportOpen() const {
  return transport_ == nullptr || transport_->isOpen();
}

#ifdef SUPLAN_TESTING
const SupLanResourceMapping *SupLan::findResource(uint32_t resourceId) const {
  for (uint8_t i = 0; i < mappingCount_; ++i) {
    if (mappings_[i].resourceId == resourceId) {
      return &mappings_[i];
    }
  }
  return nullptr;
}
#endif  // SUPLAN_TESTING

bool SupLan::mapIsValid() const {
#ifdef SUPLAN_TESTING
  if (associations() && !mappings_ && !mappingCount_) return true;
  if (mappings_ == nullptr || mappingCount_ == 0 ||
      mappingCount_ > kMaxResourceMappings) {
    return false;
  }
  for (uint8_t i = 0; i < mappingCount_; ++i) {
    if (mappings_[i].resourceId == 0 || mappings_[i].peerIndex >=
            SUPLAN_MAX_PERSISTENT_PEERS) {
      return false;
    }
    for (uint8_t j = 0; j < i; ++j) {
      if (mappings_[i].resourceId == mappings_[j].resourceId &&
          mappings_[i].peerIndex == mappings_[j].peerIndex) {
        return false;
      }
    }
  }
  return true;
#else
  return associations() != nullptr;
#endif
}

bool SupLan::localChannel(uint32_t id, uint8_t *number,
                           bool *eventOnly) const {
  if (associations() && associations()->identity()) {
    auto resolved = associations()->identity()->resolve(id);
    if (resolved.location != Supla::Device::ServerChannelLocation::kLocal) {
      return false;
    }
    *number = resolved.channelNumber;
    auto channel = Supla::Channel::GetByChannelNumber(*number);
    *eventOnly = channel && channel->getChannelType() ==
        SUPLA_CHANNELTYPE_ACTIONTRIGGER;
    return channel != nullptr;
  }
#ifdef SUPLAN_TESTING
  auto mapping = findResource(id);
  if (!mapping) return false;
  *number = mapping->channelNumber;
  *eventOnly = mapping->eventOnly;
  return true;
#else
  return false;
#endif  // SUPLAN_TESTING
}

void SupLan::putSuplaUint32(uint8_t output[4], uint32_t value) {
  output[0] = static_cast<uint8_t>(value);
  output[1] = static_cast<uint8_t>(value >> 8);
  output[2] = static_cast<uint8_t>(value >> 16);
  output[3] = static_cast<uint8_t>(value >> 24);
}

uint32_t SupLan::getSuplaUint32(const uint8_t input[4]) {
  return static_cast<uint32_t>(input[0]) |
      (static_cast<uint32_t>(input[1]) << 8) |
      (static_cast<uint32_t>(input[2]) << 16) |
      (static_cast<uint32_t>(input[3]) << 24);
}

uint8_t SupLan::channelOfflineState(const Supla::Channel *channel) {
  if (channel->isStateFirmwareUpdateOngoing()) {
    return SUPLA_CHANNEL_OFFLINE_FLAG_FIRMWARE_UPDATE_ONGOING;
  }
  if (channel->isStateOfflineRemoteWakeupNotSupported()) {
    return SUPLA_CHANNEL_OFFLINE_FLAG_OFFLINE_REMOTE_WAKEUP_NOT_SUPPORTED;
  }
  if (channel->isStateOnlineAndNotAvailable()) {
    return SUPLA_CHANNEL_OFFLINE_FLAG_ONLINE_BUT_NOT_AVAILABLE;
  }
  if (!channel->isStateOnline()) {
    return SUPLA_CHANNEL_OFFLINE_FLAG_OFFLINE;
  }
  return SUPLA_CHANNEL_OFFLINE_FLAG_ONLINE;
}

bool SupLan::readResource(const Supla::SupLan::ResourceId &resource,
                          bool *eventOnly, uint8_t *payload,
                          size_t capacity, size_t *payloadLength) {
  if (eventOnly == nullptr || payload == nullptr || payloadLength == nullptr ||
      resource.type != Supla::SupLan::kResourceTypeChannel) {
    return false;
  }
  uint8_t number = 0;
  bool onlyEvents = false;
  if (!localChannel(resource.id, &number, &onlyEvents)) return false;
  if (onlyEvents) {
    *eventOnly = true;
    *payloadLength = 0;
    return false;
  }
  Supla::Channel *channel =
      Supla::Channel::GetByChannelNumber(number);
  if (channel == nullptr || capacity < 14) {
    return false;
  }
  *eventOnly = false;
  if (fullChannelSnapshots()) {
    TDS_SuplaDeviceChannel_E snapshot;
    if (capacity < sizeof(snapshot) ||
        !buildD2dSnapshot(channel, &snapshot)) return false;
    memcpy(payload, &snapshot, sizeof(snapshot));
    *payloadLength = sizeof(snapshot);
    return true;
  }
#ifdef SUPLAN_TESTING
  payload[0] = Supla::SupLan::kChannelNumberUnresolved;
  TDS_SuplaDeviceChannel_E snapshot;
  if (!buildD2dSnapshot(channel, &snapshot)) return false;
  payload[1] = snapshot.Offline;
  putSuplaUint32(payload + 2, channel->getSupLanValidityTimeSec());
  channel->fillRawValue(payload + 6);
  *payloadLength = 14;
  return true;
#else
  return false;
#endif  // SUPLAN_TESTING
}

uint8_t SupLan::dispatchControlAction(
    const Supla::SupLan::ResourceId &resource, uint32_t actionId,
    const uint8_t *params, size_t length) {
  uint8_t number = 0;
  bool eventOnly = false;
  if (resource.type != Supla::SupLan::kResourceTypeChannel ||
      !localChannel(resource.id, &number, &eventOnly))
    return SUPLA_RESULTCODE_CHANNELNOTFOUND;
  auto element = Supla::Element::getElementByChannelNumber(number);
  if (element && !sourceControlAvailable(element))
    return SUPLA_RESULTCODE_ACTION_UNSUPPORTED;
  return element ? element->handleRemoteAction(actionId, params, length)
                 : SUPLA_RESULTCODE_CHANNELNOTFOUND;
}

uint8_t SupLan::dispatchControl(const Supla::SupLan::ResourceId &resource,
                                uint32_t messageType,
                                const uint8_t *payload,
                                size_t payloadLength) {
  if (resource.type != Supla::SupLan::kResourceTypeChannel ||
      messageType != Supla::SupLan::kSuplaCallChannelSetValue ||
      payload == nullptr || payloadLength != 17 ||
      payload[4] != Supla::SupLan::kChannelNumberUnresolved) {
    return SUPLA_RESULTCODE_UNSUPORTED;
  }
  uint8_t number = 0;
  bool eventOnly = false;
  if (!localChannel(resource.id, &number, &eventOnly)) {
    return SUPLA_RESULTCODE_CHANNELNOTFOUND;
  }
  Supla::Element *element =
      Supla::Element::getElementByChannelNumber(number);
  if (element == nullptr) {
    return SUPLA_RESULTCODE_CHANNELNOTFOUND;
  }
  TSD_SuplaChannelNewValue newValue = {};
  newValue.SenderID = static_cast<_supla_int_t>(getSuplaUint32(payload));
  newValue.ChannelNumber = number;
  newValue.DurationMS = getSuplaUint32(payload + 5);
  memcpy(newValue.value, payload + 9, SUPLA_CHANNELVALUE_SIZE);
  const int32_t result = element->handleRemoteNewValue(&newValue);
  if (result > 0) {
    return SUPLA_RESULTCODE_TRUE;
  }
  if (result == 0) {
    return SUPLA_RESULTCODE_FALSE;
  }
  return SUPLA_RESULTCODE_UNSUPORTED;
}

void SupLan::receiveState(uint8_t peerIndex,
                          const Supla::SupLan::ResourceId &resource,
                          uint32_t messageType, const uint8_t *payload,
                          size_t payloadLength) {
  if (resources_ && messageType == 6) {
    resources_->receive(peerIndex, resource, payload, payloadLength,
                         runtime_->nowMs());
  }
  if (eventHandler_ != nullptr) {
    eventHandler_(eventContext_, kSupLanRemoteState, peerIndex, resource,
                  messageType,
                  payload, payloadLength);
  }
}

void SupLan::receiveAction(uint8_t peerIndex,
                           const Supla::SupLan::ResourceId &resource,
                           uint32_t messageType, const uint8_t *payload,
                           size_t payloadLength) {
  if (messageType == Supla::SupLan::kSuplaCallActionTrigger &&
      payload && payloadLength == 15 &&
      resource.type == Supla::SupLan::kResourceTypeChannel)
    Supla::ChannelHandle::receiveAction(resource.id,
                                       getSuplaUint32(payload + 1));
  if (eventHandler_ != nullptr) {
    eventHandler_(eventContext_, kSupLanRemoteAction, peerIndex, resource,
                  messageType,
                  payload, payloadLength);
  }
}

uint8_t SupLan::receiveBindings(uint8_t peer, const uint8_t *body,
                                size_t length) {
  return resources_ && resources_->bindings
             ? resources_->bindings->accept(peer, body, length)
             : 24;
}
bool SupLan::bindingSessionNeeded(uint8_t peer) {
  return resources_ && resources_->bindings &&
      resources_->bindings->sessionNeeded(peer);
}
bool SupLan::share(const TDS_SuplaEnsureResourceShare &request) {
  return sdc && sdc->getSrpcLayer() &&
         sdc->getSrpcLayer()->ensureResourceShare(request);
}
void SupLan::cancelShare() {
  if (sdc && sdc->getSrpcLayer())
    sdc->getSrpcLayer()->cancelEnsureResourceShare();
}
void SupLan::ensureResourceShareResult(
    const TSD_SuplaEnsureResourceShareResult &result) {
  if (resources_ && resources_->bindings)
    resources_->bindings->ensureShareResult(result);
}

void SupLan::diagnostic(Supla::SupLan::DiagnosticEvent event, uint8_t peer,
                        const Supla::SupLan::ResourceId &resource,
                        uint32_t detail) {
  using E = Supla::SupLan::DiagnosticEvent;
  const bool warning = event == E::RecoveryFailed || event == E::ResourceLimit;
  const bool rejection = event >= E::RejectAuthority;
  // Disabled DEBUG rejects must not spend the WARNING rate-limit window.
  if (((rejection && !warning) || event == E::ResetSendFailed) &&
      !SUPLA_LOG_IS_ENABLED(LOG_DEBUG)) return;
  uint32_t count = 0;
  if (warning || rejection || event == E::ResetSendFailed) {
    const uint32_t now = runtime_ ? runtime_->nowMs() : millis();
    if (diagnosticSuppressed_ && now - diagnosticLastMs_ < 5000) {
      if (diagnosticSuppressed_ != UINT32_MAX) ++diagnosticSuppressed_;
      return;
    }
    diagnosticLastMs_ = now;
    count = diagnosticSuppressed_ ? diagnosticSuppressed_ - 1 : 0;
    diagnosticSuppressed_ = 1;
  }
  // Literal formats use the SDK's flash-string logging on Arduino.
  switch (event) {
    case E::RecoveryBegin:
      SUPLAN_DIAG_INFO("SupLAN SUPLAN_PEER peer=%u ChannelId=%" PRIu32
                     " recovery started"
                     " detail=%" PRIu32,
                     peer, resource.id, detail);
      break;
    case E::RecoveryFailed:
      SUPLAN_DIAG_WARNING(
          "SupLAN SUPLAN_PEER peer=%u ChannelId=%" PRIu32
          " recovery cycle failed (discovery/handshake/READ timeout)"
          " next_retry_ms=%" PRIu32 " suppressed=%" PRIu32,
          peer, resource.id, detail, count);
      break;
    case E::RecoveryComplete:
      SUPLAN_DIAG_INFO("SupLAN SUPLAN_PEER peer=%u ChannelId=%" PRIu32
                     " recovery completed"
                     " detail=%" PRIu32,
                     peer, resource.id, detail);
      break;
    case E::ReadAccepted:
      SUPLAN_DIAG_DEBUG(
          "SupLAN SUPLAN_PEER peer=%u ChannelId=%" PRIu32
          " READ logical demand accepted reason=%s awaiting queue/session",
          peer, resource.id,
                      detail == 2 ? "recovery"
                                  : detail ? "periodic" : "initial");
      break;
    case E::ReadQueued:
      SUPLAN_DIAG_DEBUG("SupLAN SUPLAN_PEER peer=%u ChannelId=%" PRIu32
                      " READ queue admission attempt"
                      " detail=%" PRIu32,
                      peer, resource.id, detail);
      break;
    case E::ReadTransmit:
      SUPLAN_DIAG_DEBUG("SupLAN SUPLAN_PEER peer=%u ChannelId=%" PRIu32
                      " READ transmit result=%s awaiting snapshot",
                      peer, resource.id, detail ? "SENT" : "TRANSPORT_FAILED");
      break;
    case E::ReadReceive:
      SUPLAN_DIAG_DEBUG("SupLAN SUPLAN_PEER peer=%u ChannelId=%" PRIu32
                      " Source READ received"
                      " detail=%" PRIu32,
                      peer, resource.id, detail);
      break;
    case E::StateTransmit:
      if (detail) {
        SUPLAN_DIAG_DEBUG("SupLAN SUPLAN_PEER peer=%u ChannelId=%" PRIu32
                         " Source state TX result=SENT", peer, resource.id);
      } else {
        SUPLAN_DIAG_DEBUG("SupLAN SUPLAN_PEER peer=%u ChannelId=%" PRIu32
                         " Source READ snapshot response enqueue attempt",
                         peer, resource.id);
      }
      break;
    case E::AckWithoutState:
      SUPLAN_DIAG_DEBUG("SupLAN SUPLAN_PEER peer=%u ChannelId=%" PRIu32
                      " ACK received awaiting snapshot (no lease renewal)"
                      " detail=%" PRIu32,
                      peer, resource.id, detail);
      break;
    case E::AckWithState:
      SUPLAN_DIAG_DEBUG("SupLAN SUPLAN_PEER peer=%u ChannelId=%" PRIu32
                      " ACK received (not a snapshot)"
                      " detail=%" PRIu32,
                      peer, resource.id, detail);
      break;
    case E::LocateTx:
      SUPLAN_DIAG_DEBUG("SupLAN SUPLAN_PEER peer=%u ChannelId=%" PRIu32
          " LOCATE TX result=%s", peer, resource.id,
          detail ? "SENT" : "TRANSPORT_FAILED");
      break;
    case E::LocateRx:
      SUPLAN_DIAG_DEBUG("SupLAN SUPLAN_PEER peer=%u ChannelId=%" PRIu32
                      " LOCATE RX"
                      " detail=%" PRIu32,
                      peer, resource.id, detail);
      break;
    case E::LocateReplyTx:
      SUPLAN_DIAG_DEBUG("SupLAN SUPLAN_PEER peer=%u ChannelId=%" PRIu32
                      " LOCATE_REPLY TX"
                      " detail=%" PRIu32,
                      peer, resource.id, detail);
      break;
    case E::LocateReplyRx:
      SUPLAN_DIAG_DEBUG("SupLAN SUPLAN_PEER peer=%u ChannelId=%" PRIu32
                      " LOCATE_REPLY RX"
                      " detail=%" PRIu32,
                      peer, resource.id, detail);
      break;
    case E::LocateTimeout:
      SUPLAN_DIAG_DEBUG("SupLAN SUPLAN_PEER peer=%u ChannelId=%" PRIu32
                      " LOCATE timeout"
                      " detail=%" PRIu32,
                      peer, resource.id, detail);
      break;
    case E::HandshakeTx:
      SUPLAN_DIAG_DEBUG("SupLAN SUPLAN_PEER peer=%u ChannelId=%" PRIu32
          " SESSION_INIT TX result=%s", peer, resource.id,
          detail ? "SENT" : "TRANSPORT_FAILED");
      break;
    case E::HandshakeTimeout:
      SUPLAN_DIAG_DEBUG("SupLAN SUPLAN_PEER peer=%u ChannelId=%" PRIu32
                      " handshake timeout"
                      " detail=%" PRIu32,
                      peer, resource.id, detail);
      break;
    case E::SessionEstablished:
      SUPLAN_DIAG_INFO("SupLAN SUPLAN_PEER peer=%u ChannelId=%" PRIu32
                     " session established/restored"
                     " detail=%" PRIu32,
                     peer, resource.id, detail);
      break;
    case E::SessionLost:
      SUPLAN_DIAG_INFO("SupLAN SUPLAN_PEER peer=%u ChannelId=%" PRIu32
                     " session lost"
                     " detail=%" PRIu32,
                     peer, resource.id, detail);
      break;
    case E::ResetTx:
      SUPLAN_DIAG_INFO("SupLAN SUPLAN_PEER peer=%u ChannelId=%" PRIu32
                     " SOURCE_INTEREST_RESET TX"
                     " detail=%" PRIu32,
                     peer, resource.id, detail);
      break;
    case E::ResetSkipped:
      SUPLAN_DIAG_DEBUG("SupLAN SUPLAN_PEER peer=%u ChannelId=%" PRIu32
                      " SOURCE_INTEREST_RESET skipped: not Source/no grants"
                      " detail=%" PRIu32,
                      peer, resource.id, detail);
      break;
    case E::ResetSendFailed:
      SUPLAN_DIAG_DEBUG("SupLAN SUPLAN_PEER peer=%u ChannelId=%" PRIu32
          " SOURCE_INTEREST_RESET not sent reason=%s", peer, resource.id,
          detail == 1 ? "PEER_MATERIAL" : detail == 2 ? "RANDOM" :
          detail == 3 ? "MAC" : "TRANSPORT");
      break;
    case E::ResetRx:
      SUPLAN_DIAG_INFO("SupLAN SUPLAN_PEER peer=%u ChannelId=%" PRIu32
                     " SOURCE_INTEREST_RESET accepted"
                     " detail=%" PRIu32,
                     peer, resource.id, detail);
      break;
    case E::ReadRefresh:
      SUPLAN_DIAG_DEBUG("SupLAN SUPLAN_PEER peer=%u ChannelId=%" PRIu32
          " READ refresh reason=%s", peer, resource.id,
          detail ? "interest_reset" : "recovery");
      break;
    case E::RejectAuthority:
      SUPLAN_DIAG_DEBUG("SupLAN SUPLAN_PEER peer=%u ChannelId=%" PRIu32
                      " packet/request rejected: live authority"
                      " detail=%" PRIu32,
                      peer, resource.id, detail);
      break;
    case E::RejectPacket:
      SUPLAN_DIAG_DEBUG("SupLAN SUPLAN_PEER peer=%u ChannelId=%" PRIu32
                      " packet rejected: invalid data/shape/authentication"
                      " detail=%" PRIu32,
                      peer, resource.id, detail);
      break;
    case E::RejectSession:
      SUPLAN_DIAG_DEBUG("SupLAN SUPLAN_PEER peer=%u ChannelId=%" PRIu32
                      " packet rejected: session authentication/shape"
                      " detail=%" PRIu32,
                      peer, resource.id, detail);
      break;
    case E::RejectLocate:
      SUPLAN_DIAG_DEBUG("SupLAN SUPLAN_PEER peer=%u ChannelId=%" PRIu32
                      " packet rejected: locate authentication/shape"
                      " detail=%" PRIu32,
                      peer, resource.id, detail);
      break;
    case E::RejectReplay:
      SUPLAN_DIAG_DEBUG("SupLAN SUPLAN_PEER peer=%u ChannelId=%" PRIu32
                      " packet rejected: replay"
                      " detail=%" PRIu32,
                      peer, resource.id, detail);
      break;
    case E::ResourceLimit:
      SUPLAN_DIAG_WARNING("SupLAN SUPLAN_PEER peer=%u ChannelId=%" PRIu32
                        " resource capacity exhausted"
                        " next_retry_ms=%" PRIu32 " suppressed=%" PRIu32,
                        peer, resource.id, detail, count);
      break;
  }
}

#undef SUPLAN_DIAG_DEBUG
#undef SUPLAN_DIAG_INFO
#undef SUPLAN_DIAG_WARNING

void SupLan::operationAcknowledged(uint8_t peerIndex,
                                   const Supla::SupLan::ResourceId &resource,
                                   uint32_t sequence, uint8_t result) {
  if (resources_ && resources_->bindings && !resource.id)
    resources_->bindings->acknowledged(peerIndex, result);
  uint8_t payload[5];
  putSuplaUint32(payload, sequence);
  payload[4] = result;
  if (eventHandler_ != nullptr) {
    eventHandler_(eventContext_, kSupLanOperationAck, peerIndex, resource, 1,
                  payload, sizeof(payload));
  }
}

void SupLan::readInterestRefreshed(
    uint8_t peerIndex, const Supla::SupLan::ResourceId &resource,
    bool eventOnly) {
  if (eventHandler_ != nullptr) {
    eventHandler_(eventContext_, kSupLanReadInterest, peerIndex, resource,
                  eventOnly ? 1 : 0, nullptr, 0);
  }
}

bool SupLan::hasSupLanActionPeer(uint8_t number) const {
  auto state = associations();
  uint32_t id = 0;
  if (!state || !state->identity() ||
      !state->identity()->reverse(number, &id)) return false;
  const Supla::SupLan::ResourceId resource = {
      Supla::SupLan::kResourceTypeChannel, id};
  for (uint16_t i = 0; i < SUPLAN_MAX_PERSISTENT_PEERS; ++i) {
    const auto peer = peers_->get(i);
    if (peer && !peer->destination && peers_->runtimeEligible(i) &&
        peers_->authorize(i, resource, Supla::SupLan::kPermissionActionTrigger))
      return true;
  }
  return false;
}

void SupLan::sendActionTrigger(uint8_t channelNumber, uint32_t actionId) {
  if (!isEnabled() || !mapIsValid()) {
    return;
  }
  uint8_t payload[15] = {};
  payload[0] = Supla::SupLan::kChannelNumberUnresolved;
  putSuplaUint32(payload + 1, actionId);
  if (associations() && associations()->identity()) {
    uint32_t id = 0;
    if (!associations()->identity()->reverse(channelNumber, &id)) return;
    const Supla::SupLan::ResourceId resource = {
        Supla::SupLan::kResourceTypeChannel, id};
    for (uint16_t i = 0; i < SUPLAN_MAX_PERSISTENT_PEERS; ++i) {
      (void)runtime_->publishAction(i, resource, payload, sizeof(payload));
    }
    return;
  }
#ifdef SUPLAN_TESTING
  for (uint8_t i = 0; i < mappingCount_; ++i) {
    const SupLanResourceMapping *mapping = &mappings_[i];
    if (mapping->channelNumber != channelNumber) {
      continue;
    }
    const Supla::SupLan::ResourceId resource = {
        Supla::SupLan::kResourceTypeChannel, mapping->resourceId};
    (void)runtime_->publishAction(mapping->peerIndex, resource, payload,
                                  sizeof(payload));
  }
#endif  // SUPLAN_TESTING
}

void SupLan::sendChannelValueChanged(uint8_t channelNumber, int8_t *value,
                                     uint8_t offline,
                                     uint32_t validityTimeSec) {
  // D2S fan-out argument; D2D owns an independent lease.
  (void)validityTimeSec;
  (void)offline;
  if (!isRegisteredAndReady() || value == nullptr) {
    return;
  }
  if (fullChannelSnapshots()) {
    uint32_t id = 0;
    TDS_SuplaDeviceChannel_E snapshot;
    auto *channel = Supla::Channel::GetByChannelNumber(channelNumber);
    if (!associations()->identity() ||
        !associations()->identity()->reverse(channelNumber, &id) ||
        !buildD2dSnapshot(channel, &snapshot)) return;
    const Supla::SupLan::ResourceId resource = {
        Supla::SupLan::kResourceTypeChannel, id};
    for (int i = 0; i < SUPLAN_MAX_PERSISTENT_PEERS; ++i) {
      runtime_->publishStateParts(i, resource, 6, nullptr, 0,
          reinterpret_cast<const uint8_t *>(&snapshot), sizeof(snapshot), true);
    }
    return;
  }
#ifdef SUPLAN_TESTING
  uint8_t payload[14] = {};
  payload[0] = Supla::SupLan::kChannelNumberUnresolved;
  payload[1] = offline;
  auto *channel = Supla::Channel::GetByChannelNumber(channelNumber);
  if (!channel) return;
  TDS_SuplaDeviceChannel_E snapshot;
  if (!buildD2dSnapshot(channel, &snapshot)) return;
  // Preserve the legacy callback's explicit Offline argument. Acquisition
  // failure can additionally withdraw availability, never restore it.
  if (offline == SUPLA_CHANNEL_OFFLINE_FLAG_ONLINE &&
      snapshot.Offline == SUPLA_CHANNEL_OFFLINE_FLAG_ONLINE_BUT_NOT_AVAILABLE)
    payload[1] = snapshot.Offline;
  putSuplaUint32(payload + 2, channel->getSupLanValidityTimeSec());
  memcpy(payload + 6, value, SUPLA_CHANNELVALUE_SIZE);
  if (associations() && associations()->identity()) {
    uint32_t id = 0;
    if (!associations()->identity()->reverse(channelNumber, &id)) return;
    const Supla::SupLan::ResourceId resource = {
        Supla::SupLan::kResourceTypeChannel, id};
    for (uint16_t i = 0; i < SUPLAN_MAX_PERSISTENT_PEERS; ++i) {
      (void)runtime_->publishState(
          i, resource, Supla::SupLan::kSuplaCallDeviceChannelValueChangedC,
          payload, sizeof(payload));
    }
    return;
  }
  for (uint8_t i = 0; i < mappingCount_; ++i) {
    const SupLanResourceMapping *mapping = &mappings_[i];
    if (mapping->channelNumber != channelNumber || mapping->eventOnly) {
      continue;
    }
    const Supla::SupLan::ResourceId resource = {
        Supla::SupLan::kResourceTypeChannel, mapping->resourceId};
    (void)runtime_->publishState(
        mapping->peerIndex, resource,
        Supla::SupLan::kSuplaCallDeviceChannelValueChangedC,
        payload, sizeof(payload));
  }
#endif  // SUPLAN_TESTING
}

void SupLan::sendExtendedChannelValueChanged(
    uint8_t channelNumber, TSuplaChannelExtendedValue *value) {
  if (!isRegisteredAndReady() || value == nullptr) {
    return;
  }
  if (value->size > 750 || value->size > sizeof(value->value)) {
    return;
  }
  uint8_t prefix[6] = {};
  prefix[0] = Supla::SupLan::kChannelNumberUnresolved;
  prefix[1] = static_cast<uint8_t>(value->type);
  putSuplaUint32(prefix + 2, value->size);
  if (associations() && associations()->identity()) {
    uint32_t id = 0;
    if (!associations()->identity()->reverse(channelNumber, &id)) return;
    const Supla::SupLan::ResourceId resource = {
        Supla::SupLan::kResourceTypeChannel, id};
    for (uint16_t i = 0; i < SUPLAN_MAX_PERSISTENT_PEERS; ++i) {
      (void)runtime_->publishStateParts(
          i, resource,
          Supla::SupLan::kSuplaCallDeviceChannelExtendedValueChanged, prefix,
          sizeof(prefix), reinterpret_cast<const uint8_t *>(value->value),
          value->size);
    }
    return;
  }
#ifdef SUPLAN_TESTING
  for (uint8_t i = 0; i < mappingCount_; ++i) {
    const SupLanResourceMapping *mapping = &mappings_[i];
    if (mapping->channelNumber != channelNumber || mapping->eventOnly) {
      continue;
    }
    const Supla::SupLan::ResourceId resource = {
        Supla::SupLan::kResourceTypeChannel, mapping->resourceId};
    (void)runtime_->publishStateParts(
        mapping->peerIndex, resource,
        Supla::SupLan::kSuplaCallDeviceChannelExtendedValueChanged,
        prefix, sizeof(prefix),
        reinterpret_cast<const uint8_t *>(value->value), value->size);
  }
#endif  // SUPLAN_TESTING
}

void SupLan::sendChannelMetadataChanged(uint8_t number) {
  auto *channel = Supla::Channel::GetByChannelNumber(number);
  if (!channel) return;
  int8_t value[8];
  channel->fillRawValue(value);
  sendChannelValueChanged(number, value, channelOfflineState(channel), 0);
}

bool SupLan::prepareBackend(uint8_t delivery,
    Device::RemoteChannelManager *manager) {
  return delivery == SUPLA_RESOURCE_DELIVERY_SERVER_STREAM && sdc &&
      sdc->getSrpcLayer() && sdc->getSrpcLayer()->prepareServerStream(manager);
}
bool SupLan::ensure(const TDS_SuplaEnsureResourceAccess &request) {
  return sdc && sdc->getSrpcLayer() &&
         sdc->getSrpcLayer()->ensureResourceAccess(request);
}
void SupLan::cancelEnsure() {
  if (sdc && sdc->getSrpcLayer()) {
    sdc->getSrpcLayer()->cancelEnsureResourceAccess();
  }
}
void SupLan::ensureResourceAccessResult(
    const TSD_SuplaEnsureResourceAccessResult &result) {
  if (resources_) resources_->ensureResult(result);
}

bool SupLan::allowPresence(uint8_t index, uint32_t nowMs) {
  return resources_ && resources_->allowPresence(index, nowMs);
}
void SupLan::peerAwake(uint8_t index, uint16_t window, bool reset,
                       const uint8_t *nonce) {
  if (resources_) {
    resources_->peerAwake(index, window, reset, nonce, runtime_->nowMs());
  }
}
void SupLan::beginWake(uint16_t window, bool retainedInterests) {
  if (!runtime_ || !resources_ || !window) return;
  resources_->presence().awakeWindowMs_ = window;
  if (sdc) {
    // Fixed fan-out plus the advertised window; peer traffic cannot extend it.
    sdc->deferSleep(window + SUPLAN_MAX_PERSISTENT_PEERS * 10);
  }
  resources_->presence().awakeUntilMs_ = runtime_->nowMs() + window;
  resources_->presence().presenceCursor_ = 0;
  resources_->presence().nextPresenceMs_ = runtime_->nowMs();
  resources_->presence().awakePending_ = true;
  if (!retainedInterests) {
    runtime_->clearInterests();
    resources_->presence().resetPending_ = true;
  } else {
    runtime_->resumeRetainedInterests();
  }
}
bool SupLan::wakeWindowComplete() const {
  return !runtime_ || !resources_ ||
         !resources_->presence().awakeWindowMs_ ||
         (!resources_->presence().awakePending_ &&
          static_cast<int32_t>(runtime_->nowMs() -
                               resources_->presence().awakeUntilMs_) >= 0);
}
void SupLan::announceSleep(uint32_t duration) {
  if (!runtime_) return;
  for (int i = 0; i < SUPLAN_MAX_PERSISTENT_PEERS; ++i) {
    runtime_->announceSleep(i, duration);
  }
}

}  // namespace Protocol
}  // namespace Supla

#endif  // !ARDUINO_ARCH_AVR

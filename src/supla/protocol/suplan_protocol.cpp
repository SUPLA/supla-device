// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright 2026 AC SOFTWARE SP. Z O.O.

#ifndef ARDUINO_ARCH_AVR

#include <supla/protocol/suplan_protocol.h>

#include <string.h>

#include <SuplaDevice.h>
#include <supla/at_channel.h>
#include <supla/channels/channel.h>
#include <supla/element.h>
#include <supla/protocol/supla_srpc.h>
#include <supla/storage/storage.h>
#include <supla/suplan/suplan_server_associations.h>
#include <supla/suplan/suplan_server_identity.h>

namespace Supla {
namespace Protocol {

SupLan::SupLan(SuplaDeviceClass *sdc, Supla::SupLan::PeerTable *peers,
               const SupLanResourceMapping *mappings, uint8_t mappingCount,
               SupLanApplicationEventHandler eventHandler,
               void *eventContext)
    : ProtocolLayer(sdc), peers_(peers), runtime_(nullptr), transport_(nullptr),
      mappings_(mappings), eventHandler_(eventHandler),
      eventContext_(eventContext), mappingCount_(mappingCount), enabled_(true),
      transportWasOpen_(false) {
  configEmpty = false;
}

SupLan::~SupLan() {}

void SupLan::attachRuntime(Supla::SupLan::Runtime *runtime) {
  runtime_ = runtime;
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
  if (associations_ && sdc && sdc->getSrpcLayer()) {
    associations_->load(Supla::Storage::ConfigInstance(),
                         &sdc->getSrpcLayer()->serverIdentity());
  }
}

void SupLan::attachServerAssociations(
    Supla::Device::ServerAssociations *state) {
  associations_ = state;
}

bool SupLan::setSuplanSourceAssociation(
    const TSDS_SuplaSetSuplanSourceAssociation &request,
    TDS_SuplaSetSuplanSourceAssociationResult *result) {
  if (!associations_ || !enabled_) return false;
  *result = associations_->accept(request);
  return true;
}

bool SupLan::setSuplanDestinationAssociation(
    const TSDS_SuplaSetSuplanDestinationAssociation &request,
    TDS_SuplaSetSuplanDestinationAssociationResult *result) {
  if (!associations_ || !enabled_) return false;
  *result = associations_->accept(request);
  return true;
}

void SupLan::suplanIdentityChanged() {
  if (associations_) associations_->identityChanged();
}

bool SupLan::onLoadConfig() {
  return true;
}

bool SupLan::verifyConfig() {
  return mapIsValid() && peers_ != nullptr && runtime_ != nullptr;
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

  if (transport_ != nullptr) {
    if (!transport_->networkReady(nowMs)) {
      if (transport_->isOpen() || transportWasOpen_) {
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

  suplanIdentityChanged();
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

const SupLanResourceMapping *SupLan::findResource(uint32_t resourceId) const {
  for (uint8_t i = 0; i < mappingCount_; ++i) {
    if (mappings_[i].resourceId == resourceId) {
      return &mappings_[i];
    }
  }
  return nullptr;
}

bool SupLan::mapIsValid() const {
  if (associations_ && !mappings_ && !mappingCount_) return true;
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
}

bool SupLan::localChannel(uint32_t id, uint8_t *number,
                           bool *eventOnly) const {
  if (associations_ && associations_->identity()) {
    auto resolved = associations_->identity()->resolve(id);
    if (resolved.location != Supla::Device::ServerChannelLocation::kLocal) {
      return false;
    }
    *number = resolved.channelNumber;
    auto channel = Supla::Channel::GetByChannelNumber(*number);
    *eventOnly = channel && channel->getChannelType() ==
        SUPLA_CHANNELTYPE_ACTIONTRIGGER;
    return channel != nullptr;
  }
  auto mapping = findResource(id);
  if (!mapping) return false;
  *number = mapping->channelNumber;
  *eventOnly = mapping->eventOnly;
  return true;
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
  payload[0] = Supla::SupLan::kChannelNumberUnresolved;
  payload[1] = channelOfflineState(channel);
  putSuplaUint32(payload + 2, channel->getValidityTimeSec());
  channel->fillRawValue(payload + 6);
  *payloadLength = 14;
  return true;
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
  const int32_t result = element->handleNewValueFromServer(&newValue);
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
  if (eventHandler_ != nullptr) {
    eventHandler_(eventContext_, kSupLanRemoteAction, peerIndex, resource,
                  messageType,
                  payload, payloadLength);
  }
}

void SupLan::operationAcknowledged(
    uint8_t peerIndex, const Supla::SupLan::ResourceId &resource,
    uint32_t sequence, uint8_t result) {
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

void SupLan::sendActionTrigger(uint8_t channelNumber, uint32_t actionId) {
  if (!isRegisteredAndReady()) {
    return;
  }
  uint8_t payload[15] = {};
  payload[0] = Supla::SupLan::kChannelNumberUnresolved;
  putSuplaUint32(payload + 1, actionId);
  if (associations_ && associations_->identity()) {
    uint32_t id = 0;
    if (!associations_->identity()->reverse(channelNumber, &id)) return;
    const Supla::SupLan::ResourceId resource = {
        Supla::SupLan::kResourceTypeChannel, id};
    for (uint16_t i = 0; i < SUPLAN_MAX_PERSISTENT_PEERS; ++i) {
      (void)runtime_->publishAction(i, resource, payload, sizeof(payload));
    }
    return;
  }
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
}

void SupLan::sendChannelValueChanged(uint8_t channelNumber, int8_t *value,
                                     uint8_t offline,
                                     uint32_t validityTimeSec) {
  if (!isRegisteredAndReady() || value == nullptr) {
    return;
  }
  uint8_t payload[14] = {};
  payload[0] = Supla::SupLan::kChannelNumberUnresolved;
  payload[1] = offline;
  putSuplaUint32(payload + 2, validityTimeSec);
  memcpy(payload + 6, value, SUPLA_CHANNELVALUE_SIZE);
  if (associations_ && associations_->identity()) {
    uint32_t id = 0;
    if (!associations_->identity()->reverse(channelNumber, &id)) return;
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
  if (associations_ && associations_->identity()) {
    uint32_t id = 0;
    if (!associations_->identity()->reverse(channelNumber, &id)) return;
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
}

}  // namespace Protocol
}  // namespace Supla

#endif  // !ARDUINO_ARCH_AVR

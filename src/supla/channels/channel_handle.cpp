// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later
#include "channel_handle.h"
#ifndef ARDUINO_ARCH_AVR
#include <string.h>
#include <supla/channels/channel.h>
#include <supla/condition.h>
#include <supla/control/action_trigger.h>
#include <supla/element.h>
#include <supla/events.h>
#include <supla/protocol/protocol_layer.h>
#include <supla/suplan/remote_resource_manager.h>
#include <supla/time.h>

namespace Supla {
ChannelHandle *ChannelHandle::first_ = nullptr;
uint32_t ChannelHandle::nextToken_ = 0x80000000U;
ChannelHandle::ChannelHandle(uint32_t channelId, uint8_t permissions)
    : location_(ChannelResolutionKind::kUnresolved), active_(true),
      admitted_(false), baselineValid_(false), baselineTimed_(false) {
  // Never recycle a token, including after storage/address reuse.
  if (nextToken_ != UINT32_MAX) token_ = ++nextToken_;
  configure(channelId, permissions);
  next_ = first_;
  first_ = this;
}
ChannelHandle::~ChannelHandle() {
  deactivate();
  auto link = &first_;
  while (*link && *link != this) link = &(*link)->next_;
  if (*link) *link = next_;
}
bool ChannelHandle::configure(uint32_t id, uint8_t permissions) {
  if (!token_ || !id || id > INT32_MAX || !permissions || (permissions & ~7))
    return false;
  if (id_ == id && permissions_ == permissions && active_) return true;
  if (id_ != id) {
    CancelDispatch(this);
    releaseChannelConsumer(token_);
    DeleteActionsHandledBy(this);
    admitted_ = false;
    baselineValid_ = false;
    continuity_ = 0;
    location_ = ChannelResolutionKind::kUnresolved;
  }
  id_ = id;
  permissions_ = permissions;
  active_ = true;
  // Reconciliation happens at the loop boundary, never in a constructor.
  return true;
}
void ChannelHandle::deactivate() {
  CancelDispatch(this);
  active_ = false;
  admitted_ = false;
  baselineValid_ = false;
  releaseChannelConsumer(token_);
  DeleteActionsHandledBy(this);
}
Device::RemoteResourceManager *ChannelHandle::manager() const {
  for (auto layer = Protocol::ProtocolLayer::first(); layer;
       layer = layer->next()) {
    if (auto resources = layer->remoteResources()) return resources;
  }
  return nullptr;
}
Channel *ChannelHandle::localChannel() {
  if (!active_) return nullptr;
  const auto resolved = resolveChannelReference(
      ChannelReference::server(id_), acceptedReferenceIdentity());
  return !remoteOnly_ && resolved.kind == ChannelResolutionKind::kLocal
             ? resolved.channel : nullptr;
}
void ChannelHandle::reconcile() {
  if (!active_) return;
  const auto resolved = resolveChannelReference(
      ChannelReference::server(id_), acceptedReferenceIdentity());
  const auto location = remoteOnly_ &&
      resolved.kind == ChannelResolutionKind::kLocal
          ? ChannelResolutionKind::kUnresolved : resolved.kind;
  const uint8_t localNumber = location == ChannelResolutionKind::kLocal
      ? resolved.channel->getChannelNumber() : 0xff;
  if (location != location_ || localNumber_ != localNumber) {
    releaseChannelConsumer(token_);
    DeleteActionsHandledBy(this);
    baselineValid_ = false;
    admitted_ = false;
    continuity_ = 0;
    location_ = location;
    localNumber_ = localNumber;
  }
  if (location == ChannelResolutionKind::kLocal) {
    admitted_ = true;
    for (auto entry = ActionHandlerClient::begin; entry; entry = entry->next) {
      if (entry->trigger == this && !entry->deletePending &&
          !resolved.channel->getHandlerForClient(this, entry->onEvent))
        resolved.channel->addAction(localNumber_, this, entry->onEvent);
    }
  } else if (location == ChannelResolutionKind::kRemote) {
    auto resources = manager();
    // Deferred demand stays on the intrusive live-handle list. Retry only
    // after a pool/authorization change, rather than polling a full pool.
    if (!admitted_ && resources &&
        continuity_ == resources->admissionGeneration()) return;
    admitted_ = resources && resources->consume(
        {token_, {SupLan::kResourceTypeChannel, id_}, permissions_, true});
    if (!admitted_ && resources)
      continuity_ = resources->admissionGeneration();
  }
}
bool ChannelHandle::canRead() {
  reconcile();
  if (!active_ || !(permissions_ & (SupLan::kPermissionRead |
                                   SupLan::kPermissionControl))) return false;
  if (localChannel()) return true;
  auto resources = manager();
  return admitted_ && resources && (resources->effectivePermissions(
      {SupLan::kResourceTypeChannel, id_}) & SupLan::kPermissionRead);
}
bool ChannelHandle::canControl() {
  reconcile();
  if (!active_ || !(permissions_ & SupLan::kPermissionControl)) return false;
  if (localChannel()) return true;
  auto resources = manager();
  return admitted_ && resources && (resources->effectivePermissions(
      {SupLan::kResourceTypeChannel, id_}) & SupLan::kPermissionControl);
}
ChannelState ChannelHandle::state() {
  if (!canRead()) return ChannelState();
  if (auto channel = localChannel()) return ChannelState(channel);
  auto resources = manager();
  return resources ? resources->state({SupLan::kResourceTypeChannel, id_},
                                      millis()) : ChannelState();
}
ChannelAccessStatus ChannelHandle::accessStatus() {
  reconcile();
  if (!active_ || !id_) return ChannelAccessStatus::INVALID;
  if (localChannel()) return ChannelAccessStatus::LOCAL_CHANNEL;
  if (location_ == ChannelResolutionKind::kUnresolved) {
    const auto resolved = resolveChannelReference(
        ChannelReference::server(id_), acceptedReferenceIdentity());
    return remoteOnly_ && resolved.kind == ChannelResolutionKind::kLocal
        ? ChannelAccessStatus::INVALID
        : ChannelAccessStatus::IDENTITY_UNRESOLVED;
  }
  if (!admitted_) return ChannelAccessStatus::RESOURCE_LIMIT;
  const SupLan::ResourceId resource = {SupLan::kResourceTypeChannel, id_};
  auto resources = manager();
  uint8_t desired = permissions_;
  if (desired & SupLan::kPermissionControl) desired |= SupLan::kPermissionRead;
  if ((resources->effectivePermissions(resource) & desired) == desired)
    return resources->peerAvailable(resource) ? ChannelAccessStatus::READY
        : ChannelAccessStatus::PEER_UNAVAILABLE;
  switch (resources->approvalStatus(resource, desired)) {
    case SUPLA_SUPLAN_ACCESS_STATUS_PENDING_APPROVAL:
      return ChannelAccessStatus::PENDING_APPROVAL;
    case SUPLA_SUPLAN_ACCESS_STATUS_REJECTED:
      return ChannelAccessStatus::REJECTED;
    default: return ChannelAccessStatus::PROVISIONING;
  }
}
bool ChannelHandle::setValue(const uint8_t value[8], uint32_t durationMs) {
  if (!value || !canControl()) return false;
  TSD_SuplaChannelNewValue command = {};
  command.DurationMS = durationMs;
  memcpy(command.value, value, 8);
  if (auto channel = localChannel()) {
    command.ChannelNumber = channel->getChannelNumber();
    auto element = Element::getElementByChannelNumber(command.ChannelNumber);
    return element && element->handleRemoteNewValue(&command) > 0;
  }
  uint8_t payload[17] = {};
  payload[4] = SupLan::kChannelNumberUnresolved;
  for (uint8_t i = 0; i < 4; ++i)
    payload[5 + i] = static_cast<uint8_t>(durationMs >> (i * 8));
  memcpy(payload + 9, value, 8);
  return manager()->control({SupLan::kResourceTypeChannel, id_}, payload,
                             sizeof(payload), token_);
}
bool ChannelHandle::executeAction(uint32_t actionId, const uint8_t *params,
                                   size_t length) {
  if (length > SupLan::kControlActionMaxParams || (length && !params) ||
      !canControl()) return false;
  if (auto channel = localChannel()) {
    auto element = Element::getElementByChannelNumber(
        channel->getChannelNumber());
    return element && element->handleRemoteAction(actionId, params, length) ==
                          SUPLA_RESULTCODE_TRUE;
  }
  return manager()->action({SupLan::kResourceTypeChannel, id_}, actionId,
                            params, length, token_);
}
void ChannelHandle::addAction(uint16_t action, ActionHandler &client,
                              uint16_t event, Condition *condition,
                              bool alwaysEnabled) {
  if (!condition) return;
  condition->setClient(client);
  condition->setStateSource(this);
  LocalAction::addAction(action, condition, event, alwaysEnabled);
  if (baselineValid_) condition->baseline();
}
void ChannelHandle::addAction(uint16_t action, ActionHandler *client,
                              uint16_t event, Condition *condition,
                              bool alwaysEnabled) {
  if (client) addAction(action, *client, event, condition, alwaysEnabled);
}
void ChannelHandle::baselineConditions() {
  baselineActions();
}
void ChannelHandle::handleAction(int event, int number) {
  auto channel = localChannel();
  if (!channel || number != channel->getChannelNumber() || !active_) return;
  if (channel->getChannelType() == SUPLA_CHANNELTYPE_ACTIONTRIGGER) {
    if (permissions_ & SupLan::kPermissionAction) runAction(event);
  } else if (baselineValid_ && canRead()) {
    if (state().available()) runAction(event);
    else baselineValid_ = false;
  }
}
ChannelHandle *ChannelHandle::after(uint32_t token, uint32_t limit) {
  ChannelHandle *result = nullptr;
  for (auto handle = first_; handle; handle = handle->next_) {
    if (handle->token_ > token && handle->token_ <= limit &&
        (!result || handle->token_ < result->token_)) result = handle;
  }
  return result;
}
void ChannelHandle::observe() {
  const auto view = state();
  uint8_t bytes[8];
  if (!view.value(bytes)) {
    baselineValid_ = false;
    return;
  }
  const uint32_t now = millis();
  const uint32_t continuity = location_ == ChannelResolutionKind::kRemote
      ? manager()->continuity({SupLan::kResourceTypeChannel, id_})
      : Channel::identityGeneration();
  if (!continuity && location_ == ChannelResolutionKind::kRemote) {
    baselineValid_ = false;
    return;
  }
  const bool continuous = continuity_ == continuity && baselineValid_ &&
      type_ == view.type() &&
      function_ == view.function() && (!baselineTimed_ ||
          static_cast<int32_t>(now - baselineExpires_) < 0);
  uint16_t events[24];
  uint8_t count = 0;
  if (continuous && location_ == ChannelResolutionKind::kRemote &&
      memcmp(baseline_, bytes, 8)) {
    events[count++] = ON_CHANGE;
    if ((view.type() == SUPLA_CHANNELTYPE_BINARYSENSOR ||
         view.type() == SUPLA_CHANNELTYPE_RELAY) &&
        (baseline_[0] != 0) != (bytes[0] != 0))
      events[count++] = bytes[0] ? ON_TURN_ON : ON_TURN_OFF;
    if (view.type() == SUPLA_CHANNELTYPE_HUMIDITYANDTEMPSENSOR &&
        memcmp(baseline_ + 4, bytes + 4, 4))
      events[count++] = ON_SECONDARY_CHANNEL_CHANGE;
    const auto type = view.type();
    if (type == SUPLA_CHANNELTYPE_DIMMER ||
        type == SUPLA_CHANNELTYPE_RGBLEDCONTROLLER ||
        type == SUPLA_CHANNELTYPE_DIMMERANDRGBLED) {
      events[count++] = ON_SECONDARY_CHANNEL_CHANGE;
      bool turnedOn = false;
      bool turnedOff = false;
      if (baseline_[0] != bytes[0]) {
        events[count++] = ON_DIMMER_BRIGHTNESS_CHANGE;
        if (!baseline_[0] || !bytes[0]) {
          events[count++] = bytes[0] ? ON_DIMMER_TURN_ON : ON_DIMMER_TURN_OFF;
          turnedOn |= bytes[0] != 0;
          turnedOff |= bytes[0] == 0;
        }
      }
      if (baseline_[7] != bytes[7])
        events[count++] = ON_WHITE_TEMPERATURE_CHANGE;
      if (baseline_[1] != bytes[1]) {
        events[count++] = ON_COLOR_BRIGHTNESS_CHANGE;
        if (!baseline_[1] || !bytes[1]) {
          events[count++] = bytes[1] ? ON_COLOR_TURN_ON : ON_COLOR_TURN_OFF;
          turnedOn |= bytes[1] != 0;
          turnedOff |= bytes[1] == 0;
        }
      }
      const uint16_t on[3] = {ON_BLUE_TURN_ON, ON_GREEN_TURN_ON,
          ON_RED_TURN_ON};
      const uint16_t off[3] = {ON_BLUE_TURN_OFF, ON_GREEN_TURN_OFF,
                               ON_RED_TURN_OFF};
      const uint16_t change[3] = {ON_BLUE_CHANGE, ON_GREEN_CHANGE,
          ON_RED_CHANGE};
      for (uint8_t color = 0; color < 3; ++color) {
        const bool wasOn = baseline_[1] && baseline_[color + 2];
        const bool isOn = bytes[1] && bytes[color + 2];
        if (wasOn != isOn) events[count++] = isOn ? on[color] : off[color];
        if (baseline_[color + 2] != bytes[color + 2])
          events[count++] = change[color];
      }
      if (turnedOn) events[count++] = ON_TURN_ON;
      if (turnedOff) events[count++] = ON_TURN_OFF;
    } else if (type == SUPLA_CHANNELTYPE_HVAC) {
      THVACValue previous, current;
      memcpy(&previous, baseline_, sizeof(previous));
      memcpy(&current, bytes, sizeof(current));
      if (previous.IsOn != current.IsOn &&
          ((!previous.IsOn || previous.IsOn == 2) !=
           (!current.IsOn || current.IsOn == 2)))
        events[count++] = (!current.IsOn || current.IsOn == 2)
                             ? ON_TURN_OFF : ON_TURN_ON;
      if (previous.Mode != current.Mode) {
        switch (current.Mode) {
          case SUPLA_HVAC_MODE_OFF: events[count++] = ON_HVAC_MODE_OFF; break;
          case SUPLA_HVAC_MODE_HEAT: events[count++] = ON_HVAC_MODE_HEAT; break;
          case SUPLA_HVAC_MODE_COOL: events[count++] = ON_HVAC_MODE_COOL; break;
          case SUPLA_HVAC_MODE_HEAT_COOL:
            events[count++] = ON_HVAC_MODE_HEAT_COOL; break;
        }
      }
      const auto changed = previous.Flags ^ current.Flags;
      if (changed & SUPLA_HVAC_VALUE_FLAG_WEEKLY_SCHEDULE)
        events[count++] = current.Flags & SUPLA_HVAC_VALUE_FLAG_WEEKLY_SCHEDULE
            ? ON_HVAC_WEEKLY_SCHEDULE_ENABLED
            : ON_HVAC_WEEKLY_SCHEDULE_DISABLED;
      if ((changed & SUPLA_HVAC_VALUE_FLAG_HEATING) &&
          (current.Flags & SUPLA_HVAC_VALUE_FLAG_HEATING))
        events[count++] = ON_HVAC_HEATING;
      if ((changed & SUPLA_HVAC_VALUE_FLAG_COOLING) &&
          (current.Flags & SUPLA_HVAC_VALUE_FLAG_COOLING))
        events[count++] = ON_HVAC_COOLING;
      if ((changed & (SUPLA_HVAC_VALUE_FLAG_HEATING |
                      SUPLA_HVAC_VALUE_FLAG_COOLING)) &&
          !(current.Flags & (SUPLA_HVAC_VALUE_FLAG_HEATING |
                             SUPLA_HVAC_VALUE_FLAG_COOLING)))
        events[count++] = ON_HVAC_STANDBY;
    }
  }
  continuity_ = continuity;
  memcpy(baseline_, bytes, 8);
  type_ = view.type();
  function_ = view.function();
  baselineValid_ = true;
  const auto ttl = view.validityTimeSec();
  baselineTimed_ = ttl && ttl < INT32_MAX / 1000U;
  baselineExpires_ = baselineTimed_ ? view.receivedMs() + ttl * 1000U : 0;
  const uint32_t token = token_;
  const uint32_t sourceId = id_;
  if (!continuous) baselineConditions();
  for (uint8_t i = 0; i < count; ++i) {
    auto handle = after(token - 1, token);
    if (!handle || !handle->active_ || handle->id_ != sourceId ||
        handle->continuity_ != continuity) break;
    handle->runAction(events[i]);
  }
}
void ChannelHandle::iterateAll() {
  const uint32_t limit = nextToken_;
  uint32_t token = 0;
  while (auto handle = after(token, limit)) {
    token = handle->token_;
    handle->reconcile();
    if (handle->active_) handle->observe();
  }
}
void ChannelHandle::receiveAction(uint32_t channelId, uint32_t capability) {
  const int event = Control::ActionTrigger::actionTriggerCapToButtonEvent(
      capability);
  if (event < 0) return;
  const uint32_t limit = nextToken_;
  uint32_t token = 0;
  while (auto handle = after(token, limit)) {
    token = handle->token_;
    handle->reconcile();
    if (handle->id_ != channelId || !handle->active_ || !handle->admitted_ ||
        handle->location_ != ChannelResolutionKind::kRemote ||
        !(handle->permissions_ & SupLan::kPermissionAction)) continue;
    auto resources = handle->manager();
    if (resources && (resources->effectivePermissions(
        {SupLan::kResourceTypeChannel, channelId}) & SupLan::kPermissionAction))
      handle->runAction(event);
  }
}
}  // namespace Supla
#endif  // !ARDUINO_ARCH_AVR

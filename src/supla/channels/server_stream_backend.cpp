// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later
#include "server_stream_backend.h"
#ifndef ARDUINO_ARCH_AVR
#include <supla/protocol/supla_srpc.h>
#include <new>
namespace Supla {
namespace Device {
ServerStreamBackend::ServerStreamBackend(RemoteChannelManager *manager,
    Protocol::SuplaSrpc *srpc) : manager_(manager), srpc_(srpc) {}
ServerStreamBackend::~ServerStreamBackend() {
  if (manager_) manager_->unregisterBackend(RemoteDelivery::ServerStream);
  if (slots_) {
    for (auto resource : slots_->resources) delete resource;
    for (auto intent : slots_->intents) delete intent;
    delete slots_;
  }
}
bool ServerStreamBackend::allocateSlots() {
  if (!slots_) slots_ = new (std::nothrow) Slots;
  return slots_ != nullptr;
}
void ServerStreamBackend::trimSlots() {
  if (!slots_) return;
  for (auto resource : slots_->resources) if (resource) return;
  for (auto intent : slots_->intents) if (intent) return;
  delete slots_;
  slots_ = nullptr;
}
ConsumeIntent ServerStreamBackend::intentAt(int index) {
  return slots_ && slots_->intents[index]
      ? slots_->intents[index]->intent(RemoteDelivery::ServerStream)
      : ConsumeIntent{};
}
RemoteChannelResource &ServerStreamBackend::resourceAt(int index) {
  return slots_ && slots_->resources[index] ? *slots_->resources[index]
                                           : emptyResource_;
}
bool ServerStreamBackend::allocateResource(int index) {
  if (!allocateSlots()) return false;
  if (!slots_->resources[index])
    slots_->resources[index] = new (std::nothrow) Resource;
  if (!slots_->resources[index]) trimSlots();
  return slots_ && slots_->resources[index];
}
void ServerStreamBackend::clearResource(int index) {
  if (!slots_) return;
  delete slots_->resources[index];
  slots_->resources[index] = nullptr;
  trimSlots();
}
bool ServerStreamBackend::storeIntent(int index, const ConsumeIntent &intent) {
  if (!intent.consumerId) {
    if (slots_) {
      delete slots_->intents[index];
      slots_->intents[index] = nullptr;
      trimSlots();
    }
    return true;
  }
  if (!allocateSlots()) return false;
  if (!slots_->intents[index])
    slots_->intents[index] = new (std::nothrow) ChannelConsumer;
  if (!slots_->intents[index]) return false;
  slots_->intents[index]->assign(intent);
  return true;
}
void ServerStreamBackend::invalidate(Resource *resource) {
  resource->effective = 0;
  resource->hasSnapshot = false;
  // Generation 0 fences borrowed views immediately. A later admitted snapshot
  // gets a fresh manager generation, never the generation of freed storage.
  resource->generation = 0;
  resource->continuityPending = true;
}
void ServerStreamBackend::reconcileResource(RemoteChannelResource &entry) {
  auto &resource = static_cast<Resource &>(entry);
  if (!srpc_->serverStreamReady()) {
    invalidate(&resource);
    resource.interest = Interest::None;
    resource.ensureAttempted = resource.manualAttempted = false;
  }
  resource.access = resource.effective & SupLan::kPermissionRead
      ? RemoteAccess::READY : resource.interest == Interest::Rejected
          ? RemoteAccess::BLOCKED : RemoteAccess::PENDING;
}
bool ServerStreamBackend::reachable(const RemoteChannelResource &) const {
  return srpc_->serverStreamReady();
}
void ServerStreamBackend::ensureStarted(RemoteChannelResource &entry) {
  static_cast<Resource &>(entry).interest = Interest::Requested;
}
void ServerStreamBackend::ensureCompleted(RemoteChannelResource &entry,
    const TSD_SuplaEnsureResourceAccessResult &result) {
  auto &resource = static_cast<Resource &>(entry);
  if (result.Result == SUPLA_SUPLAN_RESULT_OK &&
      result.AccessStatus == SUPLA_SUPLAN_ACCESS_STATUS_GRANTED) {
    resource.interest = Interest::Granted;
    if (resource.hasSnapshot) resource.effective = SupLan::kPermissionRead;
  } else if (result.Result == SUPLA_SUPLAN_RESULT_OK &&
             result.AccessStatus ==
                 SUPLA_SUPLAN_ACCESS_STATUS_PENDING_APPROVAL) {
    invalidate(&resource);
    resource.interest = Interest::Pending;
  } else {
    invalidate(&resource);
    resource.interest = Interest::Rejected;
  }
}
void ServerStreamBackend::disconnected() {
  if (!slots_) return;
  for (auto resource : slots_->resources) {
    if (!resource) continue;
    invalidate(resource);
    resource->interest = Interest::None;
    resource->ensureAttempted = resource->manualAttempted = false;
    resource->approval = SUPLA_SUPLAN_ACCESS_STATUS_INVALID;
  }
}
bool ServerStreamBackend::receive(const TSD_SuplaRemoteChannelState &state,
                                  uint32_t nowMs) {
  if (!manager_ || !slots_ || !srpc_->serverStreamReady() ||
      state.ChannelId <= 0 || state.Channel.Number != 0xff ||
      state.Channel.Type == 0 ||
      state.Channel.Offline > SUPLA_CHANNEL_OFFLINE_FLAG_MAX) return false;
  for (auto resource : slots_->resources) {
    if (!resource || resource->id.id !=
          static_cast<uint32_t>(state.ChannelId)) continue;
    if (resource->interest == Interest::None ||
        resource->interest == Interest::Rejected) return false;
    // A current authenticated full state is also the Server's post-approval
    // activation signal. Pending alone never exposes any cached bytes.
    if (!resource->generation) manager_->invalidate(resource);
    manager_->acceptSnapshot(resource,
        reinterpret_cast<const uint8_t *>(&state.Channel),
        sizeof(state.Channel), nowMs);
    if (resource->interest != Interest::Requested) {
      resource->interest = Interest::Granted;
      resource->effective = SupLan::kPermissionRead;
      resource->approval = SUPLA_SUPLAN_ACCESS_STATUS_GRANTED;
    }
    return true;
  }
  return false;
}
size_t ServerStreamBackend::allocatedBytes() const {
  size_t bytes = sizeof(*this);
  if (slots_) {
    bytes += sizeof(*slots_);
    for (auto resource : slots_->resources)
      if (resource) bytes += sizeof(Resource);
    for (auto intent : slots_->intents)
      if (intent) bytes += sizeof(ChannelConsumer);
  }
  return bytes;
}
}  // namespace Device
}  // namespace Supla
#endif

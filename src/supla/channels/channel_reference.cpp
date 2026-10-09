// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "channel_reference.h"

#include <supla/channels/channel.h>
#include <supla/channels/channel_state.h>
#include <supla/protocol/protocol_layer.h>
#include <supla/time.h>
#ifndef ARDUINO_ARCH_AVR
#include <supla/suplan/suplan_server_identity.h>
#include <supla/suplan/remote_resource_manager.h>
#endif

namespace Supla {
const Device::ServerIdentity *acceptedReferenceIdentity() {
#ifndef ARDUINO_ARCH_AVR
  return Device::ServerIdentity::current();
#endif
  return nullptr;
}

bool usesServerReferences() {
#ifndef ARDUINO_ARCH_AVR
  const auto identity = acceptedReferenceIdentity();
  return identity && identity->capable();
#else
  return false;
#endif
}

void releaseChannelConsumer(uint32_t consumer) {
#ifndef ARDUINO_ARCH_AVR
  for (auto layer = Protocol::ProtocolLayer::first(); layer;
       layer = layer->next()) {
    if (auto manager = layer->remoteResources()) manager->remove(consumer);
  }
#else
  (void)consumer;
#endif
}

ChannelState consumeChannelState(ChannelReference reference,
                                 uint32_t consumer) {
  const auto resolved =
      resolveChannelReference(reference, acceptedReferenceIdentity());
#ifndef ARDUINO_ARCH_AVR
  if (resolved.kind == ChannelResolutionKind::kRemote) {
    for (auto layer = Protocol::ProtocolLayer::first(); layer;
         layer = layer->next()) {
      if (auto manager = layer->remoteResources()) {
        const SupLan::ResourceId resource = {SupLan::kResourceTypeChannel,
                                             resolved.resourceId};
        // Never borrow another consumer's cache when this demand has no slot.
        if (!manager->consume({consumer, resource, SupLan::kPermissionRead}))
          return ChannelState();
        return manager->state(resource, millis());
      }
    }
  }
#endif
  releaseChannelConsumer(consumer);
  return resolved.kind == ChannelResolutionKind::kLocal
             ? ChannelState(resolved.channel) : ChannelState();
}

int localReferenceNumber(ChannelReference reference) {
  if (reference.valid() &&
      reference.kind == ChannelReferenceKind::LOCAL_CHANNEL_NUMBER)
    return reference.id;
  const auto resolved = resolveChannelReference(reference,
                                                acceptedReferenceIdentity());
  return resolved.kind == ChannelResolutionKind::kLocal
             ? resolved.channel->getChannelNumber() : -1;
}

bool referenceWireId(ChannelReference reference, uint32_t *id) {
  if (!id || !reference.valid()) return false;
  *id = 0;
  if (reference.kind == ChannelReferenceKind::NONE) return true;
  if (reference.kind == ChannelReferenceKind::SERVER_CHANNEL) {
    *id = reference.id;
    return true;
  }
  if (!usesServerReferences()) {
    *id = reference.id;
    return true;
  }
#ifndef ARDUINO_ARCH_AVR
  const auto identity = acceptedReferenceIdentity();
  if (!identity || !identity->identityAvailable() ||
      !identity->registrationContextValid() || identity->identityTransition())
    return false;
  auto channel = Channel::GetByChannelNumber(reference.id);
  if (!channel || !channel->getServerChannelId()) return false;
  *id = channel->getServerChannelId();
  return true;
#else
  return false;
#endif
}

bool ChannelReference::valid() const {
  switch (kind) {
    case ChannelReferenceKind::NONE:
      return id == 0;
    case ChannelReferenceKind::LOCAL_CHANNEL_NUMBER:
      return id < 255;
    case ChannelReferenceKind::SERVER_CHANNEL:
      return id > 0 && id <= INT32_MAX;
  }
  return false;
}

ChannelResolution resolveChannelReference(
    const ChannelReference &reference, const Device::ServerIdentity *identity) {
  ChannelResolution result;
  if (!reference.valid()) return result;
  if (reference.kind == ChannelReferenceKind::LOCAL_CHANNEL_NUMBER) {
    result.channel = Channel::GetByChannelNumber(reference.id);
    if (result.channel) result.kind = ChannelResolutionKind::kLocal;
    return result;
  }
#ifndef ARDUINO_ARCH_AVR
  if (reference.kind == ChannelReferenceKind::SERVER_CHANNEL && identity) {
    const auto resolved = identity->resolve(reference.id);
    if (resolved.location == Device::ServerChannelLocation::kLocal) {
      result.channel = Channel::GetByChannelNumber(resolved.channelNumber);
      if (result.channel) result.kind = ChannelResolutionKind::kLocal;
    } else if (resolved.location == Device::ServerChannelLocation::kRemote) {
      result.kind = ChannelResolutionKind::kRemote;
      result.resourceId = reference.id;
    }
  }
#else
  (void)identity;
#endif
  return result;
}
}  // namespace Supla

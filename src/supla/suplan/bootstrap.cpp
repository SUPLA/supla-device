// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later
#include "bootstrap.h"
#ifndef ARDUINO_ARCH_AVR
#include <supla/log_wrapper.h>
#include <supla/protocol/protocol_layer.h>

namespace Supla {
namespace Device {
#ifdef SUPLA_TEST
namespace { int allocationBudget = -1; }
void failSupLanAllocationAfter(int count) { allocationBudget = count; }
bool supLanAllocationAllowed() {
  if (allocationBudget == 0) return false;
  if (allocationBudget > 0) --allocationBudget;
  return true;
}
#endif
namespace {
struct Request {
  SuplaDeviceClass *device;
  SupLanFactory factory;
  SupLanStack *stack;
  Request *next;
};
Request *requests = nullptr;
Request *find(SuplaDeviceClass *device) {
  for (auto entry = requests; entry; entry = entry->next)
    if (entry->device == device) return entry;
  return nullptr;
}
int manualCount(SuplaDeviceClass *device, bool *ready) {
  int count = 0;
  *ready = false;
  for (auto proto = Protocol::ProtocolLayer::first(); proto;
       proto = proto->next()) {
    if (proto->getSdc() == device && proto->isSupLan()) {
      ++count;
      *ready = proto->isSupLanServerCapable();
    }
  }
  return count;
}
}  // namespace
bool supLanRequested(SuplaDeviceClass *device) {
  return find(device) || hasSupLanServerStack(device);
}
bool hasSupLanServerStack(SuplaDeviceClass *device) {
  bool ready;
  return manualCount(device, &ready) == 1 && ready;
}
bool requestSupLan(SuplaDeviceClass *device, SupLanFactory factory) {
  if (find(device)) return true;
  bool ready;
  const int count = manualCount(device, &ready);
  if (count > 1 || (count == 1 && !ready)) {
    SUPLA_LOG_ERROR("SupLAN: existing manual stack is incomplete/duplicated");
    return false;
  }
  if (!count && !factory) {
    SUPLA_LOG_ERROR("SupLAN: automatic bootstrap unsupported on this platform");
    return false;
  }
  auto entry = allocateSupLan<Request>(
      Request{device, factory, nullptr, requests});
  if (!entry) {
    SUPLA_LOG_ERROR("SupLAN: cannot allocate bootstrap request");
    return false;
  }
  requests = entry;
  return true;
}
bool startSupLan(SuplaDeviceClass *device, uint8_t version) {
  auto entry = find(device);
  if (!entry) return true;  // Explicit opt-in only.
  if (version < 29 || version > SUPLA_PROTO_VERSION) {
    SUPLA_LOG_ERROR("SupLAN: begin requires protocol version >= 29");
    return false;
  }
  bool ready;
  const int count = manualCount(device, &ready);
  if (count) {
    // Borrow; never delete a manual stack.
    if (count == 1 && ready) return true;
    SUPLA_LOG_ERROR("SupLAN: existing manual stack is incomplete/duplicated");
    return false;
  }
  entry->stack = entry->factory ? entry->factory(device, version) : nullptr;
  if (!entry->stack || !entry->stack->ready() ||
      !hasSupLanServerStack(device)) {
    delete entry->stack;
    entry->stack = nullptr;
    SUPLA_LOG_ERROR("SupLAN: production stack initialization failed");
    return false;
  }
  return true;
}
void releaseSupLan(SuplaDeviceClass *device) {
  auto link = &requests;
  while (*link) {
    if ((*link)->device == device) {
      auto entry = *link;
      *link = entry->next;
      delete entry->stack;
      delete entry;
      return;
    }
    link = &(*link)->next;
  }
}
}  // namespace Device
}  // namespace Supla
#endif  // !ARDUINO_ARCH_AVR

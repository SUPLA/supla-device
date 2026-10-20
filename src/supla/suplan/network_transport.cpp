// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later
#include "network_transport.h"
#ifndef ARDUINO_ARCH_AVR
#include <supla/network/network.h>
#include <supla/log_wrapper.h>
namespace Supla {
namespace Device {
bool SupLanNetworkTransport::networkReady(uint32_t) {
  uint32_t fingerprint = 0;
  uint32_t address = 0;
  for (auto net = Network::FirstInstance(); net;
       net = Network::NextInstance(net)) {
    if (!net->isReady()) continue;
    const uint32_t ip = net->getIP();
    if (!ip) continue;
    if (!address) address = ip;
    fingerprint = (fingerprint * 16777619u) ^ ip;
  }
  if (!address) {
    if (address_) SUPLA_LOG_INFO("SupLAN UDP network unavailable");
    address_ = 0;
    return false;
  }
  if (fingerprint != fingerprint_ || !address_) {
    SUPLA_LOG_INFO("SupLAN UDP interface/address changed local=%u.%u.%u.%u",
        address & 255, (address >> 8) & 255, (address >> 16) & 255,
        address >> 24);
  }
  if (isOpen() && fingerprint != fingerprint_) close();
  address_ = address;
  fingerprint_ = fingerprint;
  return true;
}
}  // namespace Device
}  // namespace Supla
#endif

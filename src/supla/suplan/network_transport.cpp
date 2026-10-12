// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later
#include "network_transport.h"
#ifndef ARDUINO_ARCH_AVR
#include <supla/network/network.h>
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
  if (!address) return false;
  if (isOpen() && fingerprint != fingerprint_) close();
  address_ = address;
  fingerprint_ = fingerprint;
  return true;
}
}  // namespace Device
}  // namespace Supla
#endif

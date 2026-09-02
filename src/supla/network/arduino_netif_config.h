// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_NETWORK_ARDUINO_NETIF_CONFIG_H_
#define SRC_SUPLA_NETWORK_ARDUINO_NETIF_CONFIG_H_

#include <Arduino.h>
#include <IPAddress.h>

#include <stdint.h>

namespace Supla {

inline IPAddress toArduinoIpAddress(uint32_t ip) {
  return IPAddress((ip >> 24) & 0xFF,
                   (ip >> 16) & 0xFF,
                   (ip >> 8) & 0xFF,
                   ip & 0xFF);
}

}  // namespace Supla

#endif  // SRC_SUPLA_NETWORK_ARDUINO_NETIF_CONFIG_H_

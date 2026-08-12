// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_NETWORK_IP_ADDRESS_H_
#define SRC_SUPLA_NETWORK_IP_ADDRESS_H_

#ifdef ARDUINO
#include <IPAddress.h>
#else

#include <stdint.h>

#include <string>

class IPAddress {
 public:
  IPAddress();
  IPAddress(uint8_t ip1, uint8_t ip2, uint8_t ip3, uint8_t ip4);
  explicit IPAddress(const std::string &ip);

  uint8_t operator[](int index) const;
  uint8_t& operator[](int index);

 protected:
  union {
    uint8_t addr[4] = {};
    uint32_t full;
  };
};
#endif

#endif  // SRC_SUPLA_NETWORK_IP_ADDRESS_H_

// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "ip_address.h"

#ifndef ARDUINO

#include <arpa/inet.h>
#include <string>

IPAddress::IPAddress() {}

IPAddress::IPAddress(uint8_t ip1, uint8_t ip2, uint8_t ip3, uint8_t ip4) {
    addr[0] = ip1;
    addr[1] = ip2;
    addr[2] = ip3;
    addr[3] = ip4;
}

IPAddress::IPAddress(const std::string &ip) {
  full = inet_addr(ip.c_str());
}

uint8_t IPAddress::operator[](int index) const {
  return addr[index];
}

uint8_t& IPAddress::operator[](int index) {
  return addr[index];
}
#endif

// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_PORTING_FREERTOS_LINUX_IPADDRESS_H_
#define EXTRAS_PORTING_FREERTOS_LINUX_IPADDRESS_H_

#include <cstdint>

typedef union {
  uint8_t addr[4];
  uint32_t full;
} IPAddress;

#endif  // EXTRAS_PORTING_FREERTOS_LINUX_IPADDRESS_H_

// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_TEST_ESP32TCPABORTTESTS_FAKES_LWIP_PRIV_TCP_PRIV_H_
#define EXTRAS_TEST_ESP32TCPABORTTESTS_FAKES_LWIP_PRIV_TCP_PRIV_H_

#include <lwip/tcp.h>

struct tcp_seg {
  struct tcp_seg *next;
  uint16_t len;
};

#endif  // EXTRAS_TEST_ESP32TCPABORTTESTS_FAKES_LWIP_PRIV_TCP_PRIV_H_

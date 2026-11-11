// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_TEST_ESP32TCPABORTTESTS_FAKES_LWIP_TCP_H_
#define EXTRAS_TEST_ESP32TCPABORTTESTS_FAKES_LWIP_TCP_H_

#include <stdint.h>

struct tcp_seg;
enum tcp_state { CLOSED, LISTEN, SYN_SENT, ESTABLISHED, FIN_WAIT_1 };
struct tcp_pcb {
  enum tcp_state state;
  struct tcp_seg *unacked;
  struct tcp_seg *unsent;
  void *callback_arg;
};
void tcp_abort(struct tcp_pcb *pcb);

#endif  // EXTRAS_TEST_ESP32TCPABORTTESTS_FAKES_LWIP_TCP_H_

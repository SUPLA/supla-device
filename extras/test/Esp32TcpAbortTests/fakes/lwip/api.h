// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_TEST_ESP32TCPABORTTESTS_FAKES_LWIP_API_H_
#define EXTRAS_TEST_ESP32TCPABORTTESTS_FAKES_LWIP_API_H_

#include <lwip/tcp.h>

#define ERR_OK 0
#define ERR_MEM -1
#define NETCONN_TCP 0x10
#define NETCONN_TCP_IPV6 0x18
#define NETCONN_UDP 0x20
#define NETCONNTYPE_GROUP(type) ((type) & 0xf0)
#define netconn_type(conn) ((conn)->type)
typedef int err_t;
struct netconn {
  int type;
  union { struct tcp_pcb *tcp; } pcb;
};

#endif  // EXTRAS_TEST_ESP32TCPABORTTESTS_FAKES_LWIP_API_H_

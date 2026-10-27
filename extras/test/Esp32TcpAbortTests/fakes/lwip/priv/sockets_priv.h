// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_TEST_ESP32TCPABORTTESTS_FAKES_LWIP_PRIV_SOCKETS_PRIV_H_
#define EXTRAS_TEST_ESP32TCPABORTTESTS_FAKES_LWIP_PRIV_SOCKETS_PRIV_H_

#include <lwip/api.h>

#ifndef LWIP_SOCKET_OFFSET
#define LWIP_SOCKET_OFFSET 4
#endif
#define NUM_SOCKETS 16

#ifdef __cplusplus
extern "C" {
#endif
struct lwip_sock { struct netconn *conn; };
#ifdef __cplusplus
}
#endif
// Deliberately outside extern "C", matching esp-lwip fd432e4.
struct lwip_sock *lwip_socket_dbg_get_socket(int fd);

#endif  // EXTRAS_TEST_ESP32TCPABORTTESTS_FAKES_LWIP_PRIV_SOCKETS_PRIV_H_

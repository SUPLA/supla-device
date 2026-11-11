// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_TEST_ESP32TCPABORTTESTS_FAKES_LWIP_PRIV_TCPIP_PRIV_H_
#define EXTRAS_TEST_ESP32TCPABORTTESTS_FAKES_LWIP_PRIV_TCPIP_PRIV_H_

#include <lwip/api.h>

struct tcpip_api_call_data { int unused; };
typedef err_t (*tcpip_api_call_fn)(struct tcpip_api_call_data *);
err_t tcpip_api_call(tcpip_api_call_fn fn, struct tcpip_api_call_data *call);

#endif  // EXTRAS_TEST_ESP32TCPABORTTESTS_FAKES_LWIP_PRIV_TCPIP_PRIV_H_

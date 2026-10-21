// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef EXTRAS_TEST_ASYNCDNSTESTS_FAKES_LWIP_TCPIP_H_
#define EXTRAS_TEST_ASYNCDNSTESTS_FAKES_LWIP_TCPIP_H_
#include "dns.h"
err_t tcpip_try_callback(void (*)(void *), void *);
#endif  // EXTRAS_TEST_ASYNCDNSTESTS_FAKES_LWIP_TCPIP_H_

// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright 2026 AC SOFTWARE SP. Z O.O.

#ifndef EXTRAS_TEST_SUPLANESPUDPTESTS_FAKES_LWIP_INET_H_
#define EXTRAS_TEST_SUPLANESPUDPTESTS_FAKES_LWIP_INET_H_

#include <arpa/inet.h>
#define lwip_htons htons
#define lwip_ntohs ntohs
#define lwip_htonl htonl
#define lwip_inet_pton inet_pton

#endif  // EXTRAS_TEST_SUPLANESPUDPTESTS_FAKES_LWIP_INET_H_

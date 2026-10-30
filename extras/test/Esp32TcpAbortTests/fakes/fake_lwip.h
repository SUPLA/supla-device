// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_TEST_ESP32TCPABORTTESTS_FAKES_FAKE_LWIP_H_
#define EXTRAS_TEST_ESP32TCPABORTTESTS_FAKES_FAKE_LWIP_H_

#ifdef __cplusplus
extern "C" {
#endif
#include <lwip/priv/sockets_priv.h>
#include <lwip/priv/tcpip_priv.h>

extern struct lwip_sock *fakeSocket;
extern int fakeResult;
extern int lookupCount;
extern int apiCallCount;
extern int abortCount;
extern int coreLocked;
extern int lastFd;
extern char lastLog[256];
extern void (*fakeAbortObserver)(int fd);
void fakeLwipReset(void);
void fakeLwipConnect(void);
#ifdef __cplusplus
}
#endif

#endif  // EXTRAS_TEST_ESP32TCPABORTTESTS_FAKES_FAKE_LWIP_H_

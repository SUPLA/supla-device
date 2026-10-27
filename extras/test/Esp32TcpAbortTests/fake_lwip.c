// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <fake_lwip.h>
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>

struct lwip_sock *fakeSocket;
int fakeResult;
int lookupCount;
int apiCallCount;
int abortCount;
int coreLocked;
int inTcpipTask;
int lastFd;
char lastLog[256];

void fakeLwipLog(const char *format, ...) {
  // Diagnostics must not run under the core lock or in the tcpip callback.
  assert(!coreLocked && !inTcpipTask);
  va_list args;
  va_start(args, format);
  vsnprintf(lastLog, sizeof(lastLog), format, args);
  va_end(args);
}

// C implementation: linking the real C++ helper against this also verifies
// the socket lookup declaration has C linkage.
struct lwip_sock *lwip_socket_dbg_get_socket(int fd) {
  assert(coreLocked || inTcpipTask);
  ++lookupCount;
  lastFd = fd;
  return fakeSocket;
}

err_t tcpip_api_call(tcpip_api_call_fn fn, struct tcpip_api_call_data *call) {
  ++apiCallCount;
  if (fakeResult != ERR_OK) {
    return fakeResult;
  }
#if LWIP_TCPIP_CORE_LOCKING
  coreLocked = 1;
#else
  inTcpipTask = 1;
#endif
  const err_t result = fn(call);
  coreLocked = 0;
  inTcpipTask = 0;
  return result;
}

void tcp_abort(struct tcp_pcb *pcb) {
  assert(coreLocked || inTcpipTask);
  assert(pcb);
  ++abortCount;
  // Model netconn's err_tcp callback, rather than freeing the stack fixture.
  ((struct netconn *)pcb->callback_arg)->pcb.tcp = 0;
}

// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <fake_lwip.h>
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

struct lwip_sock *fakeSocket;
int fakeResult;
int lookupCount;
int apiCallCount;
int abortCount;
int coreLocked;
int lastFd;
char lastLog[256];
void (*fakeAbortObserver)(int fd);

void fakeLwipReset(void) {
  fakeSocket = 0;
  fakeResult = ERR_OK;
  lookupCount = apiCallCount = abortCount = 0;
  coreLocked = 0;
  lastFd = -1;
  lastLog[0] = 0;
  fakeAbortObserver = 0;
}

void fakeLwipConnect(void) {
  static struct tcp_pcb pcb;
  static struct netconn conn;
  static struct lwip_sock socket;
  memset(&pcb, 0, sizeof(pcb));
  pcb.state = ESTABLISHED;
  pcb.callback_arg = &conn;
  conn.type = NETCONN_TCP;
  conn.pcb.tcp = &pcb;
  socket.conn = &conn;
  fakeSocket = &socket;
}

void fakeLwipLog(const char *format, ...) {
  // Diagnostics must not run under the core lock or in the tcpip callback.
  assert(!coreLocked);
  va_list args;
  va_start(args, format);
  vsnprintf(lastLog, sizeof(lastLog), format, args);
  va_end(args);
}

// C implementation: linking the real C++ helper against this also verifies
// the socket lookup declaration has C linkage.
struct lwip_sock *lwip_socket_dbg_get_socket(int fd) {
  assert(coreLocked);
  ++lookupCount;
  lastFd = fd;
  return fakeSocket;
}

err_t tcpip_api_call(tcpip_api_call_fn fn, struct tcpip_api_call_data *call) {
  ++apiCallCount;
  if (fakeResult != ERR_OK) {
    return fakeResult;
  }
  coreLocked = 1;
  const err_t result = fn(call);
  coreLocked = 0;
  return result;
}

void tcp_abort(struct tcp_pcb *pcb) {
  assert(coreLocked);
  assert(pcb);
  ++abortCount;
  if (fakeAbortObserver) fakeAbortObserver(lastFd);
  // Model netconn's err_tcp callback, rather than freeing the stack fixture.
  ((struct netconn *)pcb->callback_arg)->pcb.tcp = 0;
}

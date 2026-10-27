// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "esp32_tcp_abort.h"

#if defined(ARDUINO_ARCH_ESP32) || defined(SUPLA_DEVICE_ESP32) || \
    defined(ESP_PLATFORM)

#include <inttypes.h>
#include <supla/log_wrapper.h>

// In esp-lwip fd432e4, the debug socket lookup is declared outside the header's
// own extern "C" block. Keep that declaration's C linkage local to this helper.
extern "C" {
#include <lwip/api.h>
#include <lwip/priv/sockets_priv.h>
#include <lwip/priv/tcp_priv.h>
#include <lwip/priv/tcpip_priv.h>
#include <lwip/tcp.h>
}

namespace {
struct AbortCall {
  tcpip_api_call_data call;
  int fd;
  uint32_t queuedBytes;
  bool aborted;
};

err_t abortInCore(tcpip_api_call_data *call) {
  auto *request = reinterpret_cast<AbortCall *>(call);
  auto *socket = lwip_socket_dbg_get_socket(request->fd);
  if (!socket || !socket->conn ||
      NETCONNTYPE_GROUP(netconn_type(socket->conn)) != NETCONN_TCP) {
    return ERR_OK;
  }
  auto *pcb = socket->conn->pcb.tcp;
  if (!pcb || pcb->state == LISTEN) {
    return ERR_OK;
  }
  for (auto *segment = pcb->unacked; segment; segment = segment->next) {
    request->queuedBytes += segment->len;
  }
  for (auto *segment = pcb->unsent; segment; segment = segment->next) {
    request->queuedBytes += segment->len;
  }
  // netconn's err_tcp callback clears conn->pcb and wakes socket waiters.
  // Do not access pcb after abort; normal close will release the socket itself.
  tcp_abort(pcb);
  request->aborted = true;
  return ERR_OK;
}
}  // namespace

void Supla::abortTcpConnection(int fd) {
  if (fd < LWIP_SOCKET_OFFSET || fd >= LWIP_SOCKET_OFFSET + NUM_SOCKETS) {
    return;
  }
  AbortCall request = {};
  request.fd = fd;
  const auto result = tcpip_api_call(abortInCore, &request.call);
  if (result != ERR_OK) {
    SUPLA_LOG_WARNING("TCP abort: fd %d synchronization failed (%d)", fd,
                      static_cast<int>(result));
    return;
  }
  if (request.aborted) {
    SUPLA_LOG_DEBUG("TCP abort: fd %d, queued data bytes %" PRIu32,
                    fd, request.queuedBytes);
  }
}

#endif  // ESP32 Arduino / ESP-IDF

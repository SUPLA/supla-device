// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_NETWORK_ESP32_TCP_ABORT_H_
#define SRC_SUPLA_NETWORK_ESP32_TCP_ABORT_H_

#if defined(ARDUINO_ARCH_ESP32) || defined(SUPLA_DEVICE_ESP32) || \
    defined(ESP_PLATFORM)
namespace Supla {

// Abort an owned outgoing TCP socket before its normal socket/TLS cleanup.
// The caller must exclusively own the client: no concurrent close, reuse of
// fd, or client I/O is allowed. The socket lookup does not acquire a reference.
// Call only from an application task, never an ISR, lwIP callback or tcpip task
// (tcpip_api_call can wait for that task). Do not already hold the lwIP core
// lock. EspAsyncClient retains exclusive fd ownership through closeTransport().
// Invalid/non-TCP sockets and synchronization failures leave normal close as
// the fallback. This function does not close fd or free the TLS context.
void abortTcpConnection(int fd);

}  // namespace Supla
#endif

#endif  // SRC_SUPLA_NETWORK_ESP32_TCP_ABORT_H_

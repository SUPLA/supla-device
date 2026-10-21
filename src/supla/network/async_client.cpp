// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "async_client.h"
#ifndef ARDUINO_ARCH_AVR
#include <supla/time.h>
#include <string.h>

void Supla::AsyncClient::stop() {
  state_ = ConnectResult::Failed;
  closeTransport();
  host_[0] = 0;
  srcIp = 0;
}

void Supla::AsyncClient::setTimeoutMs(uint16_t value) {
  timeoutMs = value ? value : 1;
}

void Supla::AsyncClient::setSSLEnabled(bool value) {
  if (sslEnabled != value) stop();
  Client::setSSLEnabled(value);
}

void Supla::AsyncClient::setCACert(const char *value) {
  // Callers may replace PEM bytes in the same storage buffer.
  stop();
  Client::setCACert(value);
}

Supla::Client::ConnectResult Supla::AsyncClient::connectStep(
    const char *host, uint16_t port) {
  if (!host || !host[0] || strlen(host) >= sizeof(host_)) {
    stop();
    return ConnectResult::Failed;
  }
  if (state_ != ConnectResult::Failed &&
      (port != port_ || strcmp(host, host_))) stop();
  if (state_ == ConnectResult::Connected && !transportConnected()) {
    stop();
    return ConnectResult::Failed;
  }
  if (state_ == ConnectResult::Failed) {
    stop();
    memcpy(host_, host, strlen(host) + 1);
    port_ = port;
    startedMs_ = millis();
    connectionError_ = ConnectionError::NONE;
    logConnecting(host_, port);
    state_ = startTransport(host_, port);
  } else if (state_ == ConnectResult::Pending) {
    if (static_cast<uint32_t>(millis() - startedMs_) >= timeoutMs) {
      connectionError_ = ConnectionError::CONNECTION_TIMEOUT;
      stop();
      return ConnectResult::Failed;
    }
    state_ = pollTransport();
  }
  if (state_ == ConnectResult::Failed) {
    if (connectionError_ == ConnectionError::NONE)
      connectionError_ = ConnectionError::UNKNOWN;
    stop();
  }
  return state_;
}

// Explicit legacy connect() remains synchronous. SRPC/autodiscovery use steps.
int Supla::AsyncClient::connectImp(const char *host, uint16_t port) {
  stop();
  ConnectResult result;
  do {
    result = connectStep(host, port);
    if (result == ConnectResult::Pending) delay(1);
  } while (result == ConnectResult::Pending);
  return result == ConnectResult::Connected ? 1 : 0;
}

uint8_t Supla::AsyncClient::connected() {
  return state_ == ConnectResult::Connected && transportConnected();
}

Supla::ConnectionError Supla::AsyncClient::getConnectionError() const {
  return connectionError_;
}

// No retained TX data: success means the entire write reached the transport.
size_t Supla::AsyncClient::writeImp(const uint8_t *data, size_t size) {
  if (!connected() || (!data && size)) return 0;
  const size_t total = size;
  uint32_t progress = millis();
  while (size) {
    const int sent = transmit(data, size);
    if (sent == 0 || (sent > 0 && static_cast<size_t>(sent) > size)) {
      connectionError_ = ConnectionError::CONNECTION_LOST;
      stop();
      return 0;
    }
    if (sent < 0) {
      if (!connected() ||
          static_cast<uint32_t>(millis() - progress) >= timeoutMs) {
        connectionError_ = ConnectionError::CONNECTION_TIMEOUT;
        stop();
        return 0;
      }
      delay(1);
      continue;
    }
    data += sent;
    size -= sent;
    progress = millis();
  }
  return total;
}
#endif  // !ARDUINO_ARCH_AVR

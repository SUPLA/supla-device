// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "async_client.h"
#ifndef ARDUINO_ARCH_AVR
#include <SuplaDevice.h>
#include <supla/log_wrapper.h>
#include <supla/time.h>
#include <stdio.h>
#include <string.h>
#include <new>

// Last state texts stay in flash on ESP8266, where .rodata occupies RAM.
#if defined(ARDUINO_ARCH_ESP8266)
#include <pgmspace.h>
#define REPORT_TEXT(buffer, format, ...) \
  snprintf_P(buffer, sizeof(buffer), PSTR(format), ##__VA_ARGS__)
#else
#define REPORT_TEXT(buffer, format, ...) \
  snprintf(buffer, sizeof(buffer), format, ##__VA_ARGS__)
#endif

Supla::AsyncClient::~AsyncClient() {
  // Derived destructors stop() first; this only guards against a leak.
  delete[] host_;
}

void Supla::AsyncClient::stop() {
  state_ = ConnectResult::Failed;
  // Resolver work may borrow host_ until closeTransport() cancels it.
  closeTransport();
  delete[] host_;
  host_ = nullptr;
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

void Supla::AsyncClient::noteProgress(uint16_t minimumMs) {
  progressMs_ = millis();
  budgetMs_ = minimumMs > timeoutMs ? minimumMs : timeoutMs;
}

void Supla::AsyncClient::setFailure(Failure failure, int tlsError) {
  if (failure_) return;
  failure_ = failure;
  tlsError_ = static_cast<int16_t>(tlsError);
}

void Supla::AsyncClient::reportFailure() {
  const uint16_t failure =
      connectionError_ == ConnectionError::CONNECTION_TIMEOUT
          ? static_cast<uint16_t>(kConnectionTimeout) : failure_;
  const int16_t tlsError = failure == failure_ ? tlsError_ : 0;
  if (!failure || !sdc ||
      (failure == lastFailure_ && tlsError == lastTlsError_))
    return;
  lastFailure_ = failure;
  lastTlsError_ = tlsError;
  const char *host = connectionHost();
  char text[128] = {};
  switch (failure) {
    case kCannotResolveHostname:
      REPORT_TEXT(text, "Connection: can't resolve hostname \"%s\"", host);
      break;
    case kFailedConnectToHost:
      REPORT_TEXT(text, "Connection: failed connect to host \"%s\"", host);
      break;
    case kConnectionTimeout:
      REPORT_TEXT(text, "Connection: connection timeout to host \"%s\"",
                  host);
      break;
    case kCaParseFailed:
      REPORT_TEXT(text,
                  "CA certificate parsing error - please provide correct one");
      break;
    case kTlsHandshakeFailed:
      if (tlsError == kTlsCertVerifyFailed) {
        REPORT_TEXT(text,
                    "Connection TLS: handshake fail - server certificate "
                    "verification error \"%s\"", host);
      } else {
        REPORT_TEXT(text, "Connection TLS: handshake fail (TLS 0x%X, \"%s\")",
                    static_cast<unsigned>(tlsError), host);
      }
      break;
    default:
      REPORT_TEXT(text, "Connection: error 0x%X (TLS 0x%X \"%s\")",
                  static_cast<unsigned>(failure),
                  static_cast<unsigned>(tlsError), host);
      break;
  }
  sdc->addLastStateLog(text);
}

Supla::Client::ConnectResult Supla::AsyncClient::connectStep(
    const char *host, uint16_t port) {
  return step(host, port, true);
}

Supla::Client::ConnectResult Supla::AsyncClient::step(
    const char *host, uint16_t port, bool log) {
  const size_t length = host ? strlen(host) : 0;
  if (!length || length > 255) {
    stop();
    return ConnectResult::Failed;
  }
  if (state_ != ConnectResult::Failed &&
      (port != port_ || strcmp(host, host_))) stop();
  if (state_ == ConnectResult::Connected) {
    if (transportConnected()) return state_;
    stop();
    return ConnectResult::Failed;
  }
  if (state_ == ConnectResult::Failed) {
    stop();
    connectionError_ = ConnectionError::NONE;
    failure_ = 0;
    tlsError_ = 0;
    host_ = new (std::nothrow) char[length + 1];
    if (!host_) {
      connectionError_ = ConnectionError::UNKNOWN;
      return ConnectResult::Failed;
    }
    memcpy(host_, host, length + 1);
    port_ = port;
    noteProgress();
    if (log) logConnecting(host_, port);
    state_ = startTransport(host_, port);
  } else if (static_cast<uint32_t>(millis() - progressMs_) >= budgetMs_) {
    connectionError_ = ConnectionError::CONNECTION_TIMEOUT;
    state_ = ConnectResult::Failed;
  } else {
    state_ = pollTransport();
  }
  if (state_ == ConnectResult::Failed) {
    if (connectionError_ == ConnectionError::NONE)
      connectionError_ = ConnectionError::UNKNOWN;
    // Before stop(): the report names the host.
    reportFailure();
    stop();
  } else if (state_ == ConnectResult::Connected) {
    SUPLA_LOG_DEBUG("Connected via IP %d.%d.%d.%d",
                    static_cast<int>(srcIp & 0xFF),
                    static_cast<int>((srcIp >> 8) & 0xFF),
                    static_cast<int>((srcIp >> 16) & 0xFF),
                    static_cast<int>((srcIp >> 24) & 0xFF));
  }
  return state_;
}

// Explicit legacy connect() remains synchronous. SRPC/autodiscovery use steps.
// Client::connect() already logged this attempt.
int Supla::AsyncClient::connectImp(const char *host, uint16_t port) {
  stop();
  ConnectResult result;
  // 10 ms is at least one RTOS tick, so waiting yields instead of spinning.
  while ((result = step(host, port, false)) == ConnectResult::Pending)
    delay(10);
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
      delay(10);
      continue;
    }
    data += sent;
    size -= sent;
    progress = millis();
  }
  return total;
}
#endif  // !ARDUINO_ARCH_AVR

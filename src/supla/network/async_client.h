// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_NETWORK_ASYNC_CLIENT_H_
#define SRC_SUPLA_NETWORK_ASYNC_CLIENT_H_

#ifndef ARDUINO_ARCH_AVR
#include "client.h"

namespace Supla {
// Shared cooperative connection lifetime. Writes remain synchronous. Migrated
// transports derive from this class; legacy/AVR Client layouts are unchanged.
class AsyncClient : public Client {
 public:
  ~AsyncClient() override;
  ConnectResult connectStep(const char *host, uint16_t port) override;
  ConnectionError getConnectionError() const override;
  void stop() override;
  uint8_t connected() override;
  void setTimeoutMs(uint16_t value) override;
  void setSSLEnabled(bool value) override;
  void setCACert(const char *value) override;

 protected:
  // ESP-TLS codes (esp_tls_errors.h) of the step that failed. Failures are
  // reported to the last state log like the former EspIdfClient.
  enum Failure : uint16_t {
    kCannotResolveHostname = 0x8001,
    kCannotCreateSocket = 0x8002,
    kFailedConnectToHost = 0x8004,
    kConnectionTimeout = 0x8006,
    kCaParseFailed = 0x8015,
    kTlsSetupFailed = 0x8017,
    kTlsHandshakeFailed = 0x801A,
  };
  // -MBEDTLS_ERR_X509_CERT_VERIFY_FAILED: server certificate rejected.
  static constexpr int kTlsCertVerifyFailed = 0x2700;
  // Resolver allowance of the replaced platform clients (ESP8266 hostByName).
  static constexpr uint16_t kDnsTimeoutMs = 10000;

  virtual ConnectResult startTransport(const char *host, uint16_t port) = 0;
  virtual ConnectResult pollTransport() = 0;
  virtual void closeTransport() = 0;
  virtual bool transportConnected() = 0;
  // >0 consumed bytes, -1 would block, 0 fatal/closed. No waiting for I/O.
  virtual int transmit(const uint8_t *data, size_t size) = 0;
  // Records why the attempt failed; the first call of an attempt wins.
  // tlsError is the positive TLS library code (mbedTLS, BearSSL, OpenSSL).
  void setFailure(Failure failure, int tlsError = 0);
  // Restarts the attempt's inactivity deadline after transport progress
  // (phase change or handshake bytes). minimumMs extends a single phase.
  void noteProgress(uint16_t minimumMs = 0);
  int connectImp(const char *host, uint16_t port) override;
  size_t writeImp(const uint8_t *data, size_t size) override;
  uint16_t timeoutMs = 10000;
  ConnectionError connectionError_ = ConnectionError::NONE;
  const char *connectionHost() const { return host_ ? host_ : ""; }
  bool connectionReady() const { return state_ == ConnectResult::Connected; }

 private:
  ConnectResult step(const char *host, uint16_t port, bool log);
  void reportFailure();

  char *host_ = nullptr;  // Heap copy, held until stop().
  uint32_t progressMs_ = 0;
  // TLS codes fit 16 bits (mbedTLS up to 0x7FFF, BearSSL up to 0x3FF).
  int16_t tlsError_ = 0;
  int16_t lastTlsError_ = 0;  // Last state deduplication, as in EspIdfClient.
  uint16_t failure_ = 0;
  uint16_t lastFailure_ = 0;
  uint16_t budgetMs_ = 0;
  uint16_t port_ = 0;
  ConnectResult state_ = ConnectResult::Failed;
};
}  // namespace Supla
#endif  // !ARDUINO_ARCH_AVR
#endif  // SRC_SUPLA_NETWORK_ASYNC_CLIENT_H_

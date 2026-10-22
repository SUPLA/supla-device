// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_NETWORK_ESP_ASYNC_CLIENT_H_
#define SRC_SUPLA_NETWORK_ESP_ASYNC_CLIENT_H_
#if defined(ARDUINO_ARCH_ESP32) || defined(ESP_PLATFORM)
#include "async_client.h"
#include "async_dns.h"
namespace Supla {
// ESP-IDF and Arduino ESP32 use the platform's existing mbedTLS, with socket
// callbacks that never wait for network readiness (including the handshake).
class EspAsyncClient : public AsyncClient {
 public:
  ~EspAsyncClient() override;
  int available() override;
  bool isSecure() const override;

 protected:
  ConnectResult startTransport(const char *host, uint16_t port) override;
  ConnectResult pollTransport() override;
  void closeTransport() override;
  bool transportConnected() override;
  int transmit(const uint8_t *data, size_t size) override;
  int readImp(uint8_t *data, size_t size) override;

 private:
  struct Tls;
  bool setupTls();
  void tlsFailed(Failure failure, int result);
  static int sendSocket(void *context, const unsigned char *data, size_t size);
  static int receiveSocket(void *context, unsigned char *data, size_t size);
  AsyncDns dns_;
  Tls *tls_ = nullptr;
  int fd_ = -1;
  uint16_t port_ = 0;
  enum class Phase : uint8_t { Dns, Tcp, Tls } phase_ = Phase::Dns;
};
}  // namespace Supla
#endif
#endif  // SRC_SUPLA_NETWORK_ESP_ASYNC_CLIENT_H_

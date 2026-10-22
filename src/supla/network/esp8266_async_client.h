// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_NETWORK_ESP8266_ASYNC_CLIENT_H_
#define SRC_SUPLA_NETWORK_ESP8266_ASYNC_CLIENT_H_
#if defined(ARDUINO_ARCH_ESP8266) && defined(ARDUINO)
#include <lwip/tcp.h>
#include "async_client.h"
#include "async_dns.h"
namespace Supla {
class Esp8266AsyncClient : public AsyncClient {
 public:
  ~Esp8266AsyncClient() override;
  void setFingerprint(const char *value);
  int available() override;
  bool isSecure() const override;

 protected:
  bool isCertificateValidationEnabled() const override;
  ConnectResult startTransport(const char *host, uint16_t port) override;
  ConnectResult pollTransport() override;
  void closeTransport() override;
  bool transportConnected() override;
  size_t writeImp(const uint8_t *data, size_t size) override;
  int transmit(const uint8_t *data, size_t size) override;
  int readImp(uint8_t *data, size_t size) override;

 private:
  struct Tls;
  ConnectResult fail();
  bool setupTls();
  void pumpTls();
  int receiveTcp(uint8_t *data, size_t size);
  int sendTcp(const uint8_t *data, size_t size);
  static err_t onConnected(void *context, tcp_pcb *pcb, err_t error);
  static err_t onReceive(void *context, tcp_pcb *pcb, pbuf *data, err_t error);
  static void onError(void *context, err_t error);
  AsyncDns dns_;
  tcp_pcb *pcb_ = nullptr;
  pbuf *received_ = nullptr;
  uint16_t receiveOffset_ = 0;
  uint16_t port_ = 0;
  Tls *tls_ = nullptr;
  bool tcpReady_ = false;
  bool failed_ = false;
  bool remoteClosed_ = false;
  bool resolving_ = false;
  bool thunkReferenced_ = false;
  bool fingerprintSet_ = false;
  uint8_t *fingerprint_ = nullptr;  // SHA-1, allocated only when set.
};
}  // namespace Supla
#endif
#endif  // SRC_SUPLA_NETWORK_ESP8266_ASYNC_CLIENT_H_

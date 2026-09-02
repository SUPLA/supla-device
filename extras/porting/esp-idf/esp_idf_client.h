// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_PORTING_ESP_IDF_ESP_IDF_CLIENT_H_
#define EXTRAS_PORTING_ESP_IDF_ESP_IDF_CLIENT_H_

#include <supla/network/client.h>
#include <esp_tls.h>

namespace Supla {
class Mutex;

class EspIdfClient : public Client {
 public:
  EspIdfClient();
  virtual ~EspIdfClient();

  int available() override;
  void stop() override;
  uint8_t connected() override;
  ConnectionError getConnectionError() const override;
  void logConnReason(int, int, int, const char *);
  void setTimeoutMs(uint16_t _timeoutMs) override;

 protected:
  int connectImp(const char *host, uint16_t port) override;
  std::size_t writeImp(const uint8_t *buf, std::size_t size) override;
  int readImp(uint8_t *buf, std::size_t size) override;

  Supla::Mutex *mutex = nullptr;
  bool isConnected = false;
  bool firstConnectAfterInit = true;
  esp_tls_t *client = nullptr;
  uint16_t timeoutMs = 10000;
  int lastConnErr = 0;
  int lastTlsErr = 0;
  ConnectionError connectionError = ConnectionError::NONE;
};
};  // namespace Supla


#endif  // EXTRAS_PORTING_ESP_IDF_ESP_IDF_CLIENT_H_

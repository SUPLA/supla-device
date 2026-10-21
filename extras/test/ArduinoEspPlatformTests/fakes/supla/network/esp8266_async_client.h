// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_TEST_ARDUINOESPPLATFORMTESTS_FAKES_SUPLA_NETWORK_ESP8266_ASYNC_CLIENT_H_
#define EXTRAS_TEST_ARDUINOESPPLATFORMTESTS_FAKES_SUPLA_NETWORK_ESP8266_ASYNC_CLIENT_H_

#include "supla/network/async_client.h"

namespace Supla {
// Fake transport only: factory and shared AsyncClient lifecycle are production
// code. This does not emulate or validate BearSSL certificate verification.
class Esp8266AsyncClient : public AsyncClient {
 public:
  Esp8266AsyncClient() { ++created; ++live; }
  ~Esp8266AsyncClient() override { stop(); --live; }
  int available() override { return connected() ? 1 : 0; }
  bool isSecure() const override {
    return connectionReady() && sslEnabled;
  }
  uint16_t timeout() const { return timeoutMs; }
  const char *ca() const { return rootCACert; }
  inline static int created = 0;
  inline static int live = 0;
  inline static int starts = 0;
  inline static int closes = 0;
  inline static int writes = 0;
  inline static bool ready = false;

 protected:
  ConnectResult startTransport(const char *, uint16_t) override {
    ++starts;
    return ConnectResult::Pending;
  }
  ConnectResult pollTransport() override {
    return ready ? ConnectResult::Connected : ConnectResult::Pending;
  }
  void closeTransport() override { ++closes; }
  bool transportConnected() override { return ready; }
  int transmit(const uint8_t *, size_t size) override {
    ++writes;
    return size;
  }
  int readImp(uint8_t *, size_t) override { return -1; }
};
}  // namespace Supla
#endif  // EXTRAS_TEST_ARDUINOESPPLATFORMTESTS_FAKES_SUPLA_NETWORK_ESP8266_ASYNC_CLIENT_H_

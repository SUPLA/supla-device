// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef EXTRAS_TEST_DOUBLES_AUTHENTICATED_CLIENT_STUB_H_
#define EXTRAS_TEST_DOUBLES_AUTHENTICATED_CLIENT_STUB_H_
#include <supla/network/client.h>

// Established authenticated transport for fixtures injecting SRPC messages.
// Transport/handshake rejection is tested separately in SupLanTls.
class AuthenticatedClientStub : public Supla::Client {
 public:
  int available() override { return 0; }
  void stop() override { online = false; }
  uint8_t connected() override { return online; }
  bool isSecure() const override { return online; }
  void setTimeoutMs(uint16_t) override {}

 protected:
  bool isCertificateValidationEnabled() const override { return true; }
  int connectImp(const char *, uint16_t) override { online = true; return 1; }
  size_t writeImp(const uint8_t *, size_t size) override { return size; }
  int readImp(uint8_t *, size_t) override { return 0; }

 private:
  bool online = true;
};
#endif  // EXTRAS_TEST_DOUBLES_AUTHENTICATED_CLIENT_STUB_H_

// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_PORTING_LINUX_LINUX_CLIENT_H_
#define EXTRAS_PORTING_LINUX_LINUX_CLIENT_H_

#include <openssl/ssl.h>
#include <supla/network/client.h>
#include "supla/network/network.h"

namespace Supla {
class LinuxClient : public Client {
 public:
  LinuxClient();
  virtual ~LinuxClient();

  int available() override;
  void stop() override;
  uint8_t connected() override;

  void setTimeoutMs(uint16_t timeoutMs) override;
  void setUseDefaultCACerts(bool useDefault);

 protected:
  bool isCertificateValidationEnabled() const override;
  int readImp(uint8_t *buf, size_t size) override;
  size_t writeImp(const uint8_t *buf, size_t size) override;
  int connectImp(const char *host, uint16_t port) override;

  bool checkSslCerts(SSL *ssl);
  bool setupSslContext();
  int32_t printSslError(SSL *ssl, int ret_code);

  int connectionFd = -1;
  SSL_CTX *ctx = nullptr;
  SSL *ssl = nullptr;
  uint16_t timeoutMs = 3000;
  bool useDefaultCACerts = false;
};
};  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_LINUX_CLIENT_H_

// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_PORTING_LINUX_LINUX_CLIENT_H_
#define EXTRAS_PORTING_LINUX_LINUX_CLIENT_H_

#include <openssl/ssl.h>
#include <supla/network/async_client.h>
#include "supla/network/network.h"

namespace Supla {
class LinuxClient : public AsyncClient {
 public:
  LinuxClient();
  virtual ~LinuxClient();

  int available() override;
  bool isSecure() const override;

  void setUseDefaultCACerts(bool useDefault);

 protected:
  bool isCertificateValidationEnabled() const override;
  int readImp(uint8_t *buf, size_t size) override;
  ConnectResult startTransport(const char *host, uint16_t port) override;
  ConnectResult pollTransport() override;
  void closeTransport() override;
  bool transportConnected() override;
  int transmit(const uint8_t *buf, size_t size) override;
  ConnectResult startTcp();
  ConnectResult startTls();

  bool checkSslCerts(SSL *ssl);
  bool setupSslContext();
  int32_t printSslError(SSL *ssl, int ret_code);

  int connectionFd = -1;
  SSL_CTX *ctx = nullptr;
  SSL *ssl = nullptr;
  enum class Phase { Dns, Tcp, Tls };
  Phase phase = Phase::Dns;
  int dnsSlot = -1;
  uint32_t addresses[8] = {};
  uint8_t addressCount = 0;
  uint8_t addressIndex = 0;
  uint16_t remotePort = 0;
  bool useDefaultCACerts = false;
};
};  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_LINUX_CLIENT_H_

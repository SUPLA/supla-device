/*
 * Copyright (C) AC SOFTWARE SP. Z O.O
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

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

 protected:
  int readImp(uint8_t *buf, size_t size) override;
  size_t writeImp(const uint8_t *buf, size_t size) override;
  int connectImp(const char *host, uint16_t port) override;

  bool checkSslCerts(SSL *ssl);
  int32_t printSslError(SSL *ssl, int ret_code);

  int connectionFd = -1;
  SSL_CTX *ctx = nullptr;
  SSL *ssl = nullptr;
  uint16_t timeoutMs = 3000;
};
};  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_LINUX_CLIENT_H_

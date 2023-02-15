/*
 Copyright (C) AC SOFTWARE SP. Z O.O.

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#ifndef SRC_SUPLA_NETWORK_CLIENT_H_
#define SRC_SUPLA_NETWORK_CLIENT_H_

#include <stddef.h>
#include <stdint.h>

#include "ip_address.h"

class SuplaDeviceClass;

namespace Supla {

class Client {
 public:
  Client();
  virtual ~Client();

  virtual int available() = 0;
  virtual void stop() = 0;
  virtual uint8_t connected() = 0;
  virtual void setTimeoutMs(uint16_t timeoutMs) = 0;

  int connect(IPAddress ip, uint16_t port);
  int connect(const char *host, uint16_t port);
  size_t write(uint8_t);
  size_t write(const uint8_t *buf, size_t size);

  size_t print(const char *);
  size_t println(const char *);
  size_t println();

  int read();
  int read(uint8_t *buf, size_t size);
  int read(char *buf, size_t size);

  // SSL configuration
  virtual void setSSLEnabled(bool enabled);
  void setCACert(const char *rootCA, bool destroyCertOnExit = false);

  void setDebugLogs(bool);
  void setSdc(SuplaDeviceClass *sdc);

 protected:
  virtual int connectImp(const char *host, uint16_t port) = 0;
  virtual size_t writeImp(const uint8_t *buf, size_t size) = 0;
  virtual int readImp(uint8_t *buf, size_t size) = 0;

  bool sslEnabled = false;
  bool debugLogs = false;
  bool destroyCertOnExit = false;
  const char *rootCACert = nullptr;
  unsigned int rootCACertSize = 0;
  SuplaDeviceClass *sdc = nullptr;
};

extern Client *ClientBuilder();
};  // namespace Supla

#endif  // SRC_SUPLA_NETWORK_CLIENT_H_

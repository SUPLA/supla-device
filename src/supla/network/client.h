// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_NETWORK_CLIENT_H_
#define SRC_SUPLA_NETWORK_CLIENT_H_

#include <stddef.h>
#include <stdint.h>

#include "connection_error.h"
#include "ip_address.h"

class SuplaDeviceClass;

namespace Supla {

#ifndef ARDUINO
#ifdef F
#undef F
#endif
#define F(argument_F) (argument_F)
#endif

class Client {
 public:
  Client();
  virtual ~Client();

  virtual int available() = 0;
  virtual void stop() = 0;
  virtual uint8_t connected() = 0;
  virtual void setTimeoutMs(uint16_t timeoutMs) = 0;
  virtual ConnectionError getConnectionError() const;

  int connect(IPAddress ip, uint16_t port);
  int connect(const char *host, uint16_t port);
  size_t write(uint8_t);
  size_t write(const uint8_t *buf, size_t size);
  size_t write(const void *buf, size_t size = 0);

  size_t print(const char *);
  size_t println(const char *);
#ifdef ARDUINO
  size_t print(const ::__FlashStringHelper *);
  size_t println(const ::__FlashStringHelper *);
#endif
  size_t println();

  int read();
  int read(uint8_t *buf, size_t size);
  int read(char *buf, size_t size);

  // SSL configuration
  virtual void setSSLEnabled(bool enabled);
  void setCACert(const char *rootCA);

  void setDebugLogs(bool);
  bool isDebugLogs() const;
  void setSdc(SuplaDeviceClass *sdc);

  uint32_t getSrcConnectionIPAddress() const;

 protected:
  virtual bool isCertificateValidationEnabled() const;
  virtual int connectImp(const char *host, uint16_t port) = 0;
  virtual size_t writeImp(const uint8_t *buf, size_t size) = 0;
  virtual int readImp(uint8_t *buf, size_t size) = 0;

  bool sslEnabled = false;
  bool debugLogs = false;
  const char *rootCACert = nullptr;
  unsigned int rootCACertSize = 0;
  SuplaDeviceClass *sdc = nullptr;
  uint32_t srcIp = 0;
};

extern Client *ClientBuilder();
};  // namespace Supla

#endif  // SRC_SUPLA_NETWORK_CLIENT_H_

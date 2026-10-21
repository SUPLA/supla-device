// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_NETWORK_ASYNC_CLIENT_H_
#define SRC_SUPLA_NETWORK_ASYNC_CLIENT_H_

#ifndef ARDUINO_ARCH_AVR
#include "client.h"

namespace Supla {
// Shared cooperative connection lifetime. Writes remain synchronous. Migrated
// transports derive from this class; legacy/AVR Client layouts are unchanged.
class AsyncClient : public Client {
 public:
  ConnectResult connectStep(const char *host, uint16_t port) override;
  ConnectionError getConnectionError() const override;
  void stop() override;
  uint8_t connected() override;
  void setTimeoutMs(uint16_t value) override;
  void setSSLEnabled(bool value) override;
  void setCACert(const char *value) override;

 protected:
  virtual ConnectResult startTransport(const char *host, uint16_t port) = 0;
  virtual ConnectResult pollTransport() = 0;
  virtual void closeTransport() = 0;
  virtual bool transportConnected() = 0;
  // >0 consumed bytes, -1 would block, 0 fatal/closed. No waiting for I/O.
  virtual int transmit(const uint8_t *data, size_t size) = 0;
  int connectImp(const char *host, uint16_t port) override;
  size_t writeImp(const uint8_t *data, size_t size) override;
  uint16_t timeoutMs = 10000;
  ConnectionError connectionError_ = ConnectionError::NONE;
  const char *connectionHost() const { return host_; }
  bool connectionReady() const { return state_ == ConnectResult::Connected; }

 private:
  char host_[256] = {};
  uint16_t port_ = 0;
  uint32_t startedMs_ = 0;
  ConnectResult state_ = ConnectResult::Failed;
};
}  // namespace Supla
#endif  // !ARDUINO_ARCH_AVR
#endif  // SRC_SUPLA_NETWORK_ASYNC_CLIENT_H_

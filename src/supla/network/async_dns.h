// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_NETWORK_ASYNC_DNS_H_
#define SRC_SUPLA_NETWORK_ASYNC_DNS_H_
#if defined(ARDUINO_ARCH_ESP8266) || defined(ARDUINO_ARCH_ESP32) || \
    defined(ESP_PLATFORM)
#include <stdint.h>
namespace Supla {
// lwIP callbacks reference bounded process-lifetime slots, never this object.
class AsyncDns {
 public:
  ~AsyncDns() { cancel(); }
  bool start(const char *host);
  // 0 pending, 1 IPv4 result, -1 failed.
  int poll(uint32_t *address);
  void cancel();
 private:
  int slot_ = -1;
};
}  // namespace Supla
#endif
#endif  // SRC_SUPLA_NETWORK_ASYNC_DNS_H_

// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#if defined(ARDUINO_ARCH_AVR)

#include <supla/log_wrapper.h>
#include <Ethernet.h>

#include "tools.h"
#include "supla/network/client.h"

namespace Supla {
class ArduinoMegaClient : public Client {
 public:
  int available() override {
    return arduinoClient.available();
  }

  void stop() override {
    arduinoClient.stop();
  }

  uint8_t connected() override {
    return  arduinoClient.connected();
  }

  void setTimeoutMs(uint16_t timeoutMs) override {
    (void)(timeoutMs);
// setConnectionTimeout is not available in both UIPEthenet and EthernetShield
// libraries.
//    arduinoClient.setConnectionTimeout(timeoutMs);
  }

 protected:
  int connectImp(const char *host, uint16_t port) override {
    if (sslEnabled) {
      SUPLA_LOG_WARNING("Warning: Arduino Mega network client does not support"
          " encrypted connection");
    }
    return arduinoClient.connect(host, port);
  }

  size_t writeImp(const uint8_t *buf, size_t size) override {
    return arduinoClient.write(buf, size);
  }

  int readImp(uint8_t *buf, size_t size) override {
    return arduinoClient.read(buf, size);
  }

  EthernetClient arduinoClient;
};
};  // namespace Supla

void deviceSoftwareReset() {
  // TODO(klew): implement software reset for Arduino IDE based targets
}

bool isDeviceSoftwareResetSupported() {
  return false;
}

bool isLastResetSoft() {
  // TODO(klew): implement
  return false;
}

bool Supla::isLastResetPower() {
  // TODO(klew): implement
  return false;
}

Supla::Client *Supla::ClientBuilder() {
  return new Supla::ArduinoMegaClient;
}

int Supla::getPlatformId() {
  return 0;
}

#endif

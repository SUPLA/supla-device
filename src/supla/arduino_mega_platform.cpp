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

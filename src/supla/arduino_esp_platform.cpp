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
#if defined(ARDUINO_ARCH_ESP8266) || defined(ARDUINO_ARCH_ESP32)

#include <supla/log_wrapper.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>

#include "tools.h"
#include "supla/network/client.h"

namespace Supla {
class ArduinoEspClient : public Client {
 public:
  int available() override {
    if (wifiClient) {
      return wifiClient->available();
    }
    return 0;
  }

  void stop() override {
    if (wifiClient) {
      wifiClient->stop();
      delete wifiClient;
      wifiClient = nullptr;
    }
  }

  uint8_t connected() override {
    return (wifiClient != nullptr) && wifiClient->connected();
  }

  void setTimeoutMs(uint16_t _timeoutMs) override {
    timeoutMs = _timeoutMs;
  }

  void setServersCertFingerprint(String value) {
    fingerprint = value;
  }

 protected:
  int connectImp(const char *host, uint16_t port) override {
    WiFiClientSecure *clientSec = nullptr;
#ifdef ARDUINO_ARCH_ESP8266
    X509List *caCert = nullptr;
#endif

    stop();

    if (sslEnabled) {
      clientSec = new WiFiClientSecure();
      wifiClient = clientSec;

      wifiClient->setTimeout(timeoutMs);
#ifdef ARDUINO_ARCH_ESP8266
      clientSec->setBufferSizes(1024, 512);  // EXPERIMENTAL
      if (rootCACert) {
        // Set time via NTP, as required for x.509 validation
        static bool timeConfigured = false;

        if (!timeConfigured) {
          configTime(0, 0, "pool.ntp.org", "time.nist.gov");
          SUPLA_LOG_DEBUG("Waiting for NTP time sync");
          time_t now = time(nullptr);
          while (now < 8 * 3600 * 2) {
            delay(100);
            now = time(nullptr);
          }
        }

        caCert = new BearSSL::X509List(rootCACert);
        clientSec->setTrustAnchors(caCert);
      } else if (fingerprint.length() > 0) {
        clientSec->setFingerprint(fingerprint.c_str());
      } else {
        clientSec->setInsecure();
      }
#else
      if (rootCACert) {
        clientSec->setCACert(rootCACert);
      } else {
        clientSec->setInsecure();
      }
#endif
    } else {
      wifiClient = new WiFiClient();
    }

    int result = wifiClient->connect(host, port);
    if (clientSec) {
      char buf[200];
      int lastErr = 0;
#ifdef ARDUINO_ARCH_ESP8266
      lastErr = clientSec->getLastSSLError(buf, sizeof(buf));
#elif defined(ARDUINO_ARCH_ESP32)
      lastErr = clientSec->lastError(buf, sizeof(buf));
#endif

      if (lastErr) {
        SUPLA_LOG_ERROR("SSL error: %d, %s", lastErr, buf);
      }
    }
#ifdef ARDUINO_ARCH_ESP8266
    if (caCert) {
      delete caCert;
      caCert = nullptr;
    }
#endif

    return result;
  }

  int readImp(uint8_t *buf, size_t count) override {
    if (wifiClient) {
      size_t size = wifiClient->available();

      if (size > 0) {
        if (size > count) {
          size = count;
        }
        return wifiClient->read(buf, size);
      }
    }
    return -1;
  }

  size_t writeImp(const uint8_t *buf, size_t count) override {
    if (wifiClient) {
      return wifiClient->write(buf, count);
    }
    return 0;
  }

  WiFiClient *wifiClient = nullptr;
  String fingerprint;
  uint16_t timeoutMs = 3000;
};
};  // namespace Supla


void deviceSoftwareReset() {
  ESP.restart();
}

bool isLastResetSoft() {
  // TODO(klew): implement
  return false;
}

Supla::Client *Supla::ClientBuilder() {
  return new Supla::ArduinoEspClient;
}

int Supla::getPlatformId() {
  // TODO(klew): do we need platfom id for Arduino based ESP SW?
  return 0;
}

#endif

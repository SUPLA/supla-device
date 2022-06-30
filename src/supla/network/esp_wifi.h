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

#ifndef SRC_SUPLA_NETWORK_ESP_WIFI_H_
#define SRC_SUPLA_NETWORK_ESP_WIFI_H_

#include <Arduino.h>

#include <supla/log_wrapper.h>

#ifdef ARDUINO_ARCH_ESP8266
#include <ESP8266WiFi.h>

// workaround for incompatible names in ESP8266 and ESP32 boards
#define WIFI_MODE_AP  WIFI_AP
#define WIFI_MODE_STA WIFI_STA

#else
#include <WiFi.h>
#endif

#include <WiFiClientSecure.h>

#include "../supla_lib_config.h"
#include "netif_wifi.h"

// TODO(klew): change logs to supla_log

namespace Supla {
class ESPWifi : public Supla::Wifi {
 public:
  ESPWifi(const char *wifiSsid = nullptr,
          const char *wifiPassword = nullptr,
          unsigned char *ip = nullptr)
      : Wifi(wifiSsid, wifiPassword, ip) {
  }

  int read(void *buf, int count) {
    if (client) {
      _supla_int_t size = client->available();

      if (size > 0) {
        if (size > count) size = count;
        int readSize = client->read(reinterpret_cast<uint8_t *>(buf), size);
#ifdef SUPLA_COMM_DEBUG
        printData("Recv", buf, readSize);
#endif

        return readSize;
      }
    }
    return -1;
  }

  int write(void *buf, int count) {
    if (client) {
#ifdef SUPLA_COMM_DEBUG
      printData("Send", buf, count);
#endif
      int sendSize =
          client->write(reinterpret_cast<const uint8_t *>(buf), count);
      return sendSize;
    }
    return 0;
  }

  int connect(const char *server, int port = -1) {
    String message;
    WiFiClientSecure *clientSec = nullptr;
#ifdef ARDUINO_ARCH_ESP8266
    X509List *caCert = nullptr;
#endif

    if (client) {
      client->stop();
      delete client;
      client = nullptr;
    }

    if (client == NULL) {
      if (sslEnabled) {
        message = "Secured connection";
        clientSec = new WiFiClientSecure();
        client = clientSec;

#ifdef ARDUINO_ARCH_ESP8266
        clientSec->setBufferSizes(1024, 512);  // EXPERIMENTAL
        if (rootCACert) {
          // Set time via NTP, as required for x.509 validation
          static bool timeConfigured = false;

          if (!timeConfigured) {
            configTime(3 * 3600, 0, "pool.ntp.org", "time.nist.gov");
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
          message += " with certificate matching";
          clientSec->setFingerprint(fingerprint.c_str());
        } else {
          message += " without certificate matching";
          clientSec->setInsecure();
        }
#else
        if (rootCACert) {
          // TODO(klew): add CA cert verification for ESP32
        } else {
          clientSec->setInsecure();
        }
#endif
      } else {
        message = "unsecured connection";
        client = new WiFiClient();
      }
    }

    int connectionPort = (sslEnabled ? 2016 : 2015);
    if (port != -1) {
      connectionPort = port;
    }

    SUPLA_LOG_DEBUG(
        "Establishing %s with: %s (port: %d)",
        message.c_str(),
        server,
        connectionPort);

    bool result = client->connect(server, connectionPort);
#ifdef ARDUINO_ARCH_ESP8266
    if (clientSec) {
      char buf[200];
      int lastErr = clientSec->getLastSSLError(buf, sizeof(buf));
      if (lastErr) {
        SUPLA_LOG_ERROR("SSL error: %d, %s", lastErr, buf);
      }
    }
    if (caCert) {
      delete caCert;
      caCert = nullptr;
    }
#endif

    return result;
  }

  bool connected() {
    return (client != NULL) && client->connected();
  }

  bool isReady() {
    return WiFi.status() == WL_CONNECTED;
  }

  void disconnect() {
    if (client != nullptr) {
      client->stop();
    }
  }

  // TODO(klew): add handling of custom local ip
  void setup() {
    if (!wifiConfigured) {
      // ESP32 requires setHostname to be called before begin...
      WiFi.setHostname(hostname);
      wifiConfigured = true;
#ifdef ARDUINO_ARCH_ESP8266
      gotIpEventHandler =
        WiFi.onStationModeGotIP([](const WiFiEventStationModeGotIP &event) {
            (void)(event);
            Serial.print(F("local IP: "));
            Serial.println(WiFi.localIP());
            Serial.print(F("subnetMask: "));
            Serial.println(WiFi.subnetMask());
            Serial.print(F("gatewayIP: "));
            Serial.println(WiFi.gatewayIP());
            int rssi = WiFi.RSSI();
            Serial.print(F("Signal strength (RSSI): "));
            Serial.print(rssi);
            Serial.println(F(" dBm"));
            });
      disconnectedEventHandler = WiFi.onStationModeDisconnected(
          [](const WiFiEventStationModeDisconnected &event) {
          (void)(event);
          Serial.println(F("WiFi station disconnected"));
          });
#else
      WiFiEventId_t event_gotIP = WiFi.onEvent(
          [](WiFiEvent_t event, WiFiEventInfo_t info) {
          Serial.print(F("local IP: "));
          Serial.println(WiFi.localIP());
          Serial.print(F("subnetMask: "));
          Serial.println(WiFi.subnetMask());
          Serial.print(F("gatewayIP: "));
          Serial.println(WiFi.gatewayIP());
          int rssi = WiFi.RSSI();
          Serial.print(F("Signal Strength (RSSI): "));
          Serial.print(rssi);
          Serial.println(F(" dBm"));
          },
          WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_GOT_IP);

      (void)(event_gotIP);

      WiFiEventId_t event_disconnected = WiFi.onEvent(
          [](WiFiEvent_t event, WiFiEventInfo_t info) {
          Serial.println(F("WiFi Station disconnected"));
            // ESP32 doesn't reconnect automatically after lost connection
            WiFi.reconnect();
          },
          WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

      (void)(event_disconnected);
#endif
    } else {
      if (mode == Supla::DEVICE_MODE_CONFIG) {
        WiFi.mode(WIFI_MODE_AP);
      } else {
        WiFi.mode(WIFI_MODE_STA);
      }
      Serial.println(F("WiFi: resetting WiFi connection"));
      if (client) {
        delete client;
        client = nullptr;
      }
      WiFi.disconnect();
    }

    if (mode == Supla::DEVICE_MODE_CONFIG) {
      Serial.print("WiFi: enter config mode with SSID: ");
      Serial.println(hostname);
      WiFi.mode(WIFI_MODE_AP);
      WiFi.softAP(hostname, nullptr, 6);

    } else {
      Serial.print(F("WiFi: establishing connection with SSID: \""));
      Serial.print(ssid);
      Serial.println(F("\""));
      WiFi.mode(WIFI_MODE_STA);
      WiFi.begin(ssid, password);
      // ESP8266 requires setHostname to be called after begin...
      WiFi.setHostname(hostname);
    }

    yield();
  }

  // DEPRECATED, use setSSLEnabled instead
  void enableSSL(bool value) {
    setSSLEnabled(value);
  }

  void setServersCertFingerprint(String value) {
    fingerprint = value;
  }

  void setTimeout(int timeoutMs) {
    if (client) {
      client->setTimeout(timeoutMs);
    }
  }

  void fillStateData(TDSC_ChannelState *channelState) {
    channelState->Fields |= SUPLA_CHANNELSTATE_FIELD_IPV4 |
                            SUPLA_CHANNELSTATE_FIELD_MAC |
                            SUPLA_CHANNELSTATE_FIELD_WIFIRSSI |
                            SUPLA_CHANNELSTATE_FIELD_WIFISIGNALSTRENGTH;
    channelState->IPv4 = WiFi.localIP();
    WiFi.macAddress(channelState->MAC);
    int rssi = WiFi.RSSI();
    channelState->WiFiRSSI = rssi;
    if (rssi > -50) {
      channelState->WiFiSignalStrength = 100;
    } else if (rssi <= -100) {
      channelState->WiFiSignalStrength = 0;
    } else {
      channelState->WiFiSignalStrength = 2 * (rssi + 100);
    }
  }

  bool getMacAddr(uint8_t *out) override {
    WiFi.macAddress(out);
    return true;
  }

  void uninit() override {
    WiFi.softAPdisconnect(true);
    WiFi.disconnect(true);
  }

 protected:
  WiFiClient *client = nullptr;
  bool wifiConfigured = false;
  String fingerprint;

#ifdef ARDUINO_ARCH_ESP8266
  WiFiEventHandler gotIpEventHandler, disconnectedEventHandler;
#endif
};

};  // namespace Supla

#endif  // SRC_SUPLA_NETWORK_ESP_WIFI_H_

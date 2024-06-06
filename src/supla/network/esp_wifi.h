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
#include <supla/tools.h>

#ifdef ARDUINO_ARCH_ESP8266
#include <ESP8266WiFi.h>

// workaround for incompatible names in ESP8266 and ESP32 boards
#define WIFI_MODE_AP  WIFI_AP
#define WIFI_MODE_STA WIFI_STA

#else
#include <WiFi.h>
#endif

#include "../supla_lib_config.h"
#include "netif_wifi.h"

namespace Supla {
class ESPWifi : public Supla::Wifi {
 public:
  ESPWifi(const char *wifiSsid = nullptr,
          const char *wifiPassword = nullptr,
          unsigned char *ip = nullptr)
      : Wifi(wifiSsid, wifiPassword, ip) {
  }


  bool isReady() override {
    return WiFi.status() == WL_CONNECTED;
  }

  static void printConnectionDetails() {
    SUPLA_LOG_INFO("Connected BSSID: %s", WiFi.BSSIDstr().c_str());
    SUPLA_LOG_INFO("local IP: %s", WiFi.localIP().toString().c_str());
    SUPLA_LOG_INFO("subnetMask: %s", WiFi.subnetMask().toString().c_str());
    SUPLA_LOG_INFO("gatewayIP: %s", WiFi.gatewayIP().toString().c_str());
    int rssi = WiFi.RSSI();
    SUPLA_LOG_INFO("Signal strength (RSSI): %d dBm", rssi);
  }

  // TODO(klew): add handling of custom local ip
  void setup() override {
    // ESP8266: for some reason when hostname is longer than 30 bytes, Wi-Fi
    // connection can't be esablished. So as a workaround, we truncate hostname
    // to 30 bytes
    hostname[30] = '\0';
    if (!wifiConfigured) {
      // ESP32 requires setHostname to be called before begin...
      WiFi.setHostname(hostname);
      wifiConfigured = true;
#ifdef ARDUINO_ARCH_ESP8266
      gotIpEventHandler =
        WiFi.onStationModeGotIP([](const WiFiEventStationModeGotIP &event) {
            (void)(event);
            ESPWifi::printConnectionDetails();
            });
      disconnectedEventHandler = WiFi.onStationModeDisconnected(
          [](const WiFiEventStationModeDisconnected &event) {
          (void)(event);
          SUPLA_LOG_INFO("WiFi station disconnected");
          });
#else
      WiFiEventId_t event_gotIP = WiFi.onEvent(
          [](WiFiEvent_t event, WiFiEventInfo_t info) {
          ESPWifi::printConnectionDetails();
          },
          WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_GOT_IP);

      (void)(event_gotIP);

      WiFiEventId_t event_disconnected = WiFi.onEvent(
          [](WiFiEvent_t event, WiFiEventInfo_t info) {
          SUPLA_LOG_INFO("WiFi Station disconnected");
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
      SUPLA_LOG_INFO("WiFi: resetting WiFi connection");
      DisconnectProtocols();
      WiFi.disconnect();
    }

    if (mode == Supla::DEVICE_MODE_CONFIG) {
      SUPLA_LOG_INFO("WiFi: enter config mode with SSID: \"%s\"", hostname);
      WiFi.mode(WIFI_MODE_AP);
      WiFi.softAP(hostname, nullptr, 6);

    } else {
      SUPLA_LOG_INFO("WiFi: establishing connection with SSID: \"%s\"", ssid);
      WiFi.mode(WIFI_MODE_STA);
      WiFi.begin(ssid, password);
      // ESP8266 requires setHostname to be called after begin...
      WiFi.setHostname(hostname);
    }

    yield();
  }

  void disable() override {
  }

  // DEPRECATED, use setSSLEnabled instead
  void enableSSL(bool value) {
    setSSLEnabled(value);
  }

  void fillStateData(TDSC_ChannelState *channelState) override {
    channelState->Fields |= SUPLA_CHANNELSTATE_FIELD_IPV4 |
                            SUPLA_CHANNELSTATE_FIELD_MAC |
                            SUPLA_CHANNELSTATE_FIELD_WIFIRSSI |
                            SUPLA_CHANNELSTATE_FIELD_WIFISIGNALSTRENGTH;
    channelState->IPv4 = WiFi.localIP();
    getMacAddr(channelState->MAC);
    int rssi = WiFi.RSSI();
    channelState->WiFiRSSI = rssi;
    channelState->WiFiSignalStrength = Supla::rssiToSignalStrength(rssi);
  }

  bool getMacAddr(uint8_t *out) override {
#ifdef ARDUINO_ARCH_ESP8266
    WiFi.macAddress(out);
#else
#ifdef ESP_ARDUINO_VERSION_MAJOR
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
    // Code for version 3.x
    ::Network.macAddress(out);
#else
    // Code for version 2.x
    WiFi.macAddress(out);
#endif
#endif
#endif
    return true;
  }

  void uninit() override {
    WiFi.softAPdisconnect(true);
    WiFi.disconnect(true);
  }

 protected:
  bool wifiConfigured = false;

#ifdef ARDUINO_ARCH_ESP8266
  WiFiEventHandler gotIpEventHandler, disconnectedEventHandler;
#endif
};

};  // namespace Supla

#endif  // SRC_SUPLA_NETWORK_ESP_WIFI_H_

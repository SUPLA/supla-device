// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_NETWORK_ESP_WIFI_H_
#define SRC_SUPLA_NETWORK_ESP_WIFI_H_

#include <Arduino.h>

#include <supla/input_noise_guard.h>
#include <supla/log_wrapper.h>
#include <supla/network/arduino_netif_config.h>
#include <supla/network/wifi_scan_result.h>
#include <supla/time.h>
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
          Supla::InputNoiseGuard::NotifyWifiStaConnected();
          ESPWifi::printConnectionDetails();
          },
          WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_GOT_IP);

      (void)(event_gotIP);

      WiFiEventId_t event_disconnected = WiFi.onEvent(
          [](WiFiEvent_t event, WiFiEventInfo_t info) {
            SUPLA_LOG_INFO("WiFi Station disconnected");
            // ESP32 doesn't reconnect automatically after lost connection
            Supla::InputNoiseGuard::NotifyWifiStaDisconnected();
            if (mode != Supla::DEVICE_MODE_CONFIG) {
              WiFi.reconnect();
            }
          },
          WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

      (void)(event_disconnected);
#endif
    } else {
      if (mode == Supla::DEVICE_MODE_CONFIG) {
        Supla::InputNoiseGuard::NotifyWifiTransition();
        WiFi.mode(WIFI_MODE_AP);
      } else {
        Supla::InputNoiseGuard::NotifyWifiTransition();
        WiFi.mode(WIFI_MODE_STA);
      }
      SUPLA_LOG_INFO("WiFi: resetting WiFi connection");
      DisconnectProtocols();
      Supla::InputNoiseGuard::NotifyWifiTransition();
      WiFi.disconnect();
    }

    if (mode == Supla::DEVICE_MODE_CONFIG) {
      SUPLA_LOG_INFO("WiFi: enter config mode with SSID: \"%s\"", hostname);
      Supla::InputNoiseGuard::NotifyWifiTransition();
      WiFi.mode(WIFI_AP_STA);
      Supla::InputNoiseGuard::NotifyWifiTransition();
      WiFi.softAP(hostname, nullptr, 6);
      startConfigModeScan();

    } else {
      SUPLA_LOG_INFO("WiFi: establishing connection with SSID: \"%s\"", ssid);
      Supla::InputNoiseGuard::NotifyWifiTransition();
      WiFi.mode(WIFI_MODE_STA);
      if (hasStaticIpConfig()) {
        const auto &cfg = getNetifConfig();
        IPAddress localIp = toArduinoIpAddress(cfg.ip);
        IPAddress gateway = toArduinoIpAddress(cfg.gateway);
        IPAddress subnet = toArduinoIpAddress(cfg.netmask);
        IPAddress dns1 = toArduinoIpAddress(cfg.dns1);
        IPAddress dns2 = toArduinoIpAddress(cfg.dns2);
        if (!WiFi.config(localIp, gateway, subnet, dns1, dns2)) {
          SUPLA_LOG_WARNING("WiFi static IP config failed, continuing");
        }
      }
      Supla::InputNoiseGuard::NotifyWifiTransition();
      WiFi.begin(ssid, password);
      // ESP8266 requires setHostname to be called after begin...
      WiFi.setHostname(hostname);
    }

    yield();
    allowDisable = true;
  }

  bool iterate() override {
    if (!configModeScanInProgress) {
      return Supla::Wifi::iterate();
    }

    int scanResult = WiFi.scanComplete();
    if (scanResult == WIFI_SCAN_RUNNING) {
      return false;
    }

    configModeScanInProgress = false;
    auto cache = Supla::WifiScanResultCache::Instance();
    cache->beginUpdate();

    if (scanResult >= 0) {
      for (int i = 0; i < scanResult; i++) {
        cache->addOrUpdate(WiFi.SSID(i).c_str(), WiFi.RSSI(i), WiFi.channel(i));
      }
      cache->finishUpdate(millis());
      SUPLA_LOG_INFO("WiFi: config mode scan completed (%d networks)",
                     scanResult);
    } else {
      cache->clear();
      SUPLA_LOG_WARNING("WiFi: config mode scan failed (%d)", scanResult);
    }

    WiFi.scanDelete();
    return Supla::Wifi::iterate();
  }

  void startConfigModeScan() override {
    if (mode != Supla::DEVICE_MODE_CONFIG || configModeScanInProgress) {
      return;
    }

    WiFi.scanDelete();
    int scanResult = WiFi.scanNetworks(true, true);
    configModeScanInProgress = (scanResult == WIFI_SCAN_RUNNING);
    if (!configModeScanInProgress && scanResult >= 0) {
      auto cache = Supla::WifiScanResultCache::Instance();
      cache->beginUpdate();
      for (int i = 0; i < scanResult; i++) {
        cache->addOrUpdate(WiFi.SSID(i).c_str(), WiFi.RSSI(i), WiFi.channel(i));
      }
      cache->finishUpdate(millis());
      WiFi.scanDelete();
      SUPLA_LOG_INFO("WiFi: config mode scan completed (%d networks)",
                     scanResult);
    } else if (!configModeScanInProgress) {
      SUPLA_LOG_WARNING("WiFi: config mode scan start failed (%d)",
                        scanResult);
    } else {
      SUPLA_LOG_INFO("WiFi: config mode scan started");
    }
  }

  void disable() override {
    if (!allowDisable) {
      return;
    }

    allowDisable = false;
    configModeScanInProgress = false;
    SUPLA_LOG_DEBUG("[%s] disabling WiFi connection", getIntfName());
    DisconnectProtocols();
    WiFi.scanDelete();
    Supla::InputNoiseGuard::NotifyWifiTransition();
    WiFi.softAPdisconnect(true);
    Supla::InputNoiseGuard::NotifyWifiTransition();
    WiFi.disconnect(true);
    Supla::InputNoiseGuard::NotifyWifiTransition();
    WiFi.mode(WIFI_OFF);
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
    disable();
  }

  uint32_t getIP() override {
    return WiFi.localIP();
  }

  const char *getIntfName() const override {
    return "Wi-Fi";
  }

 protected:
  bool wifiConfigured = false;
  bool configModeScanInProgress = false;
  bool allowDisable = false;

#ifdef ARDUINO_ARCH_ESP8266
  WiFiEventHandler gotIpEventHandler, disconnectedEventHandler;
#endif

  bool isConfigModeScanInProgress() const override {
    return configModeScanInProgress;
  }
};

};  // namespace Supla

#endif  // SRC_SUPLA_NETWORK_ESP_WIFI_H_

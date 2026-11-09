// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "netif_wifi.h"

#include <string.h>
#include <stdio.h>
#include <supla/auto_lock.h>
#include <supla/storage/config_tags.h>
#include <supla/storage/config.h>
#include <supla/storage/storage.h>
#include <supla/network/wifi_scan_result.h>
#include <supla/time.h>

using Supla::Wifi;

Wifi::Wifi(const char *wifiSsid, const char *wifiPassword, unsigned char *ip)
  : Network(ip) {
    setSsid(wifiSsid);
    setPassword(wifiPassword);
    intfType = IntfType::WiFi;
  }

void Wifi::setSsid(const char *wifiSsid) {
  if (wifiSsid) {
    strncpy(ssid, wifiSsid, MAX_SSID_SIZE - 1);
    ssid[MAX_SSID_SIZE - 1] = '\0';
  }
}

void Wifi::setPassword(const char *wifiPassword) {
  if (wifiPassword) {
    strncpy(password, wifiPassword, MAX_WIFI_PASSWORD_SIZE - 1);
    password[MAX_WIFI_PASSWORD_SIZE - 1] = '\0';
  }
}

bool Wifi::isWifiConfigRequired() {
  return true;
}

void Wifi::startConfigModeScan() {
}

bool Wifi::isConfigModeScanInProgress() const {
  return false;
}

void Wifi::requestConfigModeScanIfDue() {
  if (mode != Supla::DEVICE_MODE_CONFIG || isConfigModeScanInProgress()) {
    return;
  }

  uint32_t now = millis();
  auto cache = Supla::WifiScanResultCache::Instance();
  bool scanDue = true;
  if (cache != nullptr && cache->hasScan()) {
    scanDue = now - cache->getTimestampMs() >= WifiScanRefreshIntervalMs;
  }

  if (!scanDue) {
    return;
  }

  if (configModeScanStartRecorded &&
      now - lastConfigModeScanStartMs < WifiScanRefreshIntervalMs) {
    return;
  }

  configModeScanStartRecorded = true;
  lastConfigModeScanStartMs = now;
  startConfigModeScan();
}

bool Wifi::iterate() {
  requestConfigModeScanIfDue();
  return Supla::Network::iterate();
}

void Wifi::onLoadConfig() {
  Network::onLoadConfig();
  auto cfg = Supla::Storage::ConfigInstance();
  if (!cfg) {
    return;
  }
  cfg->loadNetifConfig(Supla::ConfigTag::WifiNetifCfgTag, &netifConfig);
  char buf[100] = {};
  memset(buf, 0, sizeof(buf));
  if (cfg->getWiFiSSID(buf) && strlen(buf) > 0) {
    setSsid(buf);
  }

  memset(buf, 0, sizeof(buf));
  if (cfg->getWiFiPassword(buf) && strlen(buf) > 0) {
    setPassword(buf);
  }
}

const char* Wifi::getIntfName() const {
  return "Wi-Fi";
}

void Wifi::setWifiConnected(bool connected) {
  // Apply the event and reset diagnostics atomically with respect to shutdown.
  Supla::AutoLock lock(stateMutex);
  if (!applyLinkConnectedLocked(connected)) {
    return;
  }
  if (connected) {
    memset(lastReasons, 0, sizeof(lastReasons));
    lastReasonIdx = 0;
    wifiDisconnectExpected = false;
  }
}

void Wifi::syncWifiState(bool connected, uint32_t ip) {
  if (!connected || ip == 0) {
    return;
  }
  // Do not consume the expected disconnect: its callback can still be queued.
  setLinkConnected(true, nullptr);
  setIpv4Addr(ip);
}

void Wifi::expectWifiDisconnect() {
  Supla::AutoLock lock(stateMutex);
  wifiDisconnectExpected = true;
}

bool Wifi::handleWifiDisconnect(int reason) {
  setWifiConnected(false);
  bool expected = false;
  {
    Supla::AutoLock lock(stateMutex);
    // ASSOC_LEAVE (8) is emitted for a local station disconnect by both cores.
    expected = wifiDisconnectExpected && reason == 8;
    wifiDisconnectExpected = false;
  }
  if (!expected) {
    logWifiReason(reason);
  }
  return !expected;
}

void Supla::Wifi::logWifiReason(int reason) {
  if (!isStateLoggingAllowed()) {
    return;
  }
  bool reasonAlreadyReported = false;
  {
    Supla::AutoLock lock(stateMutex);
    for (int i = 0; i < LastReasonCount; i++) {
      if (lastReasons[i] == reason) {
        reasonAlreadyReported = true;
        break;
      }
    }

    if (lastReasonIdx >= LastReasonCount) {
      lastReasonIdx = 0;
    }
    lastReasons[lastReasonIdx++] = reason;
  }

  if (!reasonAlreadyReported) {
    switch (reason) {
      case 1: {
        logLastState("disconnect (unspecified)");
        break;
      }
      case 2: {
        logLastState("auth expire (incorrect password)");
        break;
      }
      case 3: {
        logLastState("auth leave");
        break;
      }
      case 8: {
        logLastState("disconnecting from AP");
        break;
      }
      case 15: {
        logLastState(
            "4way handshake timeout (incorrect password)");
        break;
      }
      case 200: {
        logLastState("beacon timeout");
        break;
      }
      case 201: {
        char buf[100] = {};
        snprintf(buf, sizeof(buf), "\"%s\" not found", ssid);
        logLastState(buf);
        break;
      }
      case 202: {
        logLastState("auth fail");
        break;
      }
      case 203: {
        logLastState("assoc fail");
        break;
      }
      case 204: {
        logLastState("handshake timeout (incorrect password)");
        break;
      }
      case 205: {
        logLastState("connection fail");
        break;
      }
      case 206: {
        logLastState("ap tsf reset");
        break;
      }
      case 207: {
        logLastState("roaming");
        break;
      }
      case 210: {
        logLastState(
            "SSID found but with incompatible security");
        break;
      }
      case 211: {
        logLastState(
            "AP found but with incompatible authmode (at least WPA2 PSK "
            "is required)");
        break;
      }
      case 212: {
        logLastState("AP found but RSSI is too low");
        break;
      }
      default: {
        char buf[100] = {};
        // see:
        // https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/wifi.html#wi-fi-reason-code
        snprintf(buf, sizeof(buf), "disconnect reason %d", reason);
        logLastState(buf);
        break;
      }
    }
  }
}

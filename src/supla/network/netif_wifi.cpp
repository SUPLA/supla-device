// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "netif_wifi.h"

#include <string.h>
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

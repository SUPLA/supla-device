// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_NETWORK_NETIF_WIFI_H_
#define SRC_SUPLA_NETWORK_NETIF_WIFI_H_

#include "network.h"

#define MAX_SSID_SIZE          33
#define MAX_WIFI_PASSWORD_SIZE 64

namespace Supla {

constexpr uint32_t WifiScanRefreshIntervalMs = 60UL * 1000;

class Wifi : public Supla::Network {
 public:
  Wifi(const char *wifiSsid = nullptr,
       const char *wifiPassword = nullptr,
       unsigned char *ip = nullptr);

  void setSsid(const char *wifiSsid) override;
  void setPassword(const char *wifiPassword) override;
  bool isWifiConfigRequired() override;
  const char* getIntfName() const override;
  virtual void startConfigModeScan();
  bool iterate() override;

  void onLoadConfig() override;
  void setWifiConnected(bool connected);
  void logWifiReason(int reason);
  // Recover a connection established before event handlers were registered.
  void syncWifiState(bool connected, uint32_t ip);
  // A manual disconnect may be delivered after the next setup has begun.
  void expectWifiDisconnect();
  // Returns false for an expected local disconnect (no log or reconnect).
  bool handleWifiDisconnect(int reason);

 protected:
  static constexpr int LastReasonCount = 2;
  int lastReasons[LastReasonCount] = {};
  int lastReasonIdx = 0;
  bool wifiDisconnectExpected = false;
  void requestConfigModeScanIfDue();
  virtual bool isConfigModeScanInProgress() const;
  char ssid[MAX_SSID_SIZE] = {};
  char password[MAX_WIFI_PASSWORD_SIZE] = {};
  uint32_t lastConfigModeScanStartMs = 0;
  bool configModeScanStartRecorded = false;
};

};  // namespace Supla

#endif  // SRC_SUPLA_NETWORK_NETIF_WIFI_H_

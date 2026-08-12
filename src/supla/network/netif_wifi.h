// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_NETWORK_NETIF_WIFI_H_
#define SRC_SUPLA_NETWORK_NETIF_WIFI_H_

#include "network.h"

#define MAX_SSID_SIZE          33
#define MAX_WIFI_PASSWORD_SIZE 64

namespace Supla {

constexpr uint32_t WifiScanRefreshIntervalMs = 60 * 1000;

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

 protected:
  void requestConfigModeScanIfDue();
  virtual bool isConfigModeScanInProgress() const;
  char ssid[MAX_SSID_SIZE] = {};
  char password[MAX_WIFI_PASSWORD_SIZE] = {};
  uint32_t lastConfigModeScanStartMs = 0;
  bool configModeScanStartRecorded = false;
};

};  // namespace Supla

#endif  // SRC_SUPLA_NETWORK_NETIF_WIFI_H_

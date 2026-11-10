// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_PORTING_ESP_IDF_ESP_IDF_WIFI_H_
#define EXTRAS_PORTING_ESP_IDF_ESP_IDF_WIFI_H_

// supla-device includes
#include <supla/network/netif_wifi.h>

#include <esp_wifi.h>

namespace Supla {

class EspIdfWifi : public Supla::Wifi {
 public:
  explicit EspIdfWifi(const char *wifiSsid = nullptr,
             const char *wifiPassword = nullptr,
             unsigned char *ip = nullptr);
  virtual ~EspIdfWifi();

  void setup() override;
  void disable() override;
  void uninit() override;
  bool getMacAddr(uint8_t *out) override;

  void fillStateData(TDSC_ChannelState *channelState) override;

  void setLastDisconnectReason(int reason);
  bool isAccessPointConnected() const;
  int getLastDisconnectReason() const;
  bool isIpSetupTimeout() override;

  bool isInConfigMode();
  void addSecurityLog(uint32_t, const char *) const;

  void setMaxTxPower(int power);
  uint32_t getConfiguredStaticIp() const;
  bool isStaticIpConfigured() const;
  void startConfigModeScan() override;
  void finishConfigModeScan();

  esp_netif_t *getStaNetIf() const;

 protected:
  bool initDone = false;
  uint8_t lastChannel = 0;
  int lastDisconnectReason = 0;
  int maxTxPower = -1;
  bool configModeScanInProgress = false;
  bool isConfigModeScanInProgress() const override;
  esp_netif_t *staNetIf = nullptr;
  esp_netif_t *apNetIf = nullptr;
};

};  // namespace Supla

#endif  // EXTRAS_PORTING_ESP_IDF_ESP_IDF_WIFI_H_

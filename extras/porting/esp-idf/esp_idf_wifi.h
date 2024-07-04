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

#ifndef EXTRAS_PORTING_ESP_IDF_ESP_IDF_WIFI_H_
#define EXTRAS_PORTING_ESP_IDF_ESP_IDF_WIFI_H_

// supla-device includes
#include <supla/network/netif_wifi.h>

#include <esp_wifi.h>

namespace Supla {

#define SUPLA_ESP_IDF_WIFI_LAST_REASON_MAX 2

class EspIdfWifi : public Supla::Wifi {
 public:
  explicit EspIdfWifi(const char *wifiSsid = nullptr,
             const char *wifiPassword = nullptr,
             unsigned char *ip = nullptr);
  virtual ~EspIdfWifi();

  bool isReady() override;
  void setup() override;
  void disable() override;
  void uninit() override;
  bool getMacAddr(uint8_t *out) override;

  void fillStateData(TDSC_ChannelState *channelState) override;

  void setIpReady(bool ready);
  void setIpv4Addr(uint32_t ip);
  void setWifiConnected(bool state);
  bool isIpSetupTimeout() override;

  bool isInConfigMode();
  void logWifiReason(int);

  uint32_t getIP() override;
  void setMaxTxPower(int power);

 protected:
  bool initDone = false;
  bool isWifiConnected = false;
  bool isIpReady = false;
  bool allowDisable = false;
  uint32_t ipv4 = 0;
  uint8_t lastChannel = 0;
  int lastReasons[SUPLA_ESP_IDF_WIFI_LAST_REASON_MAX] = {};
  int lastReasonIdx = 0;
  uint32_t connectedToWifiTimestamp = 0;
  int maxTxPower = -1;
#ifdef SUPLA_DEVICE_ESP32
  esp_netif_t *staNetIf = nullptr;
  esp_netif_t *apNetIf = nullptr;
#endif
};

};  // namespace Supla

#endif  // EXTRAS_PORTING_ESP_IDF_ESP_IDF_WIFI_H_

// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

// LAN8720 ESP-IDF implementation for ESP32

#ifndef EXTRAS_ESP_IDF_SUPLA_LAN8720_ESP_IDF_LAN8720_H_
#define EXTRAS_ESP_IDF_SUPLA_LAN8720_ESP_IDF_LAN8720_H_

// supla-device includes
#include <supla/network/netif_lan.h>

#include <esp_netif_types.h>
#include <esp_eth.h>

namespace Supla {

class EspIdfLan8720 : public Supla::LAN {
 public:
  EspIdfLan8720(int mdcGpio, int mdioGpio);
  virtual ~EspIdfLan8720();

  void setup() override;
  void disable() override;
  void uninit() override;
  bool getMacAddr(uint8_t *out) override;

  bool isIpSetupTimeout() override;

  SuplaDeviceClass *getSdc();
  bool isStateLoggingAllowed();
  bool isStaticIpConfigured() const;
  void setEthStarted(bool started);

 protected:
  int mdcGpio = -1;
  int mdioGpio = -1;

  esp_netif_t *netIf = nullptr;
  esp_eth_handle_t ethHandle = NULL;
  esp_eth_netif_glue_handle_t ethGlue = NULL;

  bool initDone = false;
  bool ethStarted = false;
  bool allowDisable = false;
  bool staticIpConfigured = false;
};

};  // namespace Supla

#endif  // EXTRAS_ESP_IDF_SUPLA_LAN8720_ESP_IDF_LAN8720_H_

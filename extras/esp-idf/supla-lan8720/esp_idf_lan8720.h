/*
   Copyright (C) AC SOFTWARE SP. Z O.O

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

 protected:
  int mdcGpio = -1;
  int mdioGpio = -1;

  esp_netif_t *netIf = nullptr;
  esp_eth_handle_t ethHandle = NULL;
  esp_eth_netif_glue_handle_t ethGlue = NULL;

  bool initDone = false;
  bool allowDisable = false;
};

};  // namespace Supla

#endif  // EXTRAS_ESP_IDF_SUPLA_LAN8720_ESP_IDF_LAN8720_H_

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
#include <supla/network/network.h>

#include <esp_netif_types.h>
#include <esp_eth.h>

namespace Supla {

class EspIdfLan8720 : public Supla::Network {
 public:
  EspIdfLan8720(int mdcGpio, int mdioGpio);
  virtual ~EspIdfLan8720();

  bool isReady() override;
  void setup() override;
  void disable() override;
  void uninit() override;
  bool getMacAddr(uint8_t *out) override;
  uint32_t getIP() override;

  void fillStateData(TDSC_ChannelState *channelState) override;

  void setIpReady(bool ready);
  void setIpv4Addr(uint32_t ip);
  bool isIpSetupTimeout() override;

  const char* getIntfName() const override;

 protected:
  int mdcGpio = -1;
  int mdioGpio = -1;

  bool initDone = false;
  bool isIpReady = false;
  bool allowDisable = false;

  uint32_t ipv4 = 0;

  esp_netif_t *netIf = nullptr;
  esp_eth_handle_t ethHandle = NULL;
};

};  // namespace Supla

#endif  // EXTRAS_ESP_IDF_SUPLA_LAN8720_ESP_IDF_LAN8720_H_

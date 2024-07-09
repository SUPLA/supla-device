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

#ifndef SRC_SUPLA_NETWORK_ESP32ETHSPI_H_
#define SRC_SUPLA_NETWORK_ESP32ETHSPI_H_

#include <Arduino.h>
#include <ETH.h>
#include <supla/network/netif_lan.h>
#include <supla/supla_lib_config.h>
#include <supla/log_wrapper.h>

namespace Supla {
class ESPETHSPI;
}  // namespace Supla

static Supla::ESPETHSPI *thisSpiEth = nullptr;

namespace Supla {
class ESPETHSPI : public Supla::LAN {
  public:
    explicit ESPETHSPI( eth_phy_type_t type, int32_t phy_addr, int cs, int irq, int rst, spi_host_device_t spi_host, int sck = -1, int miso = -1, int mosi = -1) {
      thisSpiEth = this;
      ethspi_type = type;
      ethspi_phy_addr = phy_addr;
      cs_pin = cs;
      irq_pin = irq;
      rst_pin = rst;
      ethspi_spi_host = spi_host;
      sck_pin = sck;
      miso_pin = miso;
      mosi_pin = mosi;
    }

    ~ESPETHSPI() {
      if (thisSpiEth == this) {
        thisSpiEth = nullptr;
      }
    }

    static void networkEventHandler(arduino_event_id_t event) {
      switch (event) {
        case ARDUINO_EVENT_ETH_GOT_IP: {
            Serial.print(F("[Ethernet] local IP: "));
            Serial.println(ETH.localIP());
            Serial.print(F("subnetMask: "));
            Serial.println(ETH.subnetMask());
            Serial.print(F("gatewayIP: "));
            Serial.println(ETH.gatewayIP());
            Serial.print(F("ETH MAC: "));
            Serial.println(ETH.macAddress());
            if (ETH.fullDuplex()) {
              Serial.print(F("FULL_DUPLEX , "));
            }
            Serial.print(ETH.linkSpeed());
            Serial.println(F("Mbps"));
            if (thisSpiEth) {
              thisSpiEth->setIpv4Addr(ETH.localIP());
            }
            break;
          }
        case ARDUINO_EVENT_ETH_DISCONNECTED: {
            Serial.println(F("[Ethernet] Disconnected"));
            if (thisSpiEth) {
              thisSpiEth->setIpv4Addr(0);
            }
            break;
          }
      }
    }

    void setup() override {
      allowDisable = true;
      if (initDone) {
        return;
      }

      ::Network.onEvent(Supla::ESPETHSPI::networkEventHandler);

      Serial.println(F("[Ethernet] establishing LAN connection"));
      ETH.begin(ethspi_type,
                ethspi_phy_addr,
                cs_pin,
                irq_pin,
                rst_pin,
                ethspi_spi_host,
                sck_pin,
                miso_pin,
                mosi_pin);
      initDone = true;

      char newHostname[32] = {};
      generateHostname(hostname, macSizeForHostname, newHostname);
      strncpy(hostname, newHostname, sizeof(hostname) - 1);
      SUPLA_LOG_DEBUG("[%s] Network Lan/hostname: %s", getIntfName(), hostname);
      ETH.setHostname(hostname);
    }

    void disable() override {
      if (!allowDisable) {
        return;
      }

      allowDisable = false;
      SUPLA_LOG_DEBUG("[%s] disabling ETH connection", getIntfName());
      DisconnectProtocols();
      //    ETH.end();
    }

    bool getMacAddr(uint8_t *mac) override {
      if (initDone) {
        ETH.macAddress(mac);
      }
      return true;
    }

    void setHostname(const char *prefix, int macSize) override {
      macSizeForHostname = macSize;
      strncpy(hostname, prefix, sizeof(hostname) - 1);
      SUPLA_LOG_DEBUG("[%s] Network Lam/hostname: %s", getIntfName(), hostname);
    }

  protected:
    eth_phy_type_t ethspi_type;
    int32_t ethspi_phy_addr;
    int cs_pin;
    int irq_pin;
    int rst_pin;
    spi_host_device_t ethspi_spi_host;
    int sck_pin;
    int miso_pin;
    int mosi_pin;
    bool allowDisable = false;
    int macSizeForHostname = 0;
    bool initDone = false;
};
};  // namespace Supla


#endif  // SRC_SUPLA_NETWORK_ESP32ETHSPI_H_
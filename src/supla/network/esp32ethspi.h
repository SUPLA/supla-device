// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_NETWORK_ESP32ETHSPI_H_
#define SRC_SUPLA_NETWORK_ESP32ETHSPI_H_

#include <Arduino.h>
#include <ETH.h>
#include <supla/network/arduino_netif_config.h>
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
    explicit ESPETHSPI(eth_phy_type_t type, int32_t phy_addr,
                       int cs, int irq, int rst, spi_host_device_t spi_host,
                       int sck = -1, int miso = -1, int mosi = -1) {
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
            IPAddress localIP = ETH.localIP();
            IPAddress subnetMaskIP = ETH.subnetMask();
            IPAddress gatewayIP = ETH.gatewayIP();
            uint8_t mac[6] = {};
            ETH.macAddress(mac);
            SUPLA_LOG_INFO("localIP: %d.%d.%d.%d",
                           localIP[0],
                           localIP[1],
                           localIP[2],
                           localIP[3]);
            SUPLA_LOG_INFO("subnetMaskIP: %d.%d.%d.%d",
                           subnetMaskIP[0],
                           subnetMaskIP[1],
                           subnetMaskIP[2],
                           subnetMaskIP[3]);
            SUPLA_LOG_INFO("gatewayIP: %d.%d.%d.%d",
                           gatewayIP[0],
                           gatewayIP[1],
                           gatewayIP[2],
                           gatewayIP[3]);
            SUPLA_LOG_INFO("ETH MAC: %02X:%02X:%02X:%02X:%02X:%02X",
                           mac[0],
                           mac[1],
                           mac[2],
                           mac[3],
                           mac[4],
                           mac[5]);
            SUPLA_LOG_INFO("speed: %d Mbps", ETH.linkSpeed());
            if (ETH.fullDuplex()) {
              SUPLA_LOG_INFO("FULL_DUPLEX");
            }
            if (thisSpiEth) {
              thisSpiEth->setIpv4Addr(ETH.localIP());
            }
            break;
          }
        case ARDUINO_EVENT_ETH_DISCONNECTED: {
            SUPLA_LOG_INFO("[Ethernet] Disconnected");
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

      SUPLA_LOG_INFO("[Ethernet] establishing LAN connection");
      ETH.begin(ethspi_type,
                ethspi_phy_addr,
                cs_pin,
                irq_pin,
                rst_pin,
                ethspi_spi_host,
                sck_pin,
                miso_pin,
                mosi_pin);
      if (hasStaticIpConfig()) {
        const auto &cfg = getNetifConfig();
        IPAddress localIp = toArduinoIpAddress(cfg.ip);
        IPAddress gateway = toArduinoIpAddress(cfg.gateway);
        IPAddress subnet = toArduinoIpAddress(cfg.netmask);
        IPAddress dns1 = toArduinoIpAddress(cfg.dns1);
        IPAddress dns2 = toArduinoIpAddress(cfg.dns2);
        if (!ETH.config(localIp, gateway, subnet, dns1, dns2)) {
          SUPLA_LOG_WARNING("ETH SPI static IP config failed, continuing");
        }
      }
      initDone = true;

      char newHostname[32] = {};
      generateHostname(hostname, macSizeForHostname, newHostname);
      strncpy(hostname, newHostname, sizeof(hostname) - 1);
      SUPLA_LOG_DEBUG("[%s] Network LAN/hostname: %s", getIntfName(), hostname);
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

    const char *getIntfName() const override {
      return "ETH";
    }

    void setHostname(const char *prefix, int macSize) override {
      macSizeForHostname = macSize;
      strncpy(hostname, prefix, sizeof(hostname) - 1);
      SUPLA_LOG_DEBUG("[%s] Network LAN/hostname: %s", getIntfName(), hostname);
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

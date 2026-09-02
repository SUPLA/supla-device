// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

/*
  - for LAN8720 + ESP32 - using as less gpio as possible
  - 50MHz oscillator disable on LAN8720 by bridging the oscillator pins 1 and 2
  -ESP32 Gpio-        -LAN8720 PIN -
  GPIO22 - EMAC_TXD1   : TX1
  GPIO19 - EMAC_TXD0   : TX0
  GPIO21 - EMAC_TX_EN  : TX_EN
  GPIO26 - EMAC_RXD1   : RX1
  GPIO25 - EMAC_RXD0   : RX0
  GPIO27 - EMAC_RX_DV  : CRS
  GPIO17 - EMAC_TX_CLK : nINT/REFCLK (50MHz)
  GPIO23 - SMI_MDC     : MDC
  GPIO18 - SMI_MDIO    : MDIO
  GND                  : GND
  3V3                  : VCC
*/

#ifndef SRC_SUPLA_NETWORK_ESP32ETH_H_
#define SRC_SUPLA_NETWORK_ESP32ETH_H_

#include <Arduino.h>
#include <ETH.h>
#include <supla/network/arduino_netif_config.h>
#include <supla/network/netif_lan.h>
#include <supla/supla_lib_config.h>
#include <supla/log_wrapper.h>

namespace Supla {
class ESPETH;
}  // namespace Supla

static Supla::ESPETH *thisEth = nullptr;

namespace Supla {
class ESPETH : public Supla::LAN {
 public:
   /**
    * @brief Constructor for LAN8720 or TLK110 (legacy)
    *
    * @param ethmode 0 for LAN8720, 1 for TLK110
    */
  explicit ESPETH(uint8_t ethMode) {
    thisEth = this;
    if (ethMode == 0) {
      ethAddress = 0;
    } else {
      ethAddress = 1;
    }
  }

  /**
   * @brief Constructor for ETH_PHY configuration
   *
   * @param ethType ETH type: ETH_PHY_LAN8720, ETH_PHY_TLK110, ETH_PHY_RTL8201,
   *                ETH_PHY_DP83848, ETH_PHY_KSZ8041, ETH_PHY_KSZ8081
   * @param phyAddr I2C address
   * @param mdc     I2C clock GPIO
   * @param mdio    I2C data GPIO
   * @param power   Power control GPIO (-1 to disable)
   * @param clkMode Clock mode: ETH_CLOCK_GPIO0_IN, ETH_CLOCK_GPIO0_OUT,
   *                ETH_CLOCK_GPIO16_OUT, ETH_CLOCK_GPIO17_OUT
   */
  ESPETH(eth_phy_type_t ethType,
         int32_t phyAddr,
         int mdc,
         int mdio,
         int power,
         eth_clock_mode_t clkMode)
      : ethType(ethType),
        ethAddress(phyAddr),
        mdcPin(mdc),
        mdioPin(mdio),
        powerPin(power),
        clkMode(clkMode) {
    thisEth = this;
  }

  ~ESPETH() {
    if (thisEth == this) {
      thisEth = nullptr;
    }
  }

  static void networkEventHandler(arduino_event_id_t event) {
    switch (event) {
      case ARDUINO_EVENT_ETH_GOT_IP: {
        if (thisEth) {
          thisEth->setIpv4Addr(ETH.localIP());
        }
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
        break;
      }
      case ARDUINO_EVENT_ETH_DISCONNECTED: {
        if (thisEth) {
          SUPLA_LOG_INFO("[%s] Disconnected", thisEth->getIntfName());
          thisEth->setIpv4Addr(0);
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

    ::Network.onEvent(Supla::ESPETH::networkEventHandler);

    SUPLA_LOG_INFO(
        "[%s] setting up ETH (type %d, address %d, mdcPin %d, mdioPin %d, "
        "powerPin %d, clkMode %d)",
        thisEth->getIntfName(),
        ethType,
        ethAddress,
        mdcPin,
        mdioPin,
        powerPin,
        clkMode);
    ETH.begin(ethType, ethAddress, mdcPin, mdioPin, powerPin, clkMode);
    if (hasStaticIpConfig()) {
      const auto &cfg = getNetifConfig();
      IPAddress localIp = toArduinoIpAddress(cfg.ip);
      IPAddress gateway = toArduinoIpAddress(cfg.gateway);
      IPAddress subnet = toArduinoIpAddress(cfg.netmask);
      IPAddress dns1 = toArduinoIpAddress(cfg.dns1);
      IPAddress dns2 = toArduinoIpAddress(cfg.dns2);
      if (!ETH.config(localIp, gateway, subnet, dns1, dns2)) {
        SUPLA_LOG_WARNING("ETH static IP config failed, continuing");
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
  }

  bool getMacAddr(uint8_t *mac) override {
    if (initDone) {
      ETH.macAddress(mac);
      return true;
    }
    return false;
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
  bool allowDisable = false;
  int macSizeForHostname = 0;
  bool initDone = false;
  eth_phy_type_t ethType = ETH_PHY_LAN8720;
  int32_t ethAddress = -1;
  int mdcPin = 23;    // GPIO for I2C clock
  int mdioPin = 18;  // GPIO for I2C IO
  int powerPin = -1;  // -1 to disable
  eth_clock_mode_t clkMode = ETH_CLOCK_GPIO17_OUT;
};

}  // namespace Supla

#endif  // SRC_SUPLA_NETWORK_ESP32ETH_H_

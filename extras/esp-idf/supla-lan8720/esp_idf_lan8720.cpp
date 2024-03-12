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

#include "esp_idf_lan8720.h"

// FreeRTOS includes
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// ESP-IDF includes
#include <esp_eth_driver.h>
#include <esp_eth_netif_glue.h>
#include <esp_eth_com.h>
#include <esp_event.h>
#include <esp_netif.h>

#ifdef SUPLA_DEVICE_ESP32
#include <esp_mac.h>
#endif

// Supla includes
#include <SuplaDevice.h>
#include <supla/storage/config.h>
#include <supla/storage/storage.h>
#include <supla/supla_lib_config.h>
#include <supla/log_wrapper.h>
#include <supla/time.h>

#include <cstring>

#include "esp_idf_network_common.h"

static Supla::EspIdfLan8720 *thisNetIntfPtr = nullptr;

Supla::EspIdfLan8720::EspIdfLan8720(int mdcGpio, int mdioGpio) :
  mdcGpio(mdcGpio), mdioGpio(mdioGpio) {
  thisNetIntfPtr = this;
}

Supla::EspIdfLan8720::~EspIdfLan8720() {
  thisNetIntfPtr = nullptr;
}

static void eventHandler(void *arg,
                         esp_event_base_t eventBase,
                         int32_t eventId,
                         void *eventData) {
  if (thisNetIntfPtr == nullptr) {
    return;
  }

  if (eventBase == IP_EVENT) {
    switch (eventId) {
      case IP_EVENT_ETH_GOT_IP: {
        ip_event_got_ip_t *event = static_cast<ip_event_got_ip_t *>(eventData);
        thisNetIntfPtr->setIpv4Addr(event->ip_info.ip.addr);
        SUPLA_LOG_INFO("[%s] Got IP: Interface \"%s\" address: " IPSTR,
            thisNetIntfPtr->getIntfName(),
            esp_netif_get_desc(event->esp_netif),
            IP2STR(&event->ip_info.ip));
        break;
      }
      case IP_EVENT_ETH_LOST_IP: {
        thisNetIntfPtr->setIpv4Addr(0);
        SUPLA_LOG_DEBUG("[%s] Lost IP", thisNetIntfPtr->getIntfName());
        break;
      }
    }
  }
  if (eventBase == ETH_EVENT) {
    switch (eventId) {
      case ETHERNET_EVENT_START: {
        SUPLA_LOG_INFO("[%s] Ethernet started", thisNetIntfPtr->getIntfName());
        break;
      }
      case ETHERNET_EVENT_STOP: {
        SUPLA_LOG_INFO("[%s] Ethernet stopped", thisNetIntfPtr->getIntfName());
        break;
      }
      case ETHERNET_EVENT_CONNECTED: {
        SUPLA_LOG_INFO("[%s] Ethernet connected",
                       thisNetIntfPtr->getIntfName());
        break;
      }
      case ETHERNET_EVENT_DISCONNECTED: {
        thisNetIntfPtr->setIpv4Addr(0);
        if (thisNetIntfPtr->isStateLoggingAllowed()) {
          auto sdc = thisNetIntfPtr->getSdc();
          if (sdc) {
            sdc->addLastStateLog("Ethernet: disconnected");
          }
        }
        SUPLA_LOG_INFO("[%s] Ethernet disconnected",
                       thisNetIntfPtr->getIntfName());
        break;
      }
    }
  }
}

void Supla::EspIdfLan8720::setup() {
  setIpReady(false);
  if (!initDone) {
    Supla::initEspNetif();

    esp_netif_inherent_config_t espNetifConfig =
      ESP_NETIF_INHERENT_DEFAULT_ETH();
    espNetifConfig.if_desc = getIntfName();
    // set Ethernet priority higher than Wi-Fi STA (100)
    espNetifConfig.route_prio = 200;
    esp_netif_config_t netifConfig = {
      .base = &espNetifConfig,
      .driver = nullptr,
      .stack = ESP_NETIF_NETSTACK_DEFAULT_ETH
    };
    netIf = esp_netif_new(&netifConfig);
    esp_netif_set_hostname(netIf, hostname);

    // Init common MAC and PHY configs to default
    eth_mac_config_t macConfig = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phyConfig = ETH_PHY_DEFAULT_CONFIG();

    // Automatically detect PHY address
    phyConfig.phy_addr = -1;
    phyConfig.autonego_timeout_ms = 1000;
    // no hw reset
    phyConfig.reset_gpio_num = -1;
    eth_esp32_emac_config_t esp32EmacConfig = ETH_ESP32_EMAC_DEFAULT_CONFIG();
    esp32EmacConfig.smi_mdc_gpio_num = mdcGpio;
    esp32EmacConfig.smi_mdio_gpio_num = mdioGpio;
    esp_eth_mac_t *mac = esp_eth_mac_new_esp32(&esp32EmacConfig, &macConfig);
    esp_eth_phy_t *phy = esp_eth_phy_new_lan87xx(&phyConfig);

    ethHandle = NULL;
    esp_eth_config_t config = ETH_DEFAULT_CONFIG(mac, phy);
    auto result = esp_eth_driver_install(&config, &ethHandle);
    if (result != ESP_OK) {
      SUPLA_LOG_ERROR(
          "[%s] Ethernet driver install failed (%d)", getIntfName(), result);
    } else {
      SUPLA_LOG_INFO("[%s] Ethernet driver install success", getIntfName());
    }

    // combine driver with netif
    ethGlue = NULL;
    ethGlue = esp_eth_new_netif_glue(ethHandle);
    esp_netif_attach(netIf, ethGlue);

    ESP_ERROR_CHECK(esp_event_handler_register(
          ETH_EVENT, ESP_EVENT_ANY_ID, &eventHandler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(
          IP_EVENT, IP_EVENT_ETH_GOT_IP, &eventHandler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(
          IP_EVENT, IP_EVENT_ETH_LOST_IP, &eventHandler, NULL));

    esp_eth_start(ethHandle);
  }


  allowDisable = true;
  initDone = true;
}

void Supla::EspIdfLan8720::disable() {
  if (!allowDisable) {
    return;
  }

  allowDisable = false;
  SUPLA_LOG_DEBUG("[%s] disabling ETH connection", getIntfName());
  DisconnectProtocols();
  ESP_ERROR_CHECK(esp_eth_stop(ethHandle));
}

void Supla::EspIdfLan8720::uninit() {
  setIpReady(false);
  DisconnectProtocols();
  if (initDone) {
    SUPLA_LOG_DEBUG("[%s] stopping ETH driver", getIntfName());
    esp_event_handler_unregister(IP_EVENT, IP_EVENT_ETH_GOT_IP, eventHandler);
    esp_event_handler_unregister(IP_EVENT, IP_EVENT_ETH_LOST_IP, eventHandler);
    esp_event_handler_unregister(ETH_EVENT, ESP_EVENT_ANY_ID, eventHandler);

    esp_netif_destroy(netIf);
    esp_netif_deinit();

    if (ethHandle != NULL) {
      esp_eth_stop(ethHandle);
      esp_eth_del_netif_glue(ethGlue);
      esp_eth_driver_uninstall(ethHandle);
    }
  }
}

bool Supla::EspIdfLan8720::getMacAddr(uint8_t *out) {
  esp_read_mac(out, ESP_MAC_ETH);
  return true;
}

bool Supla::EspIdfLan8720::isIpSetupTimeout() {
  return false;
}

SuplaDeviceClass *Supla::EspIdfLan8720::getSdc() {
  return sdc;
}

bool Supla::EspIdfLan8720::isStateLoggingAllowed() {
  // state logging is not allowed when we manually disabled network interface.
  // We leave state logging to unexpected events instead.
  return allowDisable;
}

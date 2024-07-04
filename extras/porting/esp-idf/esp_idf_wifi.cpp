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

#include "esp_idf_wifi.h"

// FreeRTOS includes
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
// ESP-IDF includes
#include <esp_tls.h>
#include <esp_wifi.h>

#ifdef SUPLA_DEVICE_ESP32
#include <esp_mac.h>
#endif

#include <SuplaDevice.h>
#include <fcntl.h>
#include <supla/storage/config.h>
#include <supla/storage/storage.h>
#include <supla/supla_lib_config.h>
#include <supla/log_wrapper.h>
#include <supla/time.h>
#include <supla/tools.h>
#include <esp_event.h>
#include <esp_netif.h>

#include <cstring>

#include "esp_idf_network_common.h"

static Supla::EspIdfWifi *thisNetIntfPtr = nullptr;

Supla::EspIdfWifi::EspIdfWifi(const char *wifiSsid,
                              const char *wifiPassword,
                              unsigned char *ip)
    : Wifi(wifiSsid, wifiPassword, ip) {
  thisNetIntfPtr = this;
}

Supla::EspIdfWifi::~EspIdfWifi() {
  thisNetIntfPtr = nullptr;
}

bool Supla::EspIdfWifi::isReady() {
  return isWifiConnected && isIpReady;
}

static void eventHandler(void *arg,
                         esp_event_base_t eventBase,
                         int32_t eventId,
                         void *eventData) {
  static bool firstWiFiScanDone = false;
  if (thisNetIntfPtr == nullptr) {
    return;
  }

  if (eventBase == WIFI_EVENT) {
    switch (eventId) {
      case WIFI_EVENT_STA_STOP: {
        firstWiFiScanDone = false;
        break;
      }
      case WIFI_EVENT_STA_START: {
        SUPLA_LOG_DEBUG("[%s] Starting connection to AP",
                        thisNetIntfPtr->getIntfName());
        firstWiFiScanDone = false;
        esp_wifi_connect();
        break;
      }
      case WIFI_EVENT_STA_CONNECTED: {
        firstWiFiScanDone = true;
        if (thisNetIntfPtr) {
          thisNetIntfPtr->setWifiConnected(true);
        }
        SUPLA_LOG_DEBUG("[%s] Connected to AP", thisNetIntfPtr->getIntfName());
        break;
      }
      case WIFI_EVENT_STA_DISCONNECTED: {
        wifi_event_sta_disconnected_t *data =
            reinterpret_cast<wifi_event_sta_disconnected_t *>(eventData);
        if (thisNetIntfPtr) {
          thisNetIntfPtr->setIpReady(false);
          thisNetIntfPtr->setWifiConnected(false);
          if (firstWiFiScanDone) {
            // we ignore connection error if it happens first time
            thisNetIntfPtr->logWifiReason(data->reason);
          }
        }
        if (!thisNetIntfPtr->isInConfigMode()) {
          esp_wifi_connect();
          SUPLA_LOG_DEBUG(
                    "[%s] Connect to the AP fail (reason %d). Trying again",
                    thisNetIntfPtr->getIntfName(),
                    data->reason);
        }
        firstWiFiScanDone = true;
        break;
      }
    }
  } else if (eventBase == IP_EVENT) {
    switch (eventId) {
      case IP_EVENT_STA_GOT_IP: {
        ip_event_got_ip_t *event = static_cast<ip_event_got_ip_t *>(eventData);
        if (thisNetIntfPtr) {
          thisNetIntfPtr->setIpReady(true);
          thisNetIntfPtr->setIpv4Addr(event->ip_info.ip.addr);
        }
        SUPLA_LOG_INFO("[%s] Got IP " IPSTR,
                       thisNetIntfPtr->getIntfName(),
                       IP2STR(&event->ip_info.ip));
        break;
      }
      case IP_EVENT_STA_LOST_IP: {
        if (thisNetIntfPtr) {
          thisNetIntfPtr->setIpReady(false);
          thisNetIntfPtr->setIpv4Addr(0);
        }
        SUPLA_LOG_DEBUG("[%s] Lost IP", thisNetIntfPtr->getIntfName());
        break;
      }
    }
  }
}

uint32_t Supla::EspIdfWifi::getIP() {
  if (isReady()) {
    return ipv4;
  }
  return 0;
}

void Supla::EspIdfWifi::setup() {
  setIpReady(false);
  if (!initDone) {
    Supla::initEspNetif();

#ifdef SUPLA_DEVICE_ESP32
    apNetIf = esp_netif_create_default_wifi_ap();
    esp_netif_set_hostname(apNetIf, hostname);
    staNetIf = esp_netif_create_default_wifi_sta();
    esp_netif_set_hostname(staNetIf, hostname);
#endif /*SUPLA_DEVICE_ESP32*/

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_event_handler_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &eventHandler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &eventHandler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(
        IP_EVENT, IP_EVENT_STA_LOST_IP, &eventHandler, NULL));
    esp_wifi_set_ps(WIFI_PS_NONE);

  } else {
    disable();
  }

  if (mode == Supla::DEVICE_MODE_CONFIG) {
    wifi_config_t wifi_config = {};

    memcpy(wifi_config.ap.ssid, hostname, strlen(hostname));
    wifi_config.ap.max_connection = 4;  // default
    wifi_config.ap.channel = 6;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
  } else {
    SUPLA_LOG_INFO(
        "[%s] establishing connection with SSID: \"%s\"", getIntfName(), ssid);
    wifi_config_t wifi_config = {};
    memcpy(wifi_config.sta.ssid, ssid, MAX_SSID_SIZE);
    memcpy(wifi_config.sta.password, password, MAX_WIFI_PASSWORD_SIZE);
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
    wifi_config.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
#ifdef SUPLA_DEVICE_ESP32
    wifi_config.sta.failure_retry_cnt = 2;
#endif

    if (strlen(reinterpret_cast<char *>(wifi_config.sta.password))) {
      wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
  }
  ESP_ERROR_CHECK(esp_wifi_start());

  if (maxTxPower >= 0) {
    ESP_ERROR_CHECK(esp_wifi_set_max_tx_power(maxTxPower));
  }

  allowDisable = true;
  initDone = true;
#ifndef SUPLA_DEVICE_ESP32
  // ESP8266 hostname settings have to be done after esp_wifi_start
  tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_STA, hostname);
  tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_AP, hostname);
#endif
}

void Supla::EspIdfWifi::disable() {
  if (!allowDisable) {
    return;
  }

  allowDisable = false;
  SUPLA_LOG_DEBUG("[%s] disabling WiFi connection", getIntfName());
  DisconnectProtocols();
  uint8_t channel = 0;
  wifi_second_chan_t secondChannel = {};
  if (esp_wifi_get_channel(&channel, &secondChannel) == ESP_OK) {
    SUPLA_LOG_DEBUG("[%s] Storing Wi-Fi channel %d (%d)",
                    getIntfName(),
                    channel,
                    secondChannel);
    lastChannel = channel;
  }
  esp_wifi_disconnect();
  ESP_ERROR_CHECK(esp_wifi_stop());
}

void Supla::EspIdfWifi::uninit() {
  setWifiConnected(false);
  setIpReady(false);
  DisconnectProtocols();
  if (initDone) {
    SUPLA_LOG_DEBUG("[%s] stopping WiFi connection", getIntfName());
    esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, eventHandler);
    esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_LOST_IP, eventHandler);
    esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, eventHandler);
    esp_wifi_stop();
#ifdef SUPLA_DEVICE_ESP32
    if (apNetIf) {
      esp_netif_destroy_default_wifi(apNetIf);
      apNetIf = nullptr;
    }
    if (staNetIf) {
      esp_netif_destroy_default_wifi(staNetIf);
      staNetIf = nullptr;
    }
#endif

    esp_netif_deinit();

    esp_wifi_deauth_sta(0);
    esp_wifi_disconnect();

    esp_wifi_deinit();

    esp_event_loop_delete_default();
  }
}

void Supla::EspIdfWifi::fillStateData(TDSC_ChannelState *channelState) {
  channelState->Fields |= SUPLA_CHANNELSTATE_FIELD_IPV4 |
                          SUPLA_CHANNELSTATE_FIELD_MAC |
                          SUPLA_CHANNELSTATE_FIELD_WIFIRSSI |
                          SUPLA_CHANNELSTATE_FIELD_WIFISIGNALSTRENGTH;

  esp_read_mac(channelState->MAC, ESP_MAC_WIFI_STA);
  channelState->IPv4 = ipv4;

  wifi_ap_record_t ap;
  esp_wifi_sta_get_ap_info(&ap);
  int rssi = ap.rssi;
  channelState->WiFiRSSI = rssi;
  channelState->WiFiSignalStrength = Supla::rssiToSignalStrength(rssi);
}

void Supla::EspIdfWifi::setIpReady(bool ready) {
  connectedToWifiTimestamp = 0;
  isIpReady = ready;
}

void Supla::EspIdfWifi::setWifiConnected(bool state) {
  connectedToWifiTimestamp = millis();
  isWifiConnected = state;
}

void Supla::EspIdfWifi::setIpv4Addr(uint32_t ip) {
  ipv4 = ip;
}

bool Supla::EspIdfWifi::isInConfigMode() {
  return mode == Supla::DEVICE_MODE_CONFIG;
}

bool Supla::EspIdfWifi::getMacAddr(uint8_t *out) {
  esp_read_mac(out, ESP_MAC_WIFI_STA);
  return true;
}

void Supla::EspIdfWifi::logWifiReason(int reason) {
  bool reasonAlreadyReported = false;
  for (int i = 0; i < SUPLA_ESP_IDF_WIFI_LAST_REASON_MAX; i++) {
    if (lastReasons[i] == reason) {
      reasonAlreadyReported = true;
      break;
    }
  }

  if (lastReasonIdx >= SUPLA_ESP_IDF_WIFI_LAST_REASON_MAX) {
    lastReasonIdx = 0;
  }
  lastReasons[lastReasonIdx++] = reason;

  if (!reasonAlreadyReported && sdc) {
    switch (reason) {
      case 1: {
        sdc->addLastStateLog("Wi-Fi: disconnect (unspecified)");
        break;
      }
      case 2: {
        sdc->addLastStateLog("Wi-Fi: auth expire (incorrect password)");
        break;
      }
      case 3: {
        sdc->addLastStateLog("Wi-Fi: auth leave");
        break;
      }
      case 8: {
        sdc->addLastStateLog("Wi-Fi: disconnecting from AP");
        break;
      }
      case 15: {
        sdc->addLastStateLog(
            "Wi-Fi: 4way handshake timeout (incorrect password)");
        break;
      }
      case 200: {
        sdc->addLastStateLog("Wi-Fi: beacon timeout");
        break;
      }
      case 201: {
        char buf[100] = {};
        snprintf(buf, sizeof(buf), "Wi-Fi: \"%s\" not found", ssid);
        sdc->addLastStateLog(buf);
        break;
      }
      case 202: {
        sdc->addLastStateLog("Wi-Fi: auth fail");
        break;
      }
      case 203: {
        sdc->addLastStateLog("Wi-Fi: assoc fail");
        break;
      }
      case 204: {
        sdc->addLastStateLog("Wi-Fi: handshake timeout (incorrect password)");
        break;
      }
      case 205: {
        sdc->addLastStateLog("Wi-Fi: connection fail");
        break;
      }
      case 206: {
        sdc->addLastStateLog("Wi-Fi: ap tsf reset");
        break;
      }
      case 207: {
        sdc->addLastStateLog("Wi-Fi: roaming");
        break;
      }
      case 210: {
        sdc->addLastStateLog(
            "Wi-Fi: SSID found but with incompatible security");
        break;
      }
      case 211: {
        sdc->addLastStateLog(
            "Wi-Fi: AP found but with incompatible authmode (at least WPA2 PSK "
            "is required)");
        break;
      }
      case 212: {
        sdc->addLastStateLog("Wi-Fi: AP found but RSSI is too low");
        break;
      }
      default: {
        char buf[100] = {};
        // see:
        // https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/wifi.html#wi-fi-reason-code
        snprintf(buf, sizeof(buf), "Wi-Fi: disconnect reason %d", reason);
        sdc->addLastStateLog(buf);
        break;
      }
    }
  }
}

bool Supla::EspIdfWifi::isIpSetupTimeout() {
  //  if (connectedToWifiTimestamp && millis() - connectedToWifiTimestamp >
  //  3000) {
  //    return true;
  //  }
  return false;
}

void Supla::EspIdfWifi::setMaxTxPower(int power) {
  maxTxPower = power;
}

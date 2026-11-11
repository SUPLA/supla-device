// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "esp_idf_wifi.h"

#include <stdio.h>

// FreeRTOS includes
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
// ESP-IDF includes
#include <esp_tls.h>
#include <esp_wifi.h>

#include <esp_mac.h>

#include <SuplaDevice.h>
#include <fcntl.h>
#include <supla/input_noise_guard.h>
#include <supla/network/wifi_scan_result.h>
#include <supla/storage/config.h>
#include <supla/storage/storage.h>
#include <supla/supla_lib_config.h>
#include <supla/log_wrapper.h>
#include <supla/time.h>
#include <supla/tools.h>
#include <esp_event.h>
#include <esp_netif.h>
#include <esp_idf_version.h>
#include <arpa/inet.h>
#include <cstring>

#include "esp_idf_network_common.h"

constexpr int SUPLA_MAX_AP_TX_POWER = 60;

static Supla::EspIdfWifi *thisNetIntfPtr = nullptr;

namespace {

esp_netif_ip_info_t makeIpInfo(const Supla::NetifConfigBlob& cfg) {
  esp_netif_ip_info_t ipInfo = {};
  ipInfo.ip.addr = htonl(cfg.ip);
  ipInfo.netmask.addr = htonl(cfg.netmask);
  ipInfo.gw.addr = htonl(cfg.gateway);
  return ipInfo;
}

void setDnsInfo(esp_netif_t* netIf,
                esp_netif_dns_type_t dnsType,
                uint32_t value) {
  if (netIf == nullptr || value == 0) {
    return;
  }
  esp_netif_dns_info_t dns = {};
  dns.ip.type = IPADDR_TYPE_V4;
  dns.ip.u_addr.ip4.addr = htonl(value);
  esp_netif_set_dns_info(netIf, dnsType, &dns);
}

}  // namespace

Supla::EspIdfWifi::EspIdfWifi(const char *wifiSsid,
                              const char *wifiPassword,
                              unsigned char *ip)
    : Wifi(wifiSsid, wifiPassword, ip) {
  thisNetIntfPtr = this;
}

Supla::EspIdfWifi::~EspIdfWifi() {
  thisNetIntfPtr = nullptr;
}

bool Supla::EspIdfWifi::isStaticIpConfigured() const {
  return isStaticIpApplied();
}

static void eventHandler(void *arg,
                         esp_event_base_t eventBase,
                         int32_t eventId,
                         void *eventData) {
  if (thisNetIntfPtr == nullptr) {
    return;
  }
  SUPLA_LOG_DEBUG("[%s] Got Event: %d", thisNetIntfPtr->getIntfName(), eventId);

  if (eventBase == WIFI_EVENT) {
    switch (eventId) {
      case WIFI_EVENT_STA_START: {
        if (!thisNetIntfPtr->isInConfigMode() &&
            thisNetIntfPtr->isStateLoggingAllowed()) {
          SUPLA_LOG_DEBUG("[%s] Starting connection to AP",
                          thisNetIntfPtr->getIntfName());
          Supla::InputNoiseGuard::NotifyWifiTransition();
          esp_wifi_connect();
        }
        break;
      }
      case WIFI_EVENT_STA_CONNECTED: {
        if (thisNetIntfPtr) {
          thisNetIntfPtr->setWifiConnected(true);
          thisNetIntfPtr->setLastDisconnectReason(0);
          if (thisNetIntfPtr->isStaticIpConfigured()) {
            thisNetIntfPtr->setIpv4Addr(
                htonl(thisNetIntfPtr->getConfiguredStaticIp()));
            char ipBuf[32] = {};
            Supla::formatIpv4Address(thisNetIntfPtr->getConfiguredStaticIp(),
                                     ipBuf,
                                     sizeof(ipBuf));
            SUPLA_LOG_INFO("[%s] Connected to AP with static IP %s",
                           thisNetIntfPtr->getIntfName(),
                           ipBuf);
          }
        }
        SUPLA_LOG_DEBUG("[%s] Connected to AP", thisNetIntfPtr->getIntfName());
        break;
      }
      case WIFI_EVENT_STA_DISCONNECTED: {
        wifi_event_sta_disconnected_t *data =
            reinterpret_cast<wifi_event_sta_disconnected_t *>(eventData);
        bool unexpected = false;
        if (thisNetIntfPtr) {
          thisNetIntfPtr->setLastDisconnectReason(data->reason);
          unexpected = thisNetIntfPtr->handleWifiDisconnect(data->reason);
        }
        if (unexpected && !thisNetIntfPtr->isInConfigMode() &&
            thisNetIntfPtr->isStateLoggingAllowed()) {
          Supla::InputNoiseGuard::NotifyWifiStaDisconnected();
          esp_wifi_connect();
          SUPLA_LOG_DEBUG(
                    "[%s] Connect to the AP fail (reason %d). Trying again",
                    thisNetIntfPtr->getIntfName(),
                    data->reason);
        }
        break;
      }
      case WIFI_EVENT_SCAN_DONE: {
        if (thisNetIntfPtr) {
          thisNetIntfPtr->finishConfigModeScan();
        }
        break;
      }
    }
  } else if (eventBase == IP_EVENT) {
    switch (eventId) {
      case IP_EVENT_STA_GOT_IP: {
        Supla::InputNoiseGuard::NotifyWifiStaConnected();
        ip_event_got_ip_t *event = static_cast<ip_event_got_ip_t *>(eventData);
        if (thisNetIntfPtr) {
          thisNetIntfPtr->setIpv4Addr(event->ip_info.ip.addr);
        }
        SUPLA_LOG_INFO("[%s] Got IP " IPSTR,
                       thisNetIntfPtr->getIntfName(),
                       IP2STR(&event->ip_info.ip));
        break;
      }
      case IP_EVENT_STA_LOST_IP: {
        if (thisNetIntfPtr) {
          thisNetIntfPtr->setIpv4Addr(0);
        }
        SUPLA_LOG_DEBUG("[%s] Lost IP", thisNetIntfPtr->getIntfName());
        break;
      }
      case
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
          IP_EVENT_ASSIGNED_IP_TO_CLIENT:
#else
          IP_EVENT_AP_STAIPASSIGNED:
#endif
      {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
        ip_event_assigned_ip_to_client_t *data =
            reinterpret_cast<ip_event_assigned_ip_to_client_t *>(eventData);
#else
        ip_event_ap_staipassigned_t *data =
            reinterpret_cast<ip_event_ap_staipassigned_t *>(eventData);
#endif
        char log[SUPLA_SECURITY_LOG_TEXT_SIZE] = {};
        constexpr char LogText[] = "Device (" MACSTR ") joined";
        static_assert(sizeof(log) > sizeof(LogText));
        snprintf(log, sizeof(log) - 1, LogText, MAC2STR(data->mac));
        thisNetIntfPtr->addSecurityLog(ntohl(data->ip.addr), log);
        break;
      }
    }
  }
}

void Supla::EspIdfWifi::setup() {
  if (initDone) {
    SUPLA_LOG_DEBUG("[%s] reinitializing WiFi connection", getIntfName());
    disable();
  }
  setAllowDisable(false);
  resetLinkState();
  setStaticIpApplied(false);
  delay(50);
  int txPower = maxTxPower;
  if (txPower < 0) {
    txPower = 80;  // set max value
  }

  if (!initDone) {
    Supla::initEspNetif();

    apNetIf = esp_netif_create_default_wifi_ap();
    esp_netif_set_hostname(apNetIf, hostname);
    staNetIf = esp_netif_create_default_wifi_sta();
    esp_netif_set_hostname(staNetIf, hostname);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    Supla::InputNoiseGuard::NotifyWifiTransition();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_event_handler_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &eventHandler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &eventHandler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(
        IP_EVENT, IP_EVENT_STA_LOST_IP, &eventHandler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(
        IP_EVENT,
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
        IP_EVENT_ASSIGNED_IP_TO_CLIENT,
#else
        IP_EVENT_AP_STAIPASSIGNED,
#endif
        &eventHandler,
        NULL));

    esp_wifi_set_ps(WIFI_PS_NONE);
    initDone = true;
  }

  if (mode == Supla::DEVICE_MODE_OFFLINE) {
    SUPLA_LOG_DEBUG("[%s] offline mode, skipping WiFi connection",
                    getIntfName());
    return;
  } else if (mode == Supla::DEVICE_MODE_CONFIG) {
    SUPLA_LOG_DEBUG("[%s] starting AP config mode", getIntfName());
    wifi_config_t wifi_config = {};

    memcpy(wifi_config.ap.ssid, hostname, strlen(hostname));
    wifi_config.ap.max_connection = 4;  // default
    wifi_config.ap.channel = 6;
    wifi_config.ap.beacon_interval = 200;

    Supla::InputNoiseGuard::NotifyWifiTransition();
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    uint8_t proto = WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G;
    esp_wifi_set_protocol(WIFI_IF_AP, proto);
    if (txPower > SUPLA_MAX_AP_TX_POWER) {
      txPower = SUPLA_MAX_AP_TX_POWER;  // limit TX power in AP mode
    }

  } else {
    SUPLA_LOG_INFO(
        "[%s] establishing connection with SSID: \"%s\"", getIntfName(), ssid);
    wifi_config_t wifi_config = {};
    memcpy(wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    memcpy(wifi_config.sta.password, password, MAX_WIFI_PASSWORD_SIZE);
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    if (testMode) {
      SUPLA_LOG_INFO("[%s] test mode enabled, using fast scan", getIntfName());
      wifi_config.sta.scan_method = WIFI_FAST_SCAN;
    } else {
      wifi_config.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
    }

    wifi_config.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
    wifi_config.sta.failure_retry_cnt = 2;

    if (strlen(reinterpret_cast<char *>(wifi_config.sta.password))) {
      wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    }

    Supla::InputNoiseGuard::NotifyWifiTransition();
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    if (hasStaticIpConfig() && staNetIf != nullptr) {
      esp_err_t dhcpStopResult = esp_netif_dhcpc_stop(staNetIf);
      if (dhcpStopResult != ESP_OK) {
        SUPLA_LOG_WARNING("[%s] failed to stop DHCP client (%d)",
                          getIntfName(),
                          dhcpStopResult);
      }

      esp_netif_ip_info_t ipInfo = makeIpInfo(getNetifConfig());
      esp_err_t ipResult = esp_netif_set_ip_info(staNetIf, &ipInfo);
      if (ipResult != ESP_OK) {
        SUPLA_LOG_WARNING("[%s] failed to set static IP info (%d)",
                          getIntfName(),
                          ipResult);
        esp_err_t dhcpStartResult = esp_netif_dhcpc_start(staNetIf);
        if (dhcpStartResult != ESP_OK) {
          SUPLA_LOG_WARNING("[%s] failed to restart DHCP client (%d)",
                            getIntfName(),
                            dhcpStartResult);
        }
      } else {
        setDnsInfo(staNetIf, ESP_NETIF_DNS_MAIN, getNetifConfig().dns1);
        setDnsInfo(staNetIf, ESP_NETIF_DNS_BACKUP, getNetifConfig().dns2);
        setStaticIpApplied(true);
        SUPLA_LOG_INFO("[%s] static IP configured", getIntfName());
      }
    }
  }
  delay(50);

  Supla::InputNoiseGuard::NotifyWifiTransition();
  setAllowDisable(true);
  esp_err_t startResult = esp_wifi_start();
  reportStartResult(startResult == ESP_OK);
  initDone = true;
  if (startResult != ESP_OK) {
    SUPLA_LOG_WARNING("[%s] WiFi start failed (%d)",
                      getIntfName(),
                      startResult);
    return;
  }
  if (txPower >= 0) {
    SUPLA_LOG_INFO("[%s] setting TX power to %d", getIntfName(), txPower);
    ESP_ERROR_CHECK(esp_wifi_set_max_tx_power(txPower));
  }

  if (mode == Supla::DEVICE_MODE_CONFIG) {
    startConfigModeScan();
  }
}

void Supla::EspIdfWifi::disable() {
  if (!takeAllowDisable()) {
    return;
  }

  expectWifiDisconnect();
  configModeScanInProgress = false;
  setStaticIpApplied(false);
  setWifiConnected(false);
  setIpv4Addr(0);
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
  Supla::InputNoiseGuard::NotifyWifiTransition();
  esp_err_t result = esp_wifi_disconnect();
  if (result != ESP_OK && result != ESP_ERR_WIFI_NOT_INIT &&
      result != ESP_ERR_WIFI_NOT_STARTED &&
      result != ESP_ERR_WIFI_NOT_CONNECT) {
    SUPLA_LOG_WARNING("[%s] WiFi disconnect failed (%d)",
                      getIntfName(),
                      result);
  }
  Supla::InputNoiseGuard::NotifyWifiTransition();
  result = esp_wifi_stop();
  if (result != ESP_OK && result != ESP_ERR_WIFI_NOT_INIT &&
      result != ESP_ERR_WIFI_NOT_STARTED) {
    SUPLA_LOG_WARNING("[%s] WiFi stop failed (%d)", getIntfName(), result);
  }
}

void Supla::EspIdfWifi::uninit() {
  setAllowDisable(false);
  resetLinkState();
  setWifiConnected(false);
  configModeScanInProgress = false;
  setStaticIpApplied(false);
  DisconnectProtocols();
  if (initDone) {
    SUPLA_LOG_DEBUG("[%s] stopping WiFi connection", getIntfName());
    esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, eventHandler);
    esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_LOST_IP, eventHandler);
    esp_event_handler_unregister(IP_EVENT,
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
                                 IP_EVENT_ASSIGNED_IP_TO_CLIENT,
#else
                                 IP_EVENT_AP_STAIPASSIGNED,
#endif
                                 eventHandler);
    esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, eventHandler);
    Supla::InputNoiseGuard::NotifyWifiTransition();
    esp_err_t result = esp_wifi_disconnect();
    if (result != ESP_OK && result != ESP_ERR_WIFI_NOT_INIT &&
        result != ESP_ERR_WIFI_NOT_STARTED &&
        result != ESP_ERR_WIFI_NOT_CONNECT) {
      SUPLA_LOG_WARNING("[%s] WiFi disconnect failed during uninit (%d)",
                        getIntfName(),
                        result);
    }
    Supla::InputNoiseGuard::NotifyWifiTransition();
    result = esp_wifi_stop();
    if (result != ESP_OK && result != ESP_ERR_WIFI_NOT_INIT &&
        result != ESP_ERR_WIFI_NOT_STARTED) {
      SUPLA_LOG_WARNING("[%s] WiFi stop failed during uninit (%d)",
                        getIntfName(),
                        result);
    }
    if (apNetIf) {
      esp_netif_destroy_default_wifi(apNetIf);
      apNetIf = nullptr;
    }
    if (staNetIf) {
      esp_netif_destroy_default_wifi(staNetIf);
      staNetIf = nullptr;
    }

    result = esp_wifi_deinit();
    if (result != ESP_OK && result != ESP_ERR_WIFI_NOT_INIT) {
      SUPLA_LOG_WARNING("[%s] WiFi deinit failed (%d)",
                        getIntfName(),
                        result);
    }
  }
  initDone = false;
  resetLinkState();
}

void Supla::EspIdfWifi::fillStateData(TDSC_ChannelState *channelState) {
  channelState->Fields |= SUPLA_CHANNELSTATE_FIELD_IPV4 |
                          SUPLA_CHANNELSTATE_FIELD_MAC |
                          SUPLA_CHANNELSTATE_FIELD_WIFIRSSI |
                          SUPLA_CHANNELSTATE_FIELD_WIFISIGNALSTRENGTH;

  esp_read_mac(channelState->MAC, ESP_MAC_WIFI_STA);
  channelState->IPv4 = getIP();

  wifi_ap_record_t ap;
  esp_wifi_sta_get_ap_info(&ap);
  int rssi = ap.rssi;
  channelState->WiFiRSSI = rssi;
  channelState->WiFiSignalStrength = Supla::rssiToSignalStrength(rssi);
}

void Supla::EspIdfWifi::setLastDisconnectReason(int reason) {
  lastDisconnectReason = reason;
}

bool Supla::EspIdfWifi::isAccessPointConnected() const {
  return isLinkConnected();
}

int Supla::EspIdfWifi::getLastDisconnectReason() const {
  return lastDisconnectReason;
}

bool Supla::EspIdfWifi::isInConfigMode() {
  return mode == Supla::DEVICE_MODE_CONFIG;
}

void Supla::EspIdfWifi::startConfigModeScan() {
  if (mode != Supla::DEVICE_MODE_CONFIG || configModeScanInProgress) {
    return;
  }

  wifi_scan_config_t scanConfig = {};
  scanConfig.show_hidden = true;

  esp_err_t result = esp_wifi_scan_start(&scanConfig, false);
  if (result == ESP_OK) {
    configModeScanInProgress = true;
    SUPLA_LOG_INFO("[%s] config mode scan started", getIntfName());
  } else {
    configModeScanInProgress = false;
    SUPLA_LOG_WARNING("[%s] config mode scan start failed (%d)",
                      getIntfName(),
                      result);
  }
}

void Supla::EspIdfWifi::finishConfigModeScan() {
  if (!configModeScanInProgress) {
    return;
  }

  configModeScanInProgress = false;
  auto cache = Supla::WifiScanResultCache::Instance();
  cache->beginUpdate();

  uint16_t apCount = 0;
  if (esp_wifi_scan_get_ap_num(&apCount) != ESP_OK) {
    cache->clear();
    SUPLA_LOG_WARNING("[%s] config mode scan failed", getIntfName());
    esp_wifi_clear_ap_list();
    return;
  }

  for (uint16_t i = 0; i < apCount; i++) {
    wifi_ap_record_t ap = {};
    if (esp_wifi_scan_get_ap_record(&ap) != ESP_OK) {
      break;
    }
    cache->addOrUpdate(reinterpret_cast<const char *>(ap.ssid),
                       ap.rssi,
                       ap.primary);
  }

  esp_wifi_clear_ap_list();
  cache->finishUpdate(millis());
  SUPLA_LOG_INFO("[%s] config mode scan completed (%u networks)",
                 getIntfName(),
                 static_cast<unsigned int>(apCount));
}

bool Supla::EspIdfWifi::isConfigModeScanInProgress() const {
  return configModeScanInProgress;
}

bool Supla::EspIdfWifi::getMacAddr(uint8_t *out) {
  esp_read_mac(out, ESP_MAC_WIFI_STA);
  return true;
}

bool Supla::EspIdfWifi::isIpSetupTimeout() {
  return false;
}

void Supla::EspIdfWifi::setMaxTxPower(int power) {
  maxTxPower = power;
}

uint32_t Supla::EspIdfWifi::getConfiguredStaticIp() const {
  return getNetifConfig().ip;
}

esp_netif_t *Supla::EspIdfWifi::getStaNetIf() const {
  return staNetIf;
}

void Supla::EspIdfWifi::addSecurityLog(uint32_t ip, const char *log) const {
  if (sdc) {
    sdc->addSecurityLog(ip, log);
  }
}

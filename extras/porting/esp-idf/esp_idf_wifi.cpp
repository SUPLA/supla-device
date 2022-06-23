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

#include <esp_netif.h>

#ifdef SUPLA_DEVICE_ESP32
#include <esp_mac.h>
#endif

#include <SuplaDevice.h>
#include <fcntl.h>
#include <nvs_flash.h>
#include <supla/auto_lock.h>
#include <supla/mutex.h>
#include <supla/storage/config.h>
#include <supla/storage/storage.h>
#include <supla/supla_lib_config.h>

#include <cstring>

#include "esp_idf_wifi.h"

#ifndef SUPLA_DEVICE_ESP32
// delete name variant is deprecated in ESP-IDF, however ESP8266 RTOS still
// use it.
#define esp_tls_conn_destroy esp_tls_conn_delete
#endif

static Supla::EspIdfWifi *netIntfPtr = nullptr;

extern const uint8_t suplaOrgCertPemStart[] asm(
    "_binary_supla_org_cert_pem_start");
extern const uint8_t suplaOrgCertPemEnd[] asm("_binary_supla_org_cert_pem_end");

Supla::EspIdfWifi::EspIdfWifi(const char *wifiSsid,
                              const char *wifiPassword,
                              unsigned char *ip)
    : Wifi(wifiSsid, wifiPassword, ip) {
  netIntfPtr = this;
  mutex = Supla::Mutex::Create();
}

Supla::EspIdfWifi::~EspIdfWifi() {
  netIntfPtr = nullptr;
}

int Supla::EspIdfWifi::read(void *buf, int count) {
  Supla::AutoLock autoLock(mutex);
  if (client == nullptr) {
    return 0;
  }

  esp_tls_conn_read(client, nullptr, 0);
  int tlsErr = 0;
  esp_tls_get_and_clear_last_error(client->error_handle, &tlsErr, nullptr);
  autoLock.unlock();
  if (tlsErr != 0 && -tlsErr != ESP_TLS_ERR_SSL_WANT_READ &&
      -tlsErr != ESP_TLS_ERR_SSL_WANT_WRITE) {
    supla_log(LOG_ERR, "Connection error %d", tlsErr);
    Disconnect();
    return 0;
  }
  autoLock.lock();
  if (client == nullptr) {
    return 0;
  }
  _supla_int_t size = esp_tls_get_bytes_avail(client);
  autoLock.unlock();
  if (size < 0) {
    supla_log(LOG_ERR, "error in esp tls get bytes avail %d", size);
    Disconnect();
    return 0;
  }
  if (size > 0) {
    int ret = 0;
    do {
      autoLock.lock();
      if (client == nullptr) {
        return 0;
      }
      ret = esp_tls_conn_read(client, buf, count);
      autoLock.unlock();

      if (ret == ESP_TLS_ERR_SSL_WANT_READ ||
          ret == ESP_TLS_ERR_SSL_WANT_WRITE) {
        vTaskDelay(1);
        continue;
      }
      if (ret < 0) {
        supla_log(LOG_ERR, "esp_tls_conn_read  returned -0x%x", -ret);
        ret = 0;
        break;
      }
      if (ret == 0) {
        supla_log(LOG_INFO, "connection closed");
        Disconnect();
        return 0;
      }
      break;
    } while (1);

#ifdef SUPLA_COMM_DEBUG
    printData("Recv", buf, ret);
#endif
    return ret;
  }

  return -1;
}

int Supla::EspIdfWifi::write(void *buf, int count) {
  Supla::AutoLock autoLock(mutex);
  if (client == nullptr) {
    return 0;
  }
#ifdef SUPLA_COMM_DEBUG
  printData("Send", buf, count);
#endif
  int sendSize = esp_tls_conn_write(client, buf, count);
  if (sendSize == 0) {
    isServerConnected = false;
  }
  return sendSize;
}

int Supla::EspIdfWifi::connect(const char *server, int port) {
  Supla::AutoLock autoLock(mutex);
  if (client != nullptr) {
    supla_log(LOG_ERR, "client ptr should be null when trying to connect");
    return 0;
  }

  esp_tls_cfg_t cfg = {};
  cfg.cacert_pem_buf = suplaOrgCertPemStart;
  cfg.cacert_pem_bytes =
      static_cast<unsigned int>(suplaOrgCertPemEnd - suplaOrgCertPemStart);
  cfg.timeout_ms = timeoutMs;
  cfg.non_block = false;

  int connectionPort = (sslEnabled ? 2016 : 2015);
  if (port != -1) {
    connectionPort = port;
  }

  supla_log(LOG_INFO,
            "Establishing connection with: %s (port: %d)",
            server,
            connectionPort);

  client = esp_tls_init();
  if (!client) {
    supla_log(LOG_ERR, "ESP TLS INIT FAILED");
    return 0;
  }
  int result = esp_tls_conn_new_sync(
      server, strlen(server), connectionPort, &cfg, client);
  if (result == 1) {
    isServerConnected = true;
    int socketFd = 0;
    if (esp_tls_get_conn_sockfd(client, &socketFd) == ESP_OK) {
      fcntl(socketFd, F_SETFL, O_NONBLOCK);
    }

  } else {
    supla_log(LOG_DEBUG,
              "last errors %d %d %d",
              client->error_handle->last_error,
              client->error_handle->esp_tls_error_code,
              client->error_handle->esp_tls_flags);
    logConnReason(client->error_handle->last_error,
                  client->error_handle->esp_tls_error_code,
                  client->error_handle->esp_tls_flags);
    isServerConnected = false;
    esp_tls_conn_destroy(client);
    client = nullptr;
  }

  // SuplaDevice expects 1 on success, which is the same
  // as esp_tls_conn_new_sync returned values
  return result;
}

bool Supla::EspIdfWifi::connected() {
  return isServerConnected;
}

void Supla::EspIdfWifi::disconnect() {
  Supla::AutoLock autoLock(mutex);
  isServerConnected = false;
  if (client != nullptr) {
    esp_tls_conn_destroy(client);
    client = nullptr;
  }
}

bool Supla::EspIdfWifi::isReady() {
  return isWifiConnected && isIpReady;
}

static void eventHandler(void *arg,
                         esp_event_base_t eventBase,
                         int32_t eventId,
                         void *eventData) {
  if (eventBase == WIFI_EVENT) {
    switch (eventId) {
      case WIFI_EVENT_STA_START: {
        supla_log(LOG_DEBUG, "Starting connection to AP");
        esp_wifi_connect();
        break;
      }
      case WIFI_EVENT_STA_CONNECTED: {
        if (netIntfPtr) {
          netIntfPtr->setWifiConnected(true);
        }
        supla_log(LOG_DEBUG, "Connected to AP");
        break;
      }
      case WIFI_EVENT_STA_DISCONNECTED: {
        wifi_event_sta_disconnected_t *data =
            reinterpret_cast<wifi_event_sta_disconnected_t *>(eventData);
        if (netIntfPtr) {
          netIntfPtr->setIpReady(false);
          netIntfPtr->setWifiConnected(false);
          netIntfPtr->logWifiReason(data->reason);
          netIntfPtr->disconnect();
        }
        if (!netIntfPtr->isInConfigMode()) {
          esp_wifi_connect();
          supla_log(LOG_DEBUG,
                    "connect to the AP fail (reason %d). Trying again",
                    data->reason);
        }
        break;
      }
    }
  } else if (eventBase == IP_EVENT) {
    switch (eventId) {
      case IP_EVENT_STA_GOT_IP: {
        ip_event_got_ip_t *event = static_cast<ip_event_got_ip_t *>(eventData);
        if (netIntfPtr) {
          netIntfPtr->setIpReady(true);
          netIntfPtr->setIpv4Addr(event->ip_info.ip.addr);
        }
        supla_log(LOG_INFO, "got ip " IPSTR, IP2STR(&event->ip_info.ip));
        break;
      }
      case IP_EVENT_STA_LOST_IP: {
        if (netIntfPtr) {
          netIntfPtr->setIpReady(false);
          netIntfPtr->setIpv4Addr(0);
        }
        supla_log(LOG_DEBUG, "lost ip");
        break;
      }
    }
  }
}

void Supla::EspIdfWifi::setup() {
  Supla::AutoLock autoLock(mutex);
  setWifiConnected(false);
  setIpReady(false);
  if (!initDone) {
    nvs_flash_init();
    esp_netif_init();

    wifiEventGroup = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_loop_create_default());

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
    supla_log(LOG_DEBUG, "WiFi: resetting WiFi connection");
    autoLock.unlock();
    disconnect();
    autoLock.lock();
    esp_wifi_disconnect();
    ESP_ERROR_CHECK(esp_wifi_stop());
  }

  initDone = true;

  if (mode == Supla::DEVICE_MODE_CONFIG) {
    wifi_config_t wifi_config = {};

    memcpy(wifi_config.ap.ssid, hostname, strlen(hostname));
    wifi_config.ap.max_connection = 4;  // default
    wifi_config.ap.channel = 6;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
  } else {
    supla_log(
        LOG_INFO, "Wi-Fi: establishing connection with SSID: \"%s\"", ssid);
    wifi_config_t wifi_config = {};
    memcpy(wifi_config.sta.ssid, ssid, MAX_SSID_SIZE);
    memcpy(wifi_config.sta.password, password, MAX_WIFI_PASSWORD_SIZE);
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
    wifi_config.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;

    if (strlen(reinterpret_cast<char *>(wifi_config.sta.password))) {
      wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
  }
  ESP_ERROR_CHECK(esp_wifi_start());

#ifndef SUPLA_DEVICE_ESP32
  // ESP8266 hostname settings have to be done after esp_wifi_start
  tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_STA, hostname);
  tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_AP, hostname);
#endif
}

void Supla::EspIdfWifi::uninit() {
  setWifiConnected(false);
  setIpReady(false);
  disconnect();
  if (initDone) {
    supla_log(LOG_DEBUG, "Wi-Fi: stopping WiFi connection");
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

    vEventGroupDelete(wifiEventGroup);
    esp_event_loop_delete_default();
  }
}

void Supla::EspIdfWifi::setTimeout(int newTimeoutMs) {
  if (newTimeoutMs > 0) {
    timeoutMs = newTimeoutMs;
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
  if (rssi > -50) {
    channelState->WiFiSignalStrength = 100;
  } else if (rssi <= -100) {
    channelState->WiFiSignalStrength = 0;
  } else {
    channelState->WiFiSignalStrength = 2 * (rssi + 100);
  }
}

void Supla::EspIdfWifi::setIpReady(bool ready) {
  isIpReady = ready;
}

void Supla::EspIdfWifi::setWifiConnected(bool state) {
  isWifiConnected = state;
}

void Supla::EspIdfWifi::setIpv4Addr(unsigned _supla_int_t ip) {
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
  for (int i = 0; i < sizeof(lastReasons); i++) {
    if (lastReasons[i] == reason) {
      reasonAlreadyReported = true;
      break;
    }
  }

  if (lastReasonIdx >= sizeof(lastReasons)) {
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
      default: {
        char buf[100] = {};
        snprintf(buf, sizeof(buf), "Wi-Fi: disconnect reason %d", reason);
        sdc->addLastStateLog(buf);
        break;
      }
    }
  }
}

void Supla::EspIdfWifi::logConnReason(int error, int tlsError, int tlsFlags) {
  if (sdc && (lastConnErr != error || lastTlsErr != tlsError)) {
    lastConnErr = error;
    lastTlsErr = tlsError;
    switch (error) {
      case ESP_ERR_ESP_TLS_CANNOT_RESOLVE_HOSTNAME: {
        sdc->addLastStateLog("Connection: can't resolve hostname");
        break;
      }
      case ESP_ERR_ESP_TLS_FAILED_CONNECT_TO_HOST: {
        sdc->addLastStateLog("Connection: failed connect to host");
        break;
      }
      case ESP_ERR_ESP_TLS_CONNECTION_TIMEOUT: {
        sdc->addLastStateLog("Connection: connection timeout");
        break;
      }
      case ESP_ERR_MBEDTLS_SSL_HANDSHAKE_FAILED: {
        switch (tlsError) {
          case -MBEDTLS_ERR_X509_CERT_VERIFY_FAILED: {
            sdc->addLastStateLog(
                "Connection TLS: handshake fail - server "
                "certificate verification error");
            break;
          }
          default: {
            char buf[100] = {};
            snprintf(buf,
                     100,
                     "Connection TLS: handshake fail (TLS 0x%X "
                     "flags 0x%x)",
                     tlsError,
                     tlsFlags);
            sdc->addLastStateLog(buf);
            break;
          }
        }
        break;
      }
      default: {
        char buf[100] = {};
        snprintf(buf,
                 100,
                 "Connection: error 0x%X (TLS 0x%X flags 0x%x)",
                 error,
                 tlsError,
                 tlsFlags);
        sdc->addLastStateLog(buf);
        break;
      }
    }
  }
}

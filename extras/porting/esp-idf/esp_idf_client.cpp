/*
 * Copyright (C) AC SOFTWARE SP. Z O.O
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <supla/mutex.h>
#include <supla/auto_lock.h>
#include <fcntl.h>
#include <esp_netif.h>
#include <supla/log_wrapper.h>
#include <SuplaDevice.h>
#include <supla/time.h>

#include <string.h>

#include "esp_idf_client.h"
#include "supla/network/client.h"

#ifndef SUPLA_DEVICE_ESP32
// delete name variant is deprecated in ESP-IDF, however ESP8266 RTOS still
// use it.
#define esp_tls_conn_destroy esp_tls_conn_delete

// Latest ESP-IDF moved definition of esp_tls_t to private section and added
// methods to access members. This change is missing in esp8266, so below
// method is added to keep the same functionality
void esp_tls_get_error_handle(esp_tls_t *client,
                              esp_tls_error_handle_t *errorHandle) {
  *errorHandle = client->error_handle;
}

#endif


Supla::EspIdfClient::EspIdfClient() {
  mutex = Supla::Mutex::Create();
}

Supla::EspIdfClient::~EspIdfClient() {
}

int Supla::EspIdfClient::connectImp(const char *host, uint16_t port) {
  Supla::AutoLock autoLock(mutex);
  if (client != nullptr) {
    SUPLA_LOG_ERROR("client ptr should be null when trying to connect");
    return 0;
  }

  esp_tls_cfg_t cfg = {};
  if (rootCACert) {
    cfg.cacert_pem_buf = reinterpret_cast<const unsigned char *>(rootCACert);
    int len = strlen(rootCACert);
    cfg.cacert_pem_bytes = len + 1;
  }
  cfg.timeout_ms = timeoutMs;
  cfg.non_block = false;

  client = esp_tls_init();
  if (!client) {
    SUPLA_LOG_ERROR("ESP TLS INIT FAILED");
    return 0;
  }
  int result = esp_tls_conn_new_sync(
      host, strlen(host), port, &cfg, client);
  if (result == 1) {
    isConnected = true;
    int socketFd = 0;
    if (esp_tls_get_conn_sockfd(client, &socketFd) == ESP_OK) {
      fcntl(socketFd, F_SETFL, O_NONBLOCK);
    }

  } else {
    esp_tls_error_handle_t errorHandle;
    esp_tls_get_error_handle(client, &errorHandle);

    SUPLA_LOG_DEBUG(
              "last errors %d %d %d",
              errorHandle->last_error,
              errorHandle->esp_tls_error_code,
              errorHandle->esp_tls_flags);
    logConnReason(errorHandle->last_error,
                  errorHandle->esp_tls_error_code,
                  errorHandle->esp_tls_flags);
    isConnected = false;
    esp_tls_conn_destroy(client);
    client = nullptr;
  }

  // SuplaDevice expects 1 on success, which is the same
  // as esp_tls_conn_new_sync returned values
  return result;
}

std::size_t Supla::EspIdfClient::writeImp(const uint8_t *buf,
                                          std::size_t size) {
  Supla::AutoLock autoLock(mutex);
  if (client == nullptr) {
    return 0;
  }
  int sendSize = esp_tls_conn_write(client, buf, size);
  if (sendSize == 0) {
    isConnected = false;
  }
  return sendSize;
}

int Supla::EspIdfClient::available() {
  Supla::AutoLock autoLock(mutex);
  if (client == nullptr) {
    return 0;
  }

  esp_tls_conn_read(client, nullptr, 0);
  int tlsErr = 0;
  esp_tls_error_handle_t errorHandle = {};
  esp_tls_get_error_handle(client, &errorHandle);
  esp_tls_get_and_clear_last_error(errorHandle, &tlsErr, nullptr);
  autoLock.unlock();
  if (tlsErr != 0 && -tlsErr != ESP_TLS_ERR_SSL_WANT_READ &&
      -tlsErr != ESP_TLS_ERR_SSL_WANT_WRITE) {
    SUPLA_LOG_ERROR("Connection error %d", tlsErr);
    stop();
    return 0;
  }
  autoLock.lock();
  if (client == nullptr) {
    return 0;
  }
  _supla_int_t size = esp_tls_get_bytes_avail(client);
  autoLock.unlock();
  if (size < 0) {
    SUPLA_LOG_ERROR("error in esp tls get bytes avail %d", size);
    stop();
    return 0;
  }
  return size;
}

int Supla::EspIdfClient::readImp(uint8_t *buf, std::size_t size) {
  if (available() > 0) {
    Supla::AutoLock autoLock(mutex);
    int ret = 0;
    do {
      autoLock.lock();
      if (client == nullptr) {
        return 0;
      }
      ret = esp_tls_conn_read(client, buf, size);
      autoLock.unlock();

      if (ret == ESP_TLS_ERR_SSL_WANT_READ ||
          ret == ESP_TLS_ERR_SSL_WANT_WRITE) {
        delay(1);
        continue;
      }
      if (ret < 0) {
        SUPLA_LOG_ERROR("esp_tls_conn_read  returned -0x%x", -ret);
        ret = 0;
        break;
      }
      if (ret == 0) {
        SUPLA_LOG_INFO("connection closed");
        stop();
        return 0;
      }
      break;
    } while (1);

    return ret;
  }

  return -1;
}

void Supla::EspIdfClient::stop() {
  Supla::AutoLock autoLock(mutex);
  isConnected = false;
  if (client != nullptr) {
    esp_tls_conn_destroy(client);
    client = nullptr;
  }
}

uint8_t Supla::EspIdfClient::connected() {
  return isConnected;
}

void Supla::EspIdfClient::logConnReason(int error, int tlsError, int tlsFlags) {
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
      case ESP_ERR_MBEDTLS_X509_CRT_PARSE_FAILED: {
        sdc->addLastStateLog(
            "CA certificate parsing error - please provide correct one");
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

void Supla::EspIdfClient::setTimeoutMs(uint16_t _timeoutMs) {
  timeoutMs = _timeoutMs;
}

Supla::Client *Supla::ClientBuilder() {
  return new Supla::EspIdfClient;
}

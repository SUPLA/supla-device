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

#include <stddef.h>
#include <string.h>
#include <supla-common/log.h>
#include <supla/time.h>
#include <supla/tools.h>
#include <supla/log_wrapper.h>
#include <supla/network/html_generator.h>
#include <supla/storage/storage.h>
#include <supla/storage/config.h>
#include <esp_tls.h>
#include <mbedtls/aes.h>

#include "esp_idf_web_server.h"

#ifdef SUPLA_DEVICE_ESP32
#include "esp_https_server.h"
#endif  // SUPLA_DEVICE_ESP32

#define IV_SIZE 16       // AES block size

static Supla::EspIdfWebServer *serverInstance = nullptr;

esp_err_t getFavicon(httpd_req_t *req) {
  SUPLA_LOG_DEBUG("SERVER: get favicon.ico");
  if (serverInstance) {
    serverInstance->notifyClientConnected();
  }
  httpd_resp_set_type(req, "image/x-icon");
  httpd_resp_send(req, (const char *)(Supla::favico), sizeof(Supla::favico));
  return ESP_OK;
}

esp_err_t getHandler(httpd_req_t *req) {
  SUPLA_LOG_DEBUG("SERVER: get request");
  Supla::EspIdfSender sender(req);

  if (serverInstance && serverInstance->htmlGenerator) {
    serverInstance->notifyClientConnected();
    serverInstance->htmlGenerator->sendPage(&sender, serverInstance->dataSaved);
    serverInstance->dataSaved = false;
  }

  return ESP_OK;
}

esp_err_t getBetaHandler(httpd_req_t *req) {
  SUPLA_LOG_DEBUG("SERVER: get beta request");
  Supla::EspIdfSender sender(req);

  if (serverInstance && serverInstance->htmlGenerator) {
    serverInstance->notifyClientConnected();
    serverInstance->htmlGenerator->sendBetaPage(&sender,
                                                serverInstance->dataSaved);
    serverInstance->dataSaved = false;
  }

  return ESP_OK;
}

esp_err_t postHandler(httpd_req_t *req) {
  SUPLA_LOG_DEBUG("SERVER: post request");
  if (serverInstance) {
    if (serverInstance->handlePost(req)) {
      httpd_resp_set_status(req, "303 See Other");
      httpd_resp_set_hdr(req, "Location", "/");
      httpd_resp_send(req, "Supla", 5);

      return ESP_OK;
    }
  }
  return ESP_FAIL;
}

esp_err_t postBetaHandler(httpd_req_t *req) {
  SUPLA_LOG_DEBUG("SERVER: beta post request");
  if (serverInstance) {
    if (serverInstance->handlePost(req, true)) {
      httpd_resp_set_status(req, "303 See Other");
      httpd_resp_set_hdr(req, "Location", "/beta");
      httpd_resp_send(req, "Supla", 5);

      return ESP_OK;
    }
  }
  return ESP_FAIL;
}

/**
 * Custom URI matcher that will catch all URLs for http->https redirect
 *
 * @param reference_uri
 * @param uri_to_match
 * @param match_upto
 *
 * @return true
 */
bool uriMatchAll(const char *reference_uri,
                                   const char *uri_to_match,
                                   size_t match_upto) {
  return true;
}

esp_err_t redirectHandler(httpd_req_t *req) {
    char host[64] = {0};
    if (httpd_req_get_hdr_value_str(req, "Host", host, sizeof(host)) !=
        ESP_OK) {
      strncpy(host, "192.168.4.1", sizeof(host));
    }

    char httpsUrl[600];
    snprintf(httpsUrl, sizeof(httpsUrl), "https://%s%s", host, req->uri);
    SUPLA_LOG_DEBUG("SERVER: redirect to %s (uri: %s)", httpsUrl, req->uri);

    httpd_resp_set_status(req, "301 Moved Permanently");
    httpd_resp_set_hdr(req, "Location", httpsUrl);
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

httpd_uri_t uriGet = {
  .uri = "/", .method = HTTP_GET, .handler = getHandler, .user_ctx = NULL};

httpd_uri_t uriGetBeta = {.uri = "/beta",
  .method = HTTP_GET,
  .handler = getBetaHandler,
  .user_ctx = NULL};

httpd_uri_t uriFavicon = {.uri = "/favicon.ico",
                          .method = HTTP_GET,
                          .handler = getFavicon,
                          .user_ctx = NULL};

httpd_uri_t uriPost = {
    .uri = "/", .method = HTTP_POST, .handler = postHandler, .user_ctx = NULL};

httpd_uri_t uriPostBeta = {.uri = "/beta",
                           .method = HTTP_POST,
                           .handler = postBetaHandler,
                           .user_ctx = NULL};

Supla::EspIdfWebServer::EspIdfWebServer(Supla::HtmlGenerator *generator)
    : WebServer(generator) {
  serverInstance = this;
}

Supla::EspIdfWebServer::~EspIdfWebServer() {
  serverInstance = nullptr;
  cleanupCerts();
}

bool Supla::EspIdfWebServer::handlePost(httpd_req_t *req, bool beta) {
  notifyClientConnected(true);
  resetParser();
  if (beta) {
    setBetaProcessing();
  }

  size_t contentLen = req->content_len;
  SUPLA_LOG_DEBUG("SERVER: content length %d B", contentLen);

  char content[500];
  size_t recvSize = req->content_len;
  if (sizeof(content) - 1 < recvSize) {
    recvSize = sizeof(content) - 1;
  }

  int ret = 0;

  while (contentLen > 0) {
    ret = httpd_req_recv(req, content, recvSize);
    content[ret] = '\0';
    SUPLA_LOG_DEBUG(
              "SERVER: received %d B (cl %d): %s",
              ret,
              contentLen,
              content);
    contentLen -= ret;
    if (ret <= 0) {
      contentLen = 0;
      break;
    }

    parsePost(content, ret, (contentLen == 0));

    delay(1);
  }
  if (ret <= 0) {
    resetParser();
    if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
      httpd_resp_send_408(req);
    }
    return false;
  }

  // call getHandler to generate config html page
  serverInstance->dataSaved = true;

  return true;
}

bool Supla::EspIdfWebServer::verfiyCertificatesFormat() {
#ifdef SUPLA_DEVICE_ESP32
  if (!prvtKeyDecrypted) {
    prvtKeyDecrypted = true;
    auto cfg = Supla::Storage::ConfigInstance();
    uint8_t aesKey[32] = {};
    if (cfg && cfg->getAESKey(aesKey)) {
      SUPLA_LOG_INFO("AES key found");
      uint8_t iv[IV_SIZE] = {};
      memcpy(iv, prvtKey, IV_SIZE);

      mbedtls_aes_context aes = {};
      mbedtls_aes_init(&aes);
      auto result = mbedtls_aes_setkey_dec(&aes, aesKey, 256);
      SUPLA_LOG_DEBUG("mbedtls_aes_setkey_dec result: %d", result);

      prvtKeyLen = prvtKeyLen - IV_SIZE;

      SUPLA_LOG_INFO("prvtKeyLen: %d, IV_SIZE: %d", prvtKeyLen, IV_SIZE);
      uint8_t *encryptedData = new uint8_t[prvtKeyLen];
      memcpy(encryptedData, prvtKey + IV_SIZE, prvtKeyLen);
      memset(prvtKey, 0, prvtKeyLen);
      encryptedData[prvtKeyLen - 1] = '\0';
      result = mbedtls_aes_crypt_cbc(&aes,
                            MBEDTLS_AES_DECRYPT,
                            prvtKeyLen - 1,
                            iv,
                            encryptedData,
                            prvtKey);
      SUPLA_LOG_DEBUG("mbedtls_aes_crypt_cbc result: %d", result);
      delete[] encryptedData;
      mbedtls_aes_free(&aes);
    } else {
      SUPLA_LOG_INFO("AES key not found");
    }
  }

  if (strncmp(reinterpret_cast<const char *>(serverCert),
              "-----BEGIN CERTIFICATE-----",
              27) != 0) {
    SUPLA_LOG_WARNING("serverCert not valid, missing header");
    return false;
  }
  if (strncmp(reinterpret_cast<const char *>(prvtKey),
              "-----BEGIN PRIVATE KEY-----",
              27) != 0) {
    SUPLA_LOG_WARNING("prvtKey not valid, missing header");
    return false;
  }
  if (strstr(reinterpret_cast<const char *>(serverCert),
             "-----END CERTIFICATE-----") == nullptr) {
    SUPLA_LOG_WARNING("serverCert not valid, missing footer");
    return false;
  }
  if (strstr(reinterpret_cast<const char *>(prvtKey),
             "-----END PRIVATE KEY-----") == nullptr) {
    SUPLA_LOG_WARNING("prvtKey not valid, missing footer");
    return false;
  }
#endif  /* SUPLA_DEVICE_ESP32 */

  return true;
}

void Supla::EspIdfWebServer::start() {
  if (serverHttps || serverHttp) {
    return;
  }

  SUPLA_LOG_INFO("Starting local web server");
  bool fallbackToHttp = true;

  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.lru_purge_enable = true;

#ifdef SUPLA_DEVICE_ESP32
  httpd_ssl_config_t configHttps = HTTPD_SSL_CONFIG_DEFAULT();

  if (!verfiyCertificatesFormat()) {
    SUPLA_LOG_ERROR("Failed to verify certificates format");
  }

  configHttps.servercert = serverCert;
  configHttps.servercert_len = serverCertLen;

  configHttps.prvtkey_pem = prvtKey;
  configHttps.prvtkey_len = prvtKeyLen;
  if (httpd_ssl_start(&serverHttps, &configHttps) == ESP_OK) {
    fallbackToHttp = false;
    httpd_register_uri_handler(serverHttps, &uriGet);
    httpd_register_uri_handler(serverHttps, &uriGetBeta);
    httpd_register_uri_handler(serverHttps, &uriFavicon);
    httpd_register_uri_handler(serverHttps, &uriPost);
    httpd_register_uri_handler(serverHttps, &uriPostBeta);

    // start http with redirect
    config.uri_match_fn = uriMatchAll;
    if (httpd_start(&serverHttp, &config) == ESP_OK) {
      httpd_uri_t redirectAll = {
        .uri       = "/",  // we use uriMatchAll which will catch all URLs
        .method    = static_cast<httpd_method_t>(HTTP_ANY),
        .handler   = redirectHandler,
        .user_ctx  = NULL
      };
      httpd_register_uri_handler(serverHttp, &redirectAll);
    }
  } else {
    SUPLA_LOG_ERROR("Failed to start local https web server");
  }
#endif  // SUPLA_DEVICE_ESP32

  if (fallbackToHttp) {
    // fallback with standard http server
    if (httpd_start(&serverHttp, &config) == ESP_OK) {
      httpd_register_uri_handler(serverHttp, &uriGet);
      httpd_register_uri_handler(serverHttp, &uriGetBeta);
      httpd_register_uri_handler(serverHttp, &uriFavicon);
      httpd_register_uri_handler(serverHttp, &uriPost);
      httpd_register_uri_handler(serverHttp, &uriPostBeta);
    }
  }
}

void Supla::EspIdfWebServer::stop() {
  SUPLA_LOG_INFO("Stopping local web server");
  if (serverHttp) {
    httpd_stop(serverHttp);
    serverHttp = nullptr;
  }
  if (serverHttps) {
    httpd_stop(serverHttps);
    serverHttps = nullptr;
  }
}

Supla::EspIdfSender::EspIdfSender(httpd_req_t *req) : reqHandler(req) {
}

Supla::EspIdfSender::~EspIdfSender() {
  if (!error) {
    httpd_resp_send_chunk(reqHandler, nullptr, 0);
  }
}

void Supla::EspIdfSender::send(const char *buf, int size) {
  if (error || !buf || !reqHandler) {
    return;
  }
  if (size == -1) {
    size = strlen(buf);
  }
  if (size == 0) {
    return;
  }

  esp_err_t err = httpd_resp_send_chunk(reqHandler, buf, size);
  if (err != ESP_OK) {
    error = true;
  }
}

void Supla::EspIdfWebServer::cleanupCerts() {
  if (this->serverCert) {
    delete [] this->serverCert;
    serverCert = nullptr;
  }
  if (this->prvtKey) {
    delete [] this->prvtKey;
    prvtKey = nullptr;
  }
}

void Supla::EspIdfWebServer::setServerCertificate(
    const char *serverCert,
    int serverCertLen,
    const char *prvtKey,
    int prvtKeyLen) {
  cleanupCerts();

  this->serverCert = new uint8_t[serverCertLen];
  this->prvtKey = new uint8_t[prvtKeyLen];

  if (this->serverCert == nullptr || this->prvtKey == nullptr) {
    SUPLA_LOG_ERROR("Failed to allocate memory for https certificates");
    cleanupCerts();
    return;
  }

  this->serverCertLen = serverCertLen;
  memcpy(this->serverCert, serverCert, serverCertLen);

  this->prvtKeyLen = prvtKeyLen;
  memcpy(this->prvtKey, prvtKey, prvtKeyLen);
}


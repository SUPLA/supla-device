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
#include <supla/device/register_device.h>
#include <ctype.h>
#include <supla/crypto.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "esp_idf_web_server.h"

#include "esp_https_server.h"

#define IV_SIZE                      16  // AES block size
#define MAX_FAILED_LOGIN_ATTEMPTS    4
#define FAILED_LOGIN_ATTEMPT_TIMEOUT 60000  // 1 minute
#define SESSION_EXPIRATION_MS        600000  // 10 minutes

static Supla::EspIdfWebServer *srvInst = nullptr;

// request: GET /favicon.ico
// no auth required, just send the icon
esp_err_t getFavicon(httpd_req_t *req) {
  SUPLA_LOG_DEBUG("SERVER: get favicon.ico");
  if (srvInst) {
    srvInst->notifyClientConnected();
  }
  httpd_resp_set_hdr(req, "CN", Supla::RegisterDevice::getName());
  httpd_resp_set_type(req, "image/x-icon");
  httpd_resp_send(req, (const char *)(Supla::favico), sizeof(Supla::favico));
  return ESP_OK;
}

// request: GET/POST /
// - if not authorized, redirect to /login or /setup
esp_err_t rootHandler(httpd_req_t *req) {
  httpd_resp_set_hdr(req, "CN", Supla::RegisterDevice::getName());
  srvInst->reloadSaltPassword();
  if (srvInst->isAuthorizationBlocked()) {
    httpd_resp_set_hdr(req, "Auth-Status", "too-many");
    httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Too many requests");
    return ESP_OK;
  }
  char sessionCookie[256] = {};
  if (req->method == HTTP_GET) {
    SUPLA_LOG_DEBUG("SERVER: get request");
    if (srvInst->htmlGenerator) {
      if (!srvInst->ensureAuthorized(
              req, sessionCookie, sizeof(sessionCookie))) {
        return srvInst->redirect(req, 303, srvInst->loginOrSetupUrl(), "main");
      } else {
        Supla::EspIdfSender sender(req);
        srvInst->htmlGenerator->sendPage(
            &sender, srvInst->dataSaved, srvInst->isHttpsEnalbled());
      }
      srvInst->dataSaved = false;
    }
  } else if (req->method == HTTP_POST) {
    // request: POST /
    SUPLA_LOG_DEBUG("SERVER: post request");
    if (!srvInst->ensureAuthorized(req, sessionCookie, sizeof(sessionCookie))) {
      return srvInst->redirect(req, 303, srvInst->loginOrSetupUrl(), "main");
    }
    if (srvInst->handlePost(req)) {
      srvInst->redirect(req, 303, "/", "deleted");
      return ESP_OK;
    }
    return ESP_FAIL;
  }

  return ESP_FAIL;
}

// request: GET/POST /login
esp_err_t loginHandler(httpd_req_t *req) {
  httpd_resp_set_hdr(req, "CN", Supla::RegisterDevice::getName());
  srvInst->reloadSaltPassword();
  if (srvInst->isAuthorizationBlocked()) {
    httpd_resp_set_hdr(req, "Auth-Status", "too-many");
    httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Too many requests");
    return ESP_OK;
  }

  srvInst->notifyClientConnected();
  if (!srvInst->isPasswordConfigured()) {
    SUPLA_LOG_DEBUG(
        "SERVER: login request without password configured redirect to setup");
    return srvInst->redirect(req, 303, srvInst->loginOrSetupUrl(), "deleted");
  }
  char sessionCookie[256] = {};
  if (req->method == HTTP_GET) {
    SUPLA_LOG_DEBUG("SERVER: GET login request");
    if (srvInst->htmlGenerator) {
      if (srvInst->ensureAuthorized(
              req, sessionCookie, sizeof(sessionCookie))) {
        SUPLA_LOG_DEBUG("SERVER: already authorized, redirect to root");
        srvInst->redirect(req, 303, "/", "deleted");
        return ESP_OK;
      }
      SUPLA_LOG_DEBUG("SERVER: not authorized, send login page");
      Supla::EspIdfSender sender(req);
      srvInst->htmlGenerator->sendLoginPage(&sender);
    }
    return ESP_OK;
  } else if (req->method == HTTP_POST) {
    SUPLA_LOG_DEBUG("SERVER: POST login request");

    if (srvInst->htmlGenerator) {
      bool loginResult =
          srvInst->login(req, nullptr, sessionCookie, sizeof(sessionCookie));
      SUPLA_LOG_DEBUG("SERVER: login result %d", loginResult);
      if (!loginResult && !srvInst->ensureAuthorized(
                              req, sessionCookie, sizeof(sessionCookie),
                              true)) {
        srvInst->addSecurityLog(req, "Failed login attempt");
        SUPLA_LOG_DEBUG("SERVER: login failed, send login page");
        Supla::EspIdfSender sender(req);
        srvInst->htmlGenerator->sendLoginPage(&sender, true);
      } else {
        // redirect based on cookie value
        srvInst->addSecurityLog(req, "Successful login");
        SUPLA_LOG_DEBUG("SERVER: login success, redirect to root or beta");
        srvInst->redirect(req, 303, nullptr, "deleted");
      }
    }
    return ESP_OK;
  }
  SUPLA_LOG_DEBUG("SERVER: unknown request method");
  return ESP_FAIL;
}

// request: POST /logout
esp_err_t logoutHandler(httpd_req_t *req) {
  SUPLA_LOG_DEBUG("SERVER: post logout request");
  srvInst->reloadSaltPassword();
  httpd_resp_set_hdr(req, "CN", Supla::RegisterDevice::getName());
  if (srvInst->isAuthorizationBlocked()) {
    httpd_resp_set_hdr(req, "Auth-Status", "too-many");
    httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Too many requests");
    return ESP_OK;
  }

  if (srvInst->htmlGenerator) {
    srvInst->handleLogout(req);
    srvInst->redirect(req, 303, srvInst->loginOrSetupUrl(), "deleted");
  }
  return ESP_OK;
}

// request: GET/POST /setup
esp_err_t setupHandler(httpd_req_t *req) {
  srvInst->reloadSaltPassword();
  httpd_resp_set_hdr(req, "CN", Supla::RegisterDevice::getName());
  if (srvInst->isAuthorizationBlocked()) {
    httpd_resp_set_hdr(req, "Auth-Status", "too-many");
    httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Too many requests");
    return ESP_OK;
  }

  char sessionCookie[256] = {};
  if (req->method == HTTP_GET) {
    SUPLA_LOG_DEBUG("SERVER: GET setup request");
    if (srvInst->htmlGenerator) {
      Supla::EspIdfSender sender(req);
      if (!srvInst->isPasswordConfigured()) {
        srvInst->htmlGenerator->sendSetupPage(&sender, false);
      } else if (srvInst->ensureAuthorized(
                     req, sessionCookie, sizeof(sessionCookie))) {
        srvInst->htmlGenerator->sendSetupPage(&sender, true);
      } else {
        return srvInst->redirect(req, 303, "/login");
      }
    }
    return ESP_OK;
  } else if (req->method == HTTP_POST) {
    SUPLA_LOG_DEBUG("SERVER: POST setup request");
    if (srvInst->htmlGenerator) {
      if (!srvInst->isPasswordConfigured() ||
          srvInst->ensureAuthorized(
              req, sessionCookie, sizeof(sessionCookie))) {
        auto setupResult =
            srvInst->handleSetup(req, sessionCookie, sizeof(sessionCookie));
        if (setupResult != Supla::SetupRequestResult::OK) {
          Supla::EspIdfSender sender(req);
          srvInst->htmlGenerator->sendSetupPage(
              &sender, srvInst->isPasswordConfigured(), setupResult);
        } else {
          srvInst->redirect(req, 303, nullptr, "deleted");
        }
      }
    }
    return ESP_OK;
  }
  return ESP_FAIL;
}


// request: GET /logs
// - if not authorized, redirect to /login or /setup
esp_err_t logsHandler(httpd_req_t *req) {
  httpd_resp_set_hdr(req, "CN", Supla::RegisterDevice::getName());
  srvInst->reloadSaltPassword();
  if (srvInst->isAuthorizationBlocked()) {
    httpd_resp_set_hdr(req, "Auth-Status", "too-many");
    httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Too many requests");
    return ESP_OK;
  }
  char sessionCookie[256] = {};
  if (req->method == HTTP_GET) {
    SUPLA_LOG_DEBUG("SERVER: get request");
    if (srvInst->htmlGenerator) {
      if (!srvInst->ensureAuthorized(
              req, sessionCookie, sizeof(sessionCookie))) {
        return srvInst->redirect(req, 303, srvInst->loginOrSetupUrl(), "main");
      } else {
        Supla::EspIdfSender sender(req);
        srvInst->htmlGenerator->sendLogsPage(&sender,
                                             srvInst->isHttpsEnalbled());
      }
      srvInst->dataSaved = false;
    }
  }

  return ESP_FAIL;
}

// request: GET/POST /beta
esp_err_t betaHandler(httpd_req_t *req) {
  srvInst->reloadSaltPassword();
  httpd_resp_set_hdr(req, "CN", Supla::RegisterDevice::getName());
  if (srvInst->isAuthorizationBlocked()) {
    httpd_resp_set_hdr(req, "Auth-Status", "too-many");
    httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Too many requests");
    return ESP_OK;
  }

  char sessionCookie[256] = {};
  if (req->method == HTTP_GET) {
    SUPLA_LOG_DEBUG("SERVER: get beta request");
    if (srvInst->htmlGenerator) {
      if (!srvInst->ensureAuthorized(
              req, sessionCookie, sizeof(sessionCookie))) {
        return srvInst->redirect(req, 303, srvInst->loginOrSetupUrl(), "beta");
      } else {
        Supla::EspIdfSender sender(req);
        srvInst->htmlGenerator->sendBetaPage(
            &sender, srvInst->dataSaved, srvInst->isHttpsEnalbled());
      }
      srvInst->dataSaved = false;
    }

    return ESP_OK;
  } else if (req->method == HTTP_POST) {
    SUPLA_LOG_DEBUG("SERVER: beta post request");
    if (!srvInst->ensureAuthorized(req, sessionCookie, sizeof(sessionCookie))) {
      return srvInst->redirect(req, 303, srvInst->loginOrSetupUrl(), "beta");
    }
    if (srvInst->handlePost(req, true)) {
      srvInst->redirect(req, 303, "/beta", "deleted");
      return ESP_OK;
    }
    return ESP_FAIL;
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
    httpd_resp_set_hdr(req, "CN", Supla::RegisterDevice::getName());
    httpd_resp_set_hdr(req, "Location", httpsUrl);
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

httpd_uri_t uriRoot = {.uri = "/",
                      .method = static_cast<httpd_method_t>(HTTP_ANY),
                      .handler = rootHandler,
                      .user_ctx = NULL};

httpd_uri_t uriBeta = {.uri = "/beta",
                          .method = static_cast<httpd_method_t>(HTTP_ANY),
                          .handler = betaHandler,
                          .user_ctx = NULL};

httpd_uri_t uriFavicon = {.uri = "/favicon.ico",
                          .method = HTTP_GET,
                          .handler = getFavicon,
                          .user_ctx = NULL};

httpd_uri_t uriLogout = {
    .uri = "/logout",
    .method = HTTP_POST,
    .handler = logoutHandler,
    .user_ctx = NULL};

httpd_uri_t uriLogin = {
    .uri = "/login",
    .method = static_cast<httpd_method_t>(HTTP_ANY),
    .handler = loginHandler,
    .user_ctx = NULL};

httpd_uri_t uriSetup = {
    .uri = "/setup",
    .method = static_cast<httpd_method_t>(HTTP_ANY),
    .handler = setupHandler,
    .user_ctx = NULL};

httpd_uri_t uriLogs = {
    .uri = "/logs",
    .method = static_cast<httpd_method_t>(HTTP_ANY),
    .handler = logsHandler,
    .user_ctx = NULL};

Supla::EspIdfWebServer::EspIdfWebServer(Supla::HtmlGenerator *generator)
    : WebServer(generator) {
  srvInst = this;
}

Supla::EspIdfWebServer::~EspIdfWebServer() {
  srvInst = nullptr;
  cleanupCerts();
}

esp_err_t Supla::EspIdfWebServer::redirect(httpd_req_t *req,
                                      int code,
                                      const char *destination,
                                      const char *cookieRedirect) {
  char buf[100] = {};
  if (cookieRedirect) {
    int maxAge = 360;
    if (strcmp(cookieRedirect, "deleted") == 0) {
      maxAge = 0;
    }
    snprintf(buf,
             sizeof(buf),
             "redirect_to=%s; Path=/; HttpOnly; Secure; Max-Age=%d",
             cookieRedirect, maxAge);
    httpd_resp_set_hdr(req, "Set-Cookie", buf);
    SUPLA_LOG_DEBUG("SERVER: Set-Cookie: %s", buf);
  }

  switch (code) {
    default:
    case 302: {
      httpd_resp_set_status(req, "302 Found");
      break;
    }
    case 303: {
      httpd_resp_set_status(req, "303 See Other");
      break;
    }
  }
  if (destination) {
    httpd_resp_set_hdr(req, "Location", destination);
  } else {
    bool redirectToRoot = true;
    char buf[10] = {};
    size_t len = sizeof(buf);
    if (httpd_req_get_cookie_val(req, "redirect_to", buf, &len) == ESP_OK) {
      if (strcmp(buf, "beta") == 0) {
        redirectToRoot = false;
      }
    }

    httpd_resp_set_hdr(req, "Location", redirectToRoot ? "/" : "/beta");
  }
  httpd_resp_send(req, "Supla", 5);
  return ESP_OK;
}

bool Supla::EspIdfWebServer::isAuthorizationBlocked() {
  if (failedLoginAttempts > MAX_FAILED_LOGIN_ATTEMPTS) {
    if (millis() - lastLoginAttemptTimestamp > FAILED_LOGIN_ATTEMPT_TIMEOUT) {
      failedLoginAttempts = 0;
    } else {
      SUPLA_LOG_WARNING("SERVER: too many failed login attempts");
      return true;
    }
  }
  lastLoginAttemptTimestamp = millis();
  return false;
}

bool Supla::EspIdfWebServer::isSessionCookieValid(const char *sessionCookie) {
  // extract timestamp:
  char timestampStr[20] = {};
  const char *separator = strchr(sessionCookie, '|');
  if (separator == nullptr || separator - sessionCookie > 19) {
    SUPLA_LOG_WARNING("SERVER: invalid session cookie: missing separator");
    return false;
  }

  strncpy(timestampStr, sessionCookie, separator - sessionCookie);
  timestampStr[separator - sessionCookie] = '\0';
  uint32_t timestamp = atoi(timestampStr);
  if (millis() - timestamp > SESSION_EXPIRATION_MS) {
    SUPLA_LOG_WARNING("SERVER: invalid session cookie: expired");
    return false;
  }

  // check hmac:
  char sessionHmacHex[65] = {};
  char key[65] = {};
  generateHexString(sessionSecret, key, sizeof(sessionSecret));
  Supla::Crypto::hmacSha256Hex(key,
                               strlen(key),
                               timestampStr,
                               strlen(timestampStr),
                               sessionHmacHex,
                               sizeof(sessionHmacHex));
  if (strcmp(sessionHmacHex, separator + 1) == 0) {
    return true;
  }

  SUPLA_LOG_WARNING("SERVER: invalid session cookie: hmac mismatch");
//  SUPLA_LOG_WARNING("SERVER: expected: %s, received: %s", sessionHmacHex,
//                    separator + 1);
  return false;
}

bool Supla::EspIdfWebServer::ensureAuthorized(httpd_req_t *req,
                                              char *sessionCookie,
                                              int sessionCookieLen,
                                              bool loginFailed) {
  notifyClientConnected();
  reloadSaltPassword();

  if (!isHttpsEnalbled()) {
    // skip authorization when https is not available (fallback to legacy
    // behavior)
    return true;
  }

  if (isAuthorizationBlocked()) {
    httpd_resp_set_hdr(req, "Auth-Status", "too-many");
    return false;
  }

  size_t len = sessionCookieLen;
  if (httpd_req_get_cookie_val(req, "session", sessionCookie, &len) == ESP_OK) {
    if (isSessionCookieValid(sessionCookie)) {
      httpd_resp_set_hdr(req, "Auth-Status", "ok");
      // renew cookie
      SUPLA_LOG_DEBUG("SERVER: renewing session cookie");
      setSessionCookie(req, sessionCookie, sessionCookieLen);;
      SUPLA_LOG_DEBUG("SERVER: session cookie renewed");

      failedLoginAttempts = 0;
      return true;
    } else {
      if (!loginFailed) {
        failedLoginAttempt(req);
        httpd_resp_set_hdr(req, "Auth-Status", "failed");
      }
      return false;
    }
  } else {
    if (!loginFailed) {
      httpd_resp_set_hdr(req, "Auth-Status", "login-required");
    }
    return false;
  }

  return false;
}

bool Supla::EspIdfWebServer::handlePost(httpd_req_t *req, bool beta) {
  notifyClientConnected(true);
  auto cfg = Supla::Storage::ConfigInstance();
  char email[SUPLA_EMAIL_MAXSIZE] = {};
  char ssid[MAX_SSID_SIZE] = {};
  char password[MAX_WIFI_PASSWORD_SIZE] = {};
  char server[SUPLA_SERVER_NAME_MAXSIZE] = {};
  if (cfg) {
    cfg->getEmail(email);
    cfg->getWiFiSSID(ssid);
    cfg->getWiFiPassword(password);
    cfg->getSuplaServer(server);
  }
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
        "SERVER: received %d B (cl %d): %s", ret, contentLen, content);
    contentLen -= ret;
    if (ret <= 0) {
      contentLen = 0;
      break;
    }

    parsePost(content, ret, (contentLen == 0));

    delay(1);
  }

  if (cfg) {
    char buf[SUPLA_EMAIL_MAXSIZE] = {};
    static_assert(SUPLA_EMAIL_MAXSIZE >= MAX_SSID_SIZE);
    static_assert(SUPLA_EMAIL_MAXSIZE >= MAX_WIFI_PASSWORD_SIZE);
    static_assert(SUPLA_EMAIL_MAXSIZE >= SUPLA_SERVER_NAME_MAXSIZE);
    cfg->getEmail(buf);
    if (strcmp(email, buf) != 0) {
      addSecurityLog(req, "Email changed");
    }

    memset(buf, 0, sizeof(buf));
    cfg->getWiFiSSID(buf);
    if (strcmp(ssid, buf) != 0) {
      addSecurityLog(req, "Wi-Fi SSID changed");
    }

    memset(buf, 0, sizeof(buf));
    cfg->getWiFiPassword(buf);
    if (strcmp(password, buf) != 0) {
      addSecurityLog(req, "Wi-Fi password changed");
    }

    memset(buf, 0, sizeof(buf));
    cfg->getSuplaServer(buf);
    if (strcmp(server, buf) != 0) {
      addSecurityLog(req, "Supla server changed");
    }
  }

  if (ret <= 0) {
    resetParser();
    if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
      httpd_resp_send_408(req);
    }
    return false;
  }

  // call getHandler to generate config html page
  srvInst->dataSaved = true;

  return true;
}

bool Supla::EspIdfWebServer::verifyCertificatesFormat() {
  if (serverCert == nullptr || prvtKey == nullptr) {
    SUPLA_LOG_ERROR("SERVER: server certificate or private key not set");
    return false;
  }
  if (!prvtKeyDecrypted) {
    prvtKeyDecrypted = true;
    auto cfg = Supla::Storage::ConfigInstance();
    uint8_t aesKey[33] =
        {};  // key is 32 B, however mbedtls_aes_setkey_dec
             // uses const char* for key, so we add null terminator just in case
    if (cfg && cfg->getAESKey(aesKey)) {
      SUPLA_LOG_INFO("AES key found");
      uint8_t iv[IV_SIZE] = {};
      memcpy(iv, prvtKey, IV_SIZE);

      mbedtls_aes_context aes = {};
      mbedtls_aes_init(&aes);
      auto result = mbedtls_aes_setkey_dec(&aes, aesKey, 256);
      (void)(result);
      SUPLA_LOG_DEBUG("mbedtls_aes_setkey_dec result: %d", result);

      prvtKeyLen = prvtKeyLen - IV_SIZE;

      SUPLA_LOG_INFO("prvtKeyLen: %d, IV_SIZE: %d", prvtKeyLen, IV_SIZE);
      uint8_t *encryptedData = new uint8_t[prvtKeyLen + 1];
      memcpy(encryptedData, prvtKey + IV_SIZE, prvtKeyLen);
      memset(prvtKey, 0, prvtKeyLen + IV_SIZE);
      encryptedData[prvtKeyLen] = '\0';
      result = mbedtls_aes_crypt_cbc(&aes,
                            MBEDTLS_AES_DECRYPT,
                            prvtKeyLen,
                            iv,
                            encryptedData,
                            prvtKey);
      prvtKeyLen++;
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
  if (serverCert[serverCertLen - 1] != '\0') {
    SUPLA_LOG_WARNING("serverCert not valid, missing null terminator");
    return false;
  }
  if (prvtKey[prvtKeyLen - 1] != '\0') {
    SUPLA_LOG_WARNING("prvtKey not valid, missing null terminator");
    return false;
  }

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
  config.max_open_sockets = 2;
  config.stack_size = 6144;

  if (!verifyCertificatesFormat()) {
    SUPLA_LOG_ERROR("Failed to verify certificates format");
  } else {
    httpd_ssl_config_t configHttps = HTTPD_SSL_CONFIG_DEFAULT();
    configHttps.httpd.lru_purge_enable = true;
    configHttps.httpd.max_open_sockets = 1;
    configHttps.httpd.max_uri_handlers = 7;

    configHttps.servercert = serverCert;
    configHttps.servercert_len = serverCertLen;

    configHttps.prvtkey_pem = prvtKey;
    configHttps.prvtkey_len = prvtKeyLen;
    delay(100);
    if (httpd_ssl_start(&serverHttps, &configHttps) == ESP_OK) {
      fallbackToHttp = false;
      httpd_register_uri_handler(serverHttps, &uriRoot);
      httpd_register_uri_handler(serverHttps, &uriBeta);
      httpd_register_uri_handler(serverHttps, &uriFavicon);
      httpd_register_uri_handler(serverHttps, &uriLogin);
      httpd_register_uri_handler(serverHttps, &uriLogout);
      httpd_register_uri_handler(serverHttps, &uriSetup);
      httpd_register_uri_handler(serverHttps, &uriLogs);

      // start http with redirect
      config.uri_match_fn = uriMatchAll;
      config.max_uri_handlers = 1;
      config.max_open_sockets = 1;
      if (httpd_start(&serverHttp, &config) == ESP_OK) {
        httpd_uri_t redirectAll = {
            .uri = "/",  // we use uriMatchAll which will catch all URLs
            .method = static_cast<httpd_method_t>(HTTP_ANY),
            .handler = redirectHandler,
            .user_ctx = NULL};
        httpd_register_uri_handler(serverHttp, &redirectAll);
      }
    } else {
      SUPLA_LOG_ERROR("Failed to start local https web server");
      if (serverHttps) {
        httpd_stop(serverHttps);
        serverHttps = nullptr;
      }
    }
  }

  if (fallbackToHttp) {
    // fallback with standard http server
    if (httpd_start(&serverHttp, &config) == ESP_OK) {
      httpd_register_uri_handler(serverHttp, &uriRoot);
      httpd_register_uri_handler(serverHttp, &uriBeta);
      httpd_register_uri_handler(serverHttp, &uriFavicon);
      // for http we do not have login/logout and setup currently
//      httpd_register_uri_handler(serverHttp, &uriLogin);
//      httpd_register_uri_handler(serverHttp, &uriLogout);
//      httpd_register_uri_handler(serverHttp, &uriSetup);
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

bool Supla::EspIdfWebServer::isHttpsEnalbled() const {
  return serverHttps != nullptr;
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
  if (this->prvtKey) {
    delete [] this->prvtKey;
    prvtKey = nullptr;
  }
}

void Supla::EspIdfWebServer::setServerCertificate(
    const uint8_t *serverCert,
    int serverCertLen,
    const uint8_t *prvtKey,
    int prvtKeyLen) {
  cleanupCerts();

  this->serverCert = serverCert;

  this->prvtKey = new uint8_t[prvtKeyLen];

  SUPLA_LOG_INFO("Server certificate length: %d, private key length: %d",
                 serverCertLen,
                 prvtKeyLen);

  if (this->serverCert == nullptr || this->prvtKey == nullptr) {
    SUPLA_LOG_ERROR("Failed to allocate memory for https certificates");
    cleanupCerts();
    return;
  }

  this->serverCertLen = serverCertLen;

  this->prvtKeyLen = prvtKeyLen;
  memcpy(this->prvtKey, prvtKey, prvtKeyLen);
}

bool Supla::EspIdfWebServer::isPasswordConfigured() const {
  return saltPassword.salt[0] != 0;
}

const char *Supla::EspIdfWebServer::loginOrSetupUrl() const {
  if (isPasswordConfigured()) {
    return "/login";
  } else {
    return "/setup";
  }
}

bool Supla::EspIdfWebServer::isPasswordCorrect(const char *password) const {
  if (isPasswordConfigured()) {
    Supla::SaltPassword userPassword = {};
    userPassword.copySalt(saltPassword);
    Supla::Config::generateSaltPassword(password, &userPassword);
    return userPassword == saltPassword;
  }
  return false;
}

bool Supla::EspIdfWebServer::login(httpd_req_t *req, const char *password,
    char *sessionCookie, int sessionCookieLen) {
  reloadSaltPassword();
  bool passwordIsCorrect = false;
  if (isPasswordConfigured()) {
    if (password == nullptr) {
      // check if password is send in form
      char buf[256] = {};
      int remaining = req->content_len;

      if (remaining >= sizeof(buf)) {
        SUPLA_LOG_INFO("Setup request too large");
        return false;
      }

      auto ret = httpd_req_recv(req, buf, remaining);
      if (ret <= 0) {
        SUPLA_LOG_INFO("Failed to read setup request");
        return false;
      }

      buf[ret] = '\0';
      char formPassword[64] = {};
      httpd_query_key_value(buf, "cfg_pwd", formPassword, sizeof(formPassword));
      urlDecodeInplace(formPassword, sizeof(formPassword));

      if (isPasswordCorrect(formPassword)) {
        passwordIsCorrect = true;
      }
    } else if (isPasswordCorrect(password)) {
      passwordIsCorrect = true;
    }

    if (passwordIsCorrect) {
      // save session cookie
      SUPLA_LOG_DEBUG("SERVER: setting session cookie");
      httpd_resp_set_hdr(req, "Auth-Status", "ok");
      setSessionCookie(req, sessionCookie, sessionCookieLen);;
      SUPLA_LOG_INFO("Login successful (%s)", sessionCookie);
      return true;
    }
    httpd_resp_set_hdr(req, "Auth-Status", "failed");
    failedLoginAttempt(req);
  }
  SUPLA_LOG_INFO("Login failed (%d)", failedLoginAttempts);
  return false;
}

void Supla::EspIdfWebServer::setSessionCookie(httpd_req_t *req,
                                              char *buf,
                                              int bufLen) {
  uint32_t timestamp = millis();
  char timestampStr[20] = {};
  snprintf(timestampStr, sizeof(timestampStr) - 1, "%ld", timestamp);
  char sessionHmacHex[65] = {};
  char key[65] = {};
  generateHexString(sessionSecret, key, sizeof(sessionSecret));

  Supla::Crypto::hmacSha256Hex(key,
                               strlen(key),
                               timestampStr,
                               strlen(timestampStr),
                               sessionHmacHex,
                               sizeof(sessionHmacHex));

  snprintf(buf,
           bufLen,
           "session=%s|%s; Path=/; HttpOnly; Secure",
           timestampStr,
           sessionHmacHex);
  SUPLA_LOG_DEBUG("SERVER: Set-Cookie: %s", buf);
  httpd_resp_set_hdr(req, "Set-Cookie", buf);
}

void Supla::EspIdfWebServer::handleLogout(httpd_req_t *req) {
  // delete session cookie
  httpd_resp_set_hdr(req,
                     "Set-Cookie",
                     "session=deleted; Path=/; HttpOnly; Secure; Max-Age=0");
}

Supla::SetupRequestResult Supla::EspIdfWebServer::handleSetup(httpd_req_t *req,
    char *sessionCookie, int sessionCookieLen) {
  char buf[256] = {};
  int ret;
  int remaining = req->content_len;

  if (remaining >= sizeof(buf)) {
    SUPLA_LOG_INFO("Setup request too large");
    return SetupRequestResult::INVALID_REQUEST;
  }

  ret = httpd_req_recv(req, buf, remaining);
  if (ret <= 0) {
    SUPLA_LOG_INFO("Failed to read setup request");
    return SetupRequestResult::INVALID_REQUEST;
  }

  buf[ret] = '\0';

  char oldPassword[64] = {};
  char password[64] = {};
  char confirmPassword[64] = {};

  httpd_query_key_value(buf, "cfg_pwd", password, sizeof(password));
  httpd_query_key_value(
      buf, "confirm_cfg_pwd", confirmPassword, sizeof(confirmPassword));
  httpd_query_key_value(buf, "old_cfg_pwd", oldPassword, sizeof(oldPassword));
  urlDecodeInplace(password, sizeof(password));
  urlDecodeInplace(confirmPassword, sizeof(confirmPassword));
  urlDecodeInplace(oldPassword, sizeof(oldPassword));

  if (isPasswordConfigured()) {
    if (!isPasswordCorrect(oldPassword)) {
      SUPLA_LOG_INFO("Invalid old password");
      addSecurityLog(req, "Password change failed: invalid old password");
      return SetupRequestResult::INVALID_OLD_PASSWORD;
    }
  }

  if (strcmp(password, confirmPassword) != 0) {
    SUPLA_LOG_INFO("Passwords do not match");
    addSecurityLog(req, "Password change failed: passwords do not match");
    return SetupRequestResult::PASSWORD_MISMATCH;
  }

  if (!saltPassword.isPasswordStrong(password)) {
    SUPLA_LOG_INFO("Password is not strong enough");
    addSecurityLog(req,
                   "Password change failed: password too weak");
    return SetupRequestResult::WEAK_PASSWORD;
  }

  saltPassword.clear();
  Supla::Config::generateSaltPassword(password, &saltPassword);
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    cfg->setCfgModeSaltPassword(saltPassword);
    cfg->saveWithDelay(2000);
  }
  addSecurityLog(req, "Password successfully changed");
  return login(req, password, sessionCookie, sessionCookieLen)
             ? SetupRequestResult::OK
             : SetupRequestResult::INVALID_REQUEST;
}

void Supla::EspIdfWebServer::reloadSaltPassword() {
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    cfg->getCfgModeSaltPassword(&saltPassword);
  }
  if (sessionSecret[0] == '\0') {
    Supla::fillRandom(sessionSecret, sizeof(sessionSecret));
  }
}

uint32_t Supla::EspIdfWebServer::getIpFromReq(httpd_req_t *req) {
  uint32_t ip = 0;
  int sockfd = httpd_req_to_sockfd(req);
  struct sockaddr_in6 addr;
  socklen_t addr_len = sizeof(addr);
  if (getpeername(sockfd, (struct sockaddr *)&addr, &addr_len) == 0) {
    ip = ntohl(addr.sin6_addr.un.u32_addr[3]);
  }

  return ip;
}

void Supla::EspIdfWebServer::addSecurityLog(httpd_req_t *req,
                                            const char *log) const {
  WebServer::addSecurityLog(getIpFromReq(req), log);
}

void Supla::EspIdfWebServer::failedLoginAttempt(httpd_req_t *req) {
  failedLoginAttempts++;
  if (isAuthorizationBlocked()) {
    addSecurityLog(req, "Too many failed login attempts");
  }
}

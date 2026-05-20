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

#include <arpa/inet.h>
#include <ctype.h>
#include <esp_tls.h>
#include <mbedtls/pk.h>
#include <mbedtls/x509_crt.h>
#include <netinet/in.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <supla-common/log.h>
#include <supla/crypto.h>
#include <supla/device/register_device.h>
#include <supla/log_wrapper.h>
#include <supla/network/html_generator.h>
#include <supla/storage/config.h>
#include <supla/storage/storage.h>
#include <supla/time.h>
#include <supla/tools.h>
#include <sys/socket.h>
#if defined(__has_include)
#if __has_include(<mbedtls/aes.h>)
#define SUPLA_HAVE_MBEDTLS_AES 1
#include <mbedtls/aes.h>
#elif __has_include(<psa/crypto.h>)
#define SUPLA_HAVE_PSA_CRYPTO 1
#include <psa/crypto.h>
#endif
#endif
#if !defined(SUPLA_HAVE_MBEDTLS_AES) && !defined(SUPLA_HAVE_PSA_CRYPTO)
#define SUPLA_HAVE_MBEDTLS_AES 1
#include <mbedtls/aes.h>
#endif

#include "esp_https_server.h"
#include "esp_idf_web_server.h"

#define IV_SIZE                      16  // AES block size
#define MAX_FAILED_LOGIN_ATTEMPTS    4
#define FAILED_LOGIN_ATTEMPT_TIMEOUT 60000   // 1 minute
#define SESSION_EXPIRATION_MS        600000  // 10 minutes

static constexpr const char *HTTPS_CERT_KEY = "https_cert";
static constexpr const char *HTTPS_KEY_KEY = "https_key";
static constexpr size_t HTTPS_CERT_BUFFER_SIZE = 4096;
static constexpr size_t HTTPS_KEY_BUFFER_SIZE = 4096;
static constexpr size_t HTTPS_RSA_KEY_BITS = 2048;

static Supla::EspIdfWebServer *srvInst = nullptr;

#ifndef SUPLA_CSRF_LOGIN_SETUP_PROTECTION
// Temporary workaround for missing CSRF protection handling in mobile
// client for login and setup pages.
#define SUPLA_CSRF_LOGIN_SETUP_PROTECTION 0
#endif

static bool readCsrfTokenFromBody(httpd_req_t *req,
                                  char *csrfToken,
                                  size_t csrfTokenLen) {
  char buf[256] = {};
  size_t remaining = req->content_len;
  if (remaining >= sizeof(buf)) {
    SUPLA_LOG_INFO("Request too large");
    return false;
  }

  size_t offset = 0;
  while (remaining > 0) {
    int ret = httpd_req_recv(req, buf + offset, remaining);
    if (ret <= 0) {
      SUPLA_LOG_INFO("Failed to read request body");
      return false;
    }
    offset += ret;
    remaining -= ret;
  }

  buf[offset] = '\0';
  if (httpd_query_key_value(buf, "csrf", csrfToken, csrfTokenLen) != ESP_OK) {
    csrfToken[0] = '\0';
  }
  return true;
}

#if defined(SUPLA_HAVE_MBEDTLS_AES)
static int decryptAesCbc(const uint8_t *key,
                         size_t keyLen,
                         uint8_t *iv,
                         const uint8_t *input,
                         size_t inputLen,
                         uint8_t *output) {
  mbedtls_aes_context aes = {};
  mbedtls_aes_init(&aes);
  int result = mbedtls_aes_setkey_dec(&aes, key, keyLen * 8);
  if (result == 0) {
    result = mbedtls_aes_crypt_cbc(
        &aes, MBEDTLS_AES_DECRYPT, inputLen, iv, input, output);
  }
  mbedtls_aes_free(&aes);
  return result;
}
#elif defined(SUPLA_HAVE_PSA_CRYPTO)
static int decryptAesCbc(const uint8_t *key,
                         size_t keyLen,
                         uint8_t *iv,
                         const uint8_t *input,
                         size_t inputLen,
                         uint8_t *output) {
  psa_status_t status = psa_crypto_init();
  if (status != PSA_SUCCESS) {
    return -1;
  }
  psa_key_attributes_t attr = psa_key_attributes_init();
  psa_set_key_type(&attr, PSA_KEY_TYPE_AES);
  psa_set_key_usage_flags(&attr, PSA_KEY_USAGE_DECRYPT);
  psa_set_key_algorithm(&attr, PSA_ALG_CBC_NO_PADDING);
  psa_key_id_t keyId = 0;
  status = psa_import_key(&attr, key, keyLen, &keyId);
  psa_reset_key_attributes(&attr);
  if (status != PSA_SUCCESS) {
    return -1;
  }
  psa_cipher_operation_t op = psa_cipher_operation_init();
  status = psa_cipher_decrypt_setup(&op, keyId, PSA_ALG_CBC_NO_PADDING);
  if (status == PSA_SUCCESS) {
    status = psa_cipher_set_iv(&op, iv, IV_SIZE);
  }
  size_t outLen = 0;
  if (status == PSA_SUCCESS) {
    status = psa_cipher_update(&op, input, inputLen, output, inputLen, &outLen);
  }
  if (status == PSA_SUCCESS) {
    size_t finishLen = 0;
    status =
        psa_cipher_finish(&op, output + outLen, inputLen - outLen, &finishLen);
    outLen += finishLen;
  }
  psa_cipher_abort(&op);
  psa_destroy_key(keyId);
  return status == PSA_SUCCESS ? 0 : -1;
}
#endif

namespace {

static bool startsWith(const uint8_t *buffer,
                       size_t bufferLen,
                       const char *prefix) {
  const size_t prefixLen = strlen(prefix);
  return buffer != nullptr && bufferLen > prefixLen &&
         strncmp(reinterpret_cast<const char *>(buffer), prefix, prefixLen) ==
             0;
}

static bool validateHttpsCertificates(const uint8_t *serverCert,
                                      size_t serverCertLen,
                                      const uint8_t *prvtKey,
                                      size_t prvtKeyLen) {
  if (serverCert == nullptr || prvtKey == nullptr || serverCertLen == 0 ||
      prvtKeyLen == 0) {
    return false;
  }

  mbedtls_x509_crt cert = {};
  mbedtls_x509_crt_init(&cert);
  int result = mbedtls_x509_crt_parse(
      &cert,
      reinterpret_cast<const unsigned char *>(serverCert),
      serverCertLen);
  if (result != 0) {
    SUPLA_LOG_ERROR("SERVER: failed to parse server certificate: -0x%04X",
                    -result);
    mbedtls_x509_crt_free(&cert);
    return false;
  }

  mbedtls_pk_context pk = {};
  mbedtls_pk_init(&pk);
  result =
      mbedtls_pk_parse_key(&pk,
                           reinterpret_cast<const unsigned char *>(prvtKey),
                           prvtKeyLen,
                           nullptr,
                           0);
  if (result != 0) {
    SUPLA_LOG_ERROR("SERVER: failed to parse private key: -0x%04X", -result);
    mbedtls_pk_free(&pk);
    mbedtls_x509_crt_free(&cert);
    return false;
  }

  result = mbedtls_pk_check_pair(&cert.pk, &pk);
  if (result != 0) {
    SUPLA_LOG_ERROR("SERVER: certificate and private key do not match: -0x%04X",
                    -result);
    mbedtls_pk_free(&pk);
    mbedtls_x509_crt_free(&cert);
    return false;
  }

  mbedtls_pk_free(&pk);
  mbedtls_x509_crt_free(&cert);
  return true;
}

static bool validatePemHttpsCertificates(const uint8_t *serverCert,
                                         size_t serverCertLen,
                                         const uint8_t *prvtKey,
                                         size_t prvtKeyLen) {
  if (serverCert == nullptr || prvtKey == nullptr || serverCertLen == 0 ||
      prvtKeyLen == 0) {
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

  if (!startsWith(serverCert, serverCertLen, "-----BEGIN CERTIFICATE-----")) {
    SUPLA_LOG_WARNING("serverCert not valid, missing header");
    return false;
  }
  if (!startsWith(prvtKey, prvtKeyLen, "-----BEGIN PRIVATE KEY-----") &&
      !startsWith(prvtKey, prvtKeyLen, "-----BEGIN EC PRIVATE KEY-----") &&
      !startsWith(prvtKey, prvtKeyLen, "-----BEGIN RSA PRIVATE KEY-----")) {
    SUPLA_LOG_WARNING("prvtKey not valid, missing header");
    return false;
  }
  if (strstr(reinterpret_cast<const char *>(serverCert),
             "-----END CERTIFICATE-----") == nullptr) {
    SUPLA_LOG_WARNING("serverCert not valid, missing footer");
    return false;
  }
  if (strstr(reinterpret_cast<const char *>(prvtKey),
             "-----END PRIVATE KEY-----") == nullptr &&
      strstr(reinterpret_cast<const char *>(prvtKey),
             "-----END EC PRIVATE KEY-----") == nullptr &&
      strstr(reinterpret_cast<const char *>(prvtKey),
             "-----END RSA PRIVATE KEY-----") == nullptr) {
    SUPLA_LOG_WARNING("prvtKey not valid, missing footer");
    return false;
  }

  return validateHttpsCertificates(
      serverCert, serverCertLen, prvtKey, prvtKeyLen);
}

static bool looksLikePemPrivateKey(const uint8_t *prvtKey, size_t prvtKeyLen) {
  if (prvtKey == nullptr || prvtKeyLen == 0) {
    return false;
  }
  return startsWith(prvtKey, prvtKeyLen, "-----BEGIN PRIVATE KEY-----") ||
         startsWith(prvtKey, prvtKeyLen, "-----BEGIN EC PRIVATE KEY-----") ||
         startsWith(prvtKey, prvtKeyLen, "-----BEGIN RSA PRIVATE KEY-----");
}

static bool generateHttpsCertificates(char *serverCert,
                                      size_t serverCertLen,
                                      char *prvtKey,
                                      size_t prvtKeyLen) {
#if defined(MBEDTLS_X509_CRT_WRITE_C) && defined(MBEDTLS_PK_WRITE_C) && \
    defined(MBEDTLS_PSA_CRYPTO_C)
  if (serverCert == nullptr || prvtKey == nullptr) {
    return false;
  }

  psa_status_t status = psa_crypto_init();
  if (status != PSA_SUCCESS) {
    SUPLA_LOG_ERROR("SERVER: failed to initialize PSA crypto: %d",
                    static_cast<int>(status));
    return false;
  }

  psa_key_attributes_t attrs = PSA_KEY_ATTRIBUTES_INIT;
  psa_set_key_type(&attrs, PSA_KEY_TYPE_RSA_KEY_PAIR);
  psa_set_key_bits(&attrs, HTTPS_RSA_KEY_BITS);
  psa_set_key_usage_flags(&attrs,
                          PSA_KEY_USAGE_SIGN_HASH | PSA_KEY_USAGE_EXPORT);
  psa_set_key_algorithm(&attrs, PSA_ALG_RSA_PKCS1V15_SIGN(PSA_ALG_SHA_256));

  psa_key_id_t keyId = 0;
  status = psa_generate_key(&attrs, &keyId);
  psa_reset_key_attributes(&attrs);
  if (status != PSA_SUCCESS) {
    SUPLA_LOG_ERROR("SERVER: failed to generate PSA RSA key: %d",
                    static_cast<int>(status));
    return false;
  }

  mbedtls_pk_context pk = {};
  mbedtls_x509write_cert crt = {};
  mbedtls_pk_init(&pk);
  mbedtls_x509write_crt_init(&crt);

  int result =
      mbedtls_pk_copy_from_psa(static_cast<mbedtls_svc_key_id_t>(keyId), &pk);
  if (result != 0) {
    SUPLA_LOG_ERROR("SERVER: failed to wrap PSA RSA key: -0x%04X", -result);
    psa_destroy_key(keyId);
    return false;
  }

  result = mbedtls_pk_write_key_pem(
      &pk, reinterpret_cast<unsigned char *>(prvtKey), prvtKeyLen);
  if (result != 0) {
    SUPLA_LOG_ERROR("SERVER: failed to write private key PEM: -0x%04X",
                    -result);
    mbedtls_pk_free(&pk);
    psa_destroy_key(keyId);
    return false;
  }

  uint8_t serial[16] = {};
  Supla::fillRandom(serial, sizeof(serial));
  result = mbedtls_x509write_crt_set_serial_raw(&crt, serial, sizeof(serial));
  if (result != 0) {
    SUPLA_LOG_ERROR("SERVER: failed to set certificate serial: -0x%04X",
                    -result);
    mbedtls_x509write_crt_free(&crt);
    mbedtls_pk_free(&pk);
    psa_destroy_key(keyId);
    return false;
  }

  result = mbedtls_x509write_crt_set_issuer_name(&crt, "C=PL,O=SUPLA,CN=SUPLA");
  if (result != 0) {
    SUPLA_LOG_ERROR("SERVER: failed to set issuer name: -0x%04X", -result);
    mbedtls_x509write_crt_free(&crt);
    mbedtls_pk_free(&pk);
    psa_destroy_key(keyId);
    return false;
  }

  const char *deviceName = Supla::RegisterDevice::getName();
  char subjectName[128] = {};
  if (deviceName == nullptr || deviceName[0] == '\0') {
    deviceName = "SUPLA Local Web Server";
  }
  snprintf(subjectName, sizeof(subjectName), "C=PL,O=SUPLA,CN=%s", deviceName);
  result = mbedtls_x509write_crt_set_subject_name(&crt, subjectName);
  if (result != 0) {
    SUPLA_LOG_ERROR("SERVER: failed to set subject name: -0x%04X", -result);
    mbedtls_x509write_crt_free(&crt);
    mbedtls_pk_free(&pk);
    psa_destroy_key(keyId);
    return false;
  }

  mbedtls_x509write_crt_set_subject_key(&crt, &pk);
  mbedtls_x509write_crt_set_issuer_key(&crt, &pk);
  mbedtls_x509write_crt_set_md_alg(&crt, MBEDTLS_MD_SHA256);
  mbedtls_x509write_crt_set_validity(&crt, "20000101000000", "20991231235959");
  mbedtls_x509write_crt_set_basic_constraints(&crt, 0, -1);

  result = mbedtls_x509write_crt_pem(
      &crt, reinterpret_cast<unsigned char *>(serverCert), serverCertLen);
  if (result != 0) {
    SUPLA_LOG_ERROR("SERVER: failed to write server certificate PEM: -0x%04X",
                    -result);
    mbedtls_x509write_crt_free(&crt);
    mbedtls_pk_free(&pk);
    psa_destroy_key(keyId);
    return false;
  }

  mbedtls_x509write_crt_free(&crt);
  mbedtls_pk_free(&pk);
  psa_destroy_key(keyId);
  return true;
#else
#error "Missing menuconfig flags for certificate generation"
  (void)(serverCert);
  (void)(serverCertLen);
  (void)(prvtKey);
  (void)(prvtKeyLen);
  SUPLA_LOG_ERROR("SERVER: HTTPS certificate generation is not available");
  return false;
#endif
}

}  // namespace

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
        Supla::EspIdfSender sender(req,
                                   srvInst->getSendBufPtr(),
                                   Supla::SUPLA_HTML_OUTPUT_BUFFER_SIZE);
        srvInst->htmlGenerator->sendPage(
            &sender, srvInst->dataSaved, srvInst->isHttpsEnalbled());
      }
      srvInst->dataSaved = false;
    }
    return ESP_OK;
  } else if (req->method == HTTP_POST) {
    // request: POST /
    SUPLA_LOG_DEBUG("SERVER: post request");
    if (!srvInst->ensureAuthorized(req, sessionCookie, sizeof(sessionCookie))) {
      return srvInst->redirect(req, 303, srvInst->loginOrSetupUrl(), "main");
    }
    auto postResult = srvInst->handlePost(req);
    if (postResult == Supla::EspIdfWebServer::PostRequestResult::OK) {
      SUPLA_LOG_DEBUG("SERVER: post request handled OK, redirecting to /");
      auto redirectResult = srvInst->redirect(req, 303, "/", "deleted");
      SUPLA_LOG_DEBUG("SERVER: redirect result %d", redirectResult);
      if (redirectResult != ESP_OK) {
        httpd_resp_send_err(
            req, HTTPD_500_INTERNAL_SERVER_ERROR, "Redirect failed");
      }
      return ESP_OK;
    }
    switch (postResult) {
      case Supla::EspIdfWebServer::PostRequestResult::TIMEOUT:
        httpd_resp_send_err(req, HTTPD_408_REQ_TIMEOUT, "Request timeout");
        break;
      case Supla::EspIdfWebServer::PostRequestResult::CSRF_INVALID:
        httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Invalid CSRF token");
        break;
      case Supla::EspIdfWebServer::PostRequestResult::INVALID_REQUEST:
      default:
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid POST request");
        break;
    }
    return ESP_OK;
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
      Supla::EspIdfSender sender(
          req, srvInst->getSendBufPtr(), Supla::SUPLA_HTML_OUTPUT_BUFFER_SIZE);
      srvInst->htmlGenerator->sendLoginPage(&sender);
    }
    return ESP_OK;
  } else if (req->method == HTTP_POST) {
    SUPLA_LOG_DEBUG("SERVER: POST login request");

    if (srvInst->htmlGenerator) {
      bool loginResult =
          srvInst->login(req, nullptr, sessionCookie, sizeof(sessionCookie));
      SUPLA_LOG_DEBUG("SERVER: login result %d", loginResult);
      if (!loginResult &&
          !srvInst->ensureAuthorized(
              req, sessionCookie, sizeof(sessionCookie), true)) {
        srvInst->addSecurityLog(req, "Failed login attempt");
        SUPLA_LOG_DEBUG("SERVER: login failed, send login page");
        Supla::EspIdfSender sender(req,
                                   srvInst->getSendBufPtr(),
                                   Supla::SUPLA_HTML_OUTPUT_BUFFER_SIZE);
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

  char csrfToken[65] = {};
  if (!readCsrfTokenFromBody(req, csrfToken, sizeof(csrfToken)) ||
      !srvInst->isCsrfTokenValid(csrfToken)) {
    SUPLA_LOG_WARNING("Invalid CSRF token on logout request");
    httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Invalid CSRF token");
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
      Supla::EspIdfSender sender(
          req, srvInst->getSendBufPtr(), Supla::SUPLA_HTML_OUTPUT_BUFFER_SIZE);
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
          Supla::EspIdfSender sender(req,
                                     srvInst->getSendBufPtr(),
                                     Supla::SUPLA_HTML_OUTPUT_BUFFER_SIZE);
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
        Supla::EspIdfSender sender(req,
                                   srvInst->getSendBufPtr(),
                                   Supla::SUPLA_HTML_OUTPUT_BUFFER_SIZE);
        srvInst->htmlGenerator->sendLogsPage(&sender,
                                             srvInst->isHttpsEnalbled());
      }
      srvInst->dataSaved = false;
    }
    return ESP_OK;
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
        Supla::EspIdfSender sender(req,
                                   srvInst->getSendBufPtr(),
                                   Supla::SUPLA_HTML_OUTPUT_BUFFER_SIZE);
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
    auto postResult = srvInst->handlePost(req, true);
    if (postResult == Supla::EspIdfWebServer::PostRequestResult::OK) {
      SUPLA_LOG_DEBUG(
          "SERVER: beta post request handled OK, redirecting to /beta");
      auto redirectResult = srvInst->redirect(req, 303, "/beta", "deleted");
      SUPLA_LOG_DEBUG("SERVER: redirect result %d", redirectResult);
      if (redirectResult != ESP_OK) {
        httpd_resp_send_err(
            req, HTTPD_500_INTERNAL_SERVER_ERROR, "Redirect failed");
      }
      return ESP_OK;
    }
    switch (postResult) {
      case Supla::EspIdfWebServer::PostRequestResult::TIMEOUT:
        httpd_resp_send_err(req, HTTPD_408_REQ_TIMEOUT, "Request timeout");
        break;
      case Supla::EspIdfWebServer::PostRequestResult::CSRF_INVALID:
        httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Invalid CSRF token");
        break;
      case Supla::EspIdfWebServer::PostRequestResult::INVALID_REQUEST:
      default:
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid POST request");
        break;
    }
    return ESP_OK;
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

httpd_uri_t uriLogout = {.uri = "/logout",
                         .method = HTTP_POST,
                         .handler = logoutHandler,
                         .user_ctx = NULL};

httpd_uri_t uriLogin = {.uri = "/login",
                        .method = static_cast<httpd_method_t>(HTTP_ANY),
                        .handler = loginHandler,
                        .user_ctx = NULL};

httpd_uri_t uriSetup = {.uri = "/setup",
                        .method = static_cast<httpd_method_t>(HTTP_ANY),
                        .handler = setupHandler,
                        .user_ctx = NULL};

httpd_uri_t uriLogs = {.uri = "/logs",
                       .method = static_cast<httpd_method_t>(HTTP_ANY),
                       .handler = logsHandler,
                       .user_ctx = NULL};

/**
 * Custom URI matcher that will catch all URLs for http->https redirect
 */
bool uriMatchAll(const char *reference_uri,
                 const char *uri_to_match,
                 size_t match_upto) {
  return true;
}

esp_err_t redirectHandler(httpd_req_t *req) {
  char host[64] = {0};
  if (httpd_req_get_hdr_value_str(req, "Host", host, sizeof(host)) != ESP_OK) {
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

Supla::EspIdfWebServer::EspIdfWebServer(Supla::HtmlGenerator *generator,
                                        WebServerMode mode)
    : WebServer(generator), webServerMode(mode) {
  srvInst = this;
}

Supla::EspIdfWebServer::~EspIdfWebServer() {
  srvInst = nullptr;
  cleanupCerts();
}

void Supla::EspIdfWebServer::setWebServerMode(WebServerMode mode) {
  webServerMode = mode;
}

Supla::WebServer::WebServerMode Supla::EspIdfWebServer::getWebServerMode()
    const {
  return webServerMode;
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
    snprintf(
        buf,
        sizeof(buf),
        "redirect_to=%s; Path=/; HttpOnly; Secure; SameSite=Lax; Max-Age=%d",
        cookieRedirect,
        maxAge);
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

  if (resolveWebServerMode() == WebServerMode::HttpOnly) {
    // Plain HTTP mode keeps the legacy behavior without login.
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
      setSessionCookie(req, sessionCookie, sessionCookieLen);
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

Supla::EspIdfWebServer::PostRequestResult Supla::EspIdfWebServer::handlePost(
    httpd_req_t *req, bool beta) {
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
    if (ret <= 0) {
      SUPLA_LOG_WARNING("SERVER: failed to receive POST chunk: %d", ret);
      contentLen = 0;
      break;
    }
    content[ret] = '\0';
    SUPLA_LOG_DEBUG("SERVER: received %d B (cl %d)", ret, contentLen);
    contentLen -= ret;

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
      return PostRequestResult::TIMEOUT;
    }
    return PostRequestResult::INVALID_REQUEST;
  }

  if (csrfRejected || !csrfValidated) {
    resetParser();
    return csrfRejected ? PostRequestResult::CSRF_INVALID
                        : PostRequestResult::INVALID_REQUEST;
  }

  // call getHandler to generate config html page
  srvInst->dataSaved = true;
  resetParser();

  return PostRequestResult::OK;
}

Supla::WebServer::WebServerMode Supla::EspIdfWebServer::resolveWebServerMode()
    const {
  if (webServerMode != WebServerMode::Auto) {
    return webServerMode;
  }

  // Backward compatibility note:
  // - Older devices may not have a device-data partition and do not support
  //   HTTPS private-key decryption.
  // - Newer devices include device-data and must run with HTTPS in Auto mode.
  // Auto only falls back to HTTP when the partition is genuinely absent.
  auto cfg = Supla::Storage::ConfigInstance();
  return cfg && cfg->isDeviceDataPartitionDeclared()
             ? WebServerMode::HttpsOnly
             : WebServerMode::HttpOnly;
}

bool Supla::EspIdfWebServer::loadEmbeddedHttpsCertificates(bool storeActive) {
  if (embeddedServerCert == nullptr || embeddedPrvtKey == nullptr ||
      embeddedServerCertLen == 0 || embeddedPrvtKeyLen == 0) {
    SUPLA_LOG_WARNING("SERVER: embedded HTTPS certificates are missing");
    return false;
  }

  if (looksLikePemPrivateKey(embeddedPrvtKey, embeddedPrvtKeyLen)) {
    if (!validatePemHttpsCertificates(embeddedServerCert,
                                      embeddedServerCertLen,
                                      embeddedPrvtKey,
                                      embeddedPrvtKeyLen)) {
      return false;
    }
    (void)(storeActive);
    return true;
  }

  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg == nullptr) {
    SUPLA_LOG_WARNING("SERVER: config storage not available");
    return false;
  }

  uint8_t aesKey[33] = {};
  if (!cfg->getAESKey(aesKey)) {
    SUPLA_LOG_INFO("AES key not found");
    return false;
  }
  SUPLA_LOG_INFO("AES key found");

  if (embeddedPrvtKeyLen <= IV_SIZE) {
    SUPLA_LOG_WARNING("SERVER: embedded encrypted private key is too short");
    return false;
  }

  uint8_t iv[IV_SIZE] = {};
  memcpy(iv, embeddedPrvtKey, IV_SIZE);

  size_t encryptedLen = embeddedPrvtKeyLen - IV_SIZE;
  SUPLA_LOG_INFO("prvtKeyLen: %d, IV_SIZE: %d",
                 static_cast<int>(embeddedPrvtKeyLen),
                 IV_SIZE);
  uint8_t *encryptedData = new uint8_t[encryptedLen + 1];
  uint8_t *decryptedKey = new uint8_t[encryptedLen + 1];
  if (encryptedData == nullptr || decryptedKey == nullptr) {
    delete[] encryptedData;
    delete[] decryptedKey;
    SUPLA_LOG_ERROR("SERVER: failed to allocate embedded certificate buffers");
    return false;
  }

  memcpy(encryptedData, embeddedPrvtKey + IV_SIZE, encryptedLen);
  encryptedData[encryptedLen] = '\0';
  memset(decryptedKey, 0, encryptedLen + 1);

  int decryptResult =
      decryptAesCbc(aesKey, 32, iv, encryptedData, encryptedLen, decryptedKey);
  delete[] encryptedData;
  if (decryptResult != 0) {
    delete[] decryptedKey;
    SUPLA_LOG_ERROR("AES-CBC decrypt result: %d", decryptResult);
    return false;
  }

  if (!validatePemHttpsCertificates(embeddedServerCert,
                                    embeddedServerCertLen,
                                    decryptedKey,
                                    encryptedLen + 1)) {
    SUPLA_LOG_WARNING("SERVER: embedded HTTPS certificates are invalid");
    delete[] decryptedKey;
    return false;
  }

  if (!storeActive) {
    delete[] decryptedKey;
    return true;
  }

  bool stored = setActivePrivateKeyBuffer(decryptedKey,
                                          static_cast<int>(encryptedLen + 1));
  if (!stored) {
    delete[] decryptedKey;
    SUPLA_LOG_WARNING("SERVER: failed to store embedded HTTPS certificates");
    return false;
  }
  return true;
}

bool Supla::EspIdfWebServer::verifyEmbeddedHttpsCertificates() {
  return loadEmbeddedHttpsCertificates(false);
}

bool Supla::EspIdfWebServer::loadHttpsCertificatesFromStorage() {
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg == nullptr) {
    SUPLA_LOG_WARNING("SERVER: config storage not available");
    return false;
  }

  int serverCertSize = cfg->getStringSize(HTTPS_CERT_KEY);
  int prvtKeySize = cfg->getStringSize(HTTPS_KEY_KEY);
  if (serverCertSize <= 0 || prvtKeySize <= 0) {
    SUPLA_LOG_WARNING("SERVER: stored HTTPS certificates not found");
    return false;
  }
  if (serverCertSize > static_cast<int>(UINT16_MAX) ||
      prvtKeySize > static_cast<int>(UINT16_MAX)) {
    SUPLA_LOG_ERROR("SERVER: stored HTTPS certificates are too large");
    return false;
  }

  uint8_t *serverCertBuf = new uint8_t[serverCertSize];
  uint8_t *prvtKeyBuf = new uint8_t[prvtKeySize];
  if (serverCertBuf == nullptr || prvtKeyBuf == nullptr) {
    delete[] serverCertBuf;
    delete[] prvtKeyBuf;
    SUPLA_LOG_ERROR("SERVER: failed to allocate HTTPS certificate buffers");
    return false;
  }

  bool result =
      cfg->getString(HTTPS_CERT_KEY,
                     reinterpret_cast<char *>(serverCertBuf),
                     serverCertSize) &&
      cfg->getString(
          HTTPS_KEY_KEY, reinterpret_cast<char *>(prvtKeyBuf), prvtKeySize);
  if (!result) {
    SUPLA_LOG_WARNING("SERVER: failed to read stored HTTPS certificates");
    delete[] serverCertBuf;
    delete[] prvtKeyBuf;
    return false;
  }

  if (!looksLikePemPrivateKey(prvtKeyBuf, prvtKeySize)) {
    SUPLA_LOG_WARNING("SERVER: stored HTTPS private key is not PEM");
    delete[] serverCertBuf;
    delete[] prvtKeyBuf;
    return false;
  }

  if (!validatePemHttpsCertificates(serverCertBuf,
                                    static_cast<size_t>(serverCertSize),
                                    prvtKeyBuf,
                                    static_cast<size_t>(prvtKeySize))) {
    SUPLA_LOG_WARNING("SERVER: stored HTTPS certificates are invalid");
    delete[] serverCertBuf;
    delete[] prvtKeyBuf;
    return false;
  }

  bool loaded = setActiveCertificateBuffers(serverCertBuf,
                                            static_cast<int>(serverCertSize),
                                            prvtKeyBuf,
                                            static_cast<int>(prvtKeySize));
  if (!loaded) {
    delete[] serverCertBuf;
    delete[] prvtKeyBuf;
  }

  return loaded;
}

bool Supla::EspIdfWebServer::generateHttpsCertificates() {
  uint8_t *serverCertBuf = new uint8_t[HTTPS_CERT_BUFFER_SIZE];
  uint8_t *prvtKeyBuf = new uint8_t[HTTPS_KEY_BUFFER_SIZE];
  if (serverCertBuf == nullptr || prvtKeyBuf == nullptr) {
    delete[] serverCertBuf;
    delete[] prvtKeyBuf;
    SUPLA_LOG_ERROR("SERVER: failed to allocate temporary HTTPS buffers");
    return false;
  }
  memset(serverCertBuf, 0, HTTPS_CERT_BUFFER_SIZE);
  memset(prvtKeyBuf, 0, HTTPS_KEY_BUFFER_SIZE);

  bool result =
      ::generateHttpsCertificates(reinterpret_cast<char *>(serverCertBuf),
                                  HTTPS_CERT_BUFFER_SIZE,
                                  reinterpret_cast<char *>(prvtKeyBuf),
                                  HTTPS_KEY_BUFFER_SIZE);
  if (!result) {
    delete[] serverCertBuf;
    delete[] prvtKeyBuf;
    return false;
  }

  bool stored = setActiveCertificateBuffers(
      serverCertBuf,
      strlen(reinterpret_cast<char *>(serverCertBuf)) + 1,
      prvtKeyBuf,
      strlen(reinterpret_cast<char *>(prvtKeyBuf)) + 1);
  if (!stored) {
    delete[] serverCertBuf;
    delete[] prvtKeyBuf;
  }

  if (!stored || this->serverCert == nullptr || this->prvtKey == nullptr) {
    SUPLA_LOG_ERROR(
        "SERVER: generated HTTPS certificates could not be stored in memory");
    return false;
  }

  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    bool saved =
        cfg->setString(HTTPS_CERT_KEY,
                       reinterpret_cast<const char *>(serverCert)) &&
        cfg->setString(HTTPS_KEY_KEY, reinterpret_cast<const char *>(prvtKey));
    if (saved) {
      cfg->commit();
      SUPLA_LOG_INFO("SERVER: generated and stored new HTTPS certificates");
    } else {
      SUPLA_LOG_WARNING(
          "SERVER: generated HTTPS certificates but failed to store them");
    }
  } else {
    SUPLA_LOG_WARNING(
        "SERVER: generated HTTPS certificates without config storage");
  }

  return validatePemHttpsCertificates(
      serverCert, serverCertLen, prvtKey, prvtKeyLen);
}

bool Supla::EspIdfWebServer::ensureHttpsCertificates() {
  cleanupCerts();

  if (loadEmbeddedHttpsCertificates()) {
    return true;
  }

  if (loadHttpsCertificatesFromStorage()) {
    return true;
  }

  SUPLA_LOG_WARNING("SERVER: HTTPS certificates are missing or invalid");
  SUPLA_LOG_WARNING("SERVER: generating replacement HTTPS certificates");
  return generateHttpsCertificates();
}

void Supla::EspIdfWebServer::start() {
  if (serverHttps || serverHttp) {
    return;
  }

  SUPLA_LOG_INFO("Starting local web server");

  WebServerMode activeMode = resolveWebServerMode();

  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.lru_purge_enable = true;
  config.max_open_sockets = 2;
  config.stack_size = 6144;

  if (activeMode == WebServerMode::HttpOnly) {
    SUPLA_LOG_INFO("SERVER: starting local web server in HTTP mode");
    if (httpd_start(&serverHttp, &config) == ESP_OK) {
      httpd_register_uri_handler(serverHttp, &uriRoot);
      httpd_register_uri_handler(serverHttp, &uriBeta);
      httpd_register_uri_handler(serverHttp, &uriFavicon);
      // for http we do not have login/logout and setup currently
      //      httpd_register_uri_handler(serverHttp, &uriLogin);
      //      httpd_register_uri_handler(serverHttp, &uriLogout);
      //      httpd_register_uri_handler(serverHttp, &uriSetup);
    } else {
      SUPLA_LOG_ERROR("Failed to start local http web server");
    }
  } else {
    if (!ensureHttpsCertificates()) {
      SUPLA_LOG_ERROR("Failed to prepare HTTPS certificates");
    } else {
      const uint8_t *httpsServerCert =
          serverCert != nullptr ? serverCert : embeddedServerCert;
      uint16_t httpsServerCertLen =
          serverCert != nullptr ? serverCertLen : embeddedServerCertLen;
      const uint8_t *httpsPrvtKey =
          prvtKey != nullptr ? prvtKey : embeddedPrvtKey;
      uint16_t httpsPrvtKeyLen =
          prvtKey != nullptr ? prvtKeyLen : embeddedPrvtKeyLen;

      if (httpsServerCert == nullptr || httpsPrvtKey == nullptr ||
          httpsServerCertLen == 0 || httpsPrvtKeyLen == 0) {
        SUPLA_LOG_ERROR("Failed to select HTTPS certificates for startup");
      } else {
        httpd_ssl_config_t configHttps = HTTPD_SSL_CONFIG_DEFAULT();
        configHttps.httpd.lru_purge_enable = true;
        configHttps.httpd.max_open_sockets = 5;
        configHttps.httpd.max_uri_handlers = 7;

        configHttps.servercert = httpsServerCert;
        configHttps.servercert_len = httpsServerCertLen;

        configHttps.prvtkey_pem = httpsPrvtKey;
        configHttps.prvtkey_len = httpsPrvtKeyLen;
        delay(100);
        if (httpd_ssl_start(&serverHttps, &configHttps) == ESP_OK) {
          httpd_register_uri_handler(serverHttps, &uriRoot);
          httpd_register_uri_handler(serverHttps, &uriBeta);
          httpd_register_uri_handler(serverHttps, &uriFavicon);
          httpd_register_uri_handler(serverHttps, &uriLogin);
          httpd_register_uri_handler(serverHttps, &uriLogout);
          httpd_register_uri_handler(serverHttps, &uriSetup);
          httpd_register_uri_handler(serverHttps, &uriLogs);

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
    }
  }

  if (!sendBuf) {
    sendBuf = new char[Supla::SUPLA_HTML_OUTPUT_BUFFER_SIZE];
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
  if (sendBuf) {
    delete[] sendBuf;
    sendBuf = nullptr;
  }
}

Supla::EspIdfSender::EspIdfSender(httpd_req_t *req,
                                  char *sendBuf,
                                  int sendBufLen)
    : reqHandler(req), outputBuffer(sendBuf, sendBufLen) {
}

bool Supla::EspIdfWebServer::isHttpsEnalbled() const {
  return serverHttps != nullptr;
}

Supla::EspIdfSender::~EspIdfSender() {
  if (reqHandler && !outputBuffer.error()) {
    outputBuffer.flush(reqHandler, &EspIdfSender::flushChunk);
    httpd_resp_send_chunk(reqHandler, nullptr, 0);
  }
}

bool Supla::EspIdfSender::flushChunk(void *context, const char *buf, int size) {
  if (context == nullptr || buf == nullptr || size < 0) {
    return false;
  }
  auto *req = reinterpret_cast<httpd_req_t *>(context);
  return httpd_resp_send_chunk(req, buf, size) == ESP_OK;
}

void Supla::EspIdfSender::send(const char *buf, int size) {
  if (!buf || !reqHandler) {
    return;
  }
  outputBuffer.send(reqHandler, &EspIdfSender::flushChunk, buf, size);
}

void Supla::EspIdfWebServer::cleanupCerts() {
  if (this->serverCert) {
    delete[] this->serverCert;
  }
  if (this->prvtKey) {
    delete[] this->prvtKey;
  }
  serverCert = nullptr;
  prvtKey = nullptr;
  serverCertLen = 0;
  prvtKeyLen = 0;
}

bool Supla::EspIdfWebServer::setActiveCertificateBuffers(uint8_t *serverCert,
                                                         int serverCertLen,
                                                         uint8_t *prvtKey,
                                                         int prvtKeyLen) {
  if (serverCert == nullptr || prvtKey == nullptr || serverCertLen <= 0 ||
      prvtKeyLen <= 0) {
    SUPLA_LOG_ERROR("Failed to set active server certificate or private key");
    return false;
  }
  if (serverCertLen > static_cast<int>(UINT16_MAX) ||
      prvtKeyLen > static_cast<int>(UINT16_MAX)) {
    SUPLA_LOG_ERROR("Active server certificate or private key is too large");
    return false;
  }

  SUPLA_LOG_INFO("Server certificate length: %d, private key length: %d",
                 serverCertLen,
                 prvtKeyLen);

  cleanupCerts();
  this->serverCert = serverCert;
  this->prvtKey = prvtKey;
  this->serverCertLen = static_cast<uint16_t>(serverCertLen);
  this->prvtKeyLen = static_cast<uint16_t>(prvtKeyLen);
  return true;
}

bool Supla::EspIdfWebServer::setActivePrivateKeyBuffer(uint8_t *prvtKey,
                                                       int prvtKeyLen) {
  if (prvtKey == nullptr || prvtKeyLen <= 0) {
    SUPLA_LOG_ERROR("Failed to set active private key");
    return false;
  }
  if (prvtKeyLen > static_cast<int>(UINT16_MAX)) {
    SUPLA_LOG_ERROR("Active private key is too large");
    return false;
  }

  cleanupCerts();
  this->prvtKey = prvtKey;
  this->prvtKeyLen = static_cast<uint16_t>(prvtKeyLen);
  return true;
}

void Supla::EspIdfWebServer::setServerCertificate(const uint8_t *serverCert,
                                                  int serverCertLen,
                                                  const uint8_t *prvtKey,
                                                  int prvtKeyLen) {
  if (serverCert == nullptr || prvtKey == nullptr || serverCertLen <= 0 ||
      prvtKeyLen <= 0) {
    SUPLA_LOG_ERROR("Failed to set embedded server certificate or private key");
    return;
  }
  if (serverCertLen > static_cast<int>(UINT16_MAX) ||
      prvtKeyLen > static_cast<int>(UINT16_MAX)) {
    SUPLA_LOG_ERROR("Embedded server certificate or private key is too large");
    return;
  }

  embeddedServerCert = serverCert;
  embeddedPrvtKey = prvtKey;
  embeddedServerCertLen = static_cast<uint16_t>(serverCertLen);
  embeddedPrvtKeyLen = static_cast<uint16_t>(prvtKeyLen);
  SUPLA_LOG_INFO("Embedded server certificate pointers updated: cert=%p key=%p",
                 static_cast<const void *>(embeddedServerCert),
                 static_cast<const void *>(embeddedPrvtKey));
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

bool Supla::EspIdfWebServer::login(httpd_req_t *req,
                                   const char *password,
                                   char *sessionCookie,
                                   int sessionCookieLen) {
  reloadSaltPassword();
  bool passwordIsCorrect = false;
  if (isPasswordConfigured()) {
    if (password == nullptr) {
      // check if password is send in form
      char buf[256] = {};
      size_t remaining = req->content_len;

      if (remaining >= sizeof(buf)) {
        SUPLA_LOG_INFO("Setup request too large");
        return false;
      }

      size_t offset = 0;
      while (remaining > 0) {
        int ret = httpd_req_recv(req, buf + offset, remaining);
        if (ret <= 0) {
          SUPLA_LOG_INFO("Failed to read setup request");
          return false;
        }
        offset += ret;
        remaining -= ret;
      }

      buf[offset] = '\0';
#if SUPLA_CSRF_LOGIN_SETUP_PROTECTION
      char csrfToken[65] = {};
      httpd_query_key_value(buf, "csrf", csrfToken, sizeof(csrfToken));
      if (!isCsrfTokenValid(csrfToken)) {
        SUPLA_LOG_WARNING("Invalid CSRF token on login request");
        return false;
      }
#endif
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
      setSessionCookie(req, sessionCookie, sessionCookieLen);
      SUPLA_LOG_INFO("Login successful");
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
           "session=%s|%s; Path=/; HttpOnly; Secure; SameSite=Lax",
           timestampStr,
           sessionHmacHex);
  httpd_resp_set_hdr(req, "Set-Cookie", buf);
}

void Supla::EspIdfWebServer::handleLogout(httpd_req_t *req) {
  // delete session cookie
  httpd_resp_set_hdr(
      req,
      "Set-Cookie",
      "session=deleted; Path=/; HttpOnly; Secure; SameSite=Lax; Max-Age=0");
}

Supla::SetupRequestResult Supla::EspIdfWebServer::handleSetup(
    httpd_req_t *req, char *sessionCookie, int sessionCookieLen) {
  char buf[256] = {};
  int ret = 0;
  size_t remaining = req->content_len;

  if (remaining >= sizeof(buf)) {
    SUPLA_LOG_INFO("Setup request too large");
    return SetupRequestResult::INVALID_REQUEST;
  }

  size_t offset = 0;
  while (remaining > 0) {
    ret = httpd_req_recv(req, buf + offset, remaining);
    if (ret <= 0) {
      SUPLA_LOG_INFO("Failed to read setup request");
      return SetupRequestResult::INVALID_REQUEST;
    }
    offset += ret;
    remaining -= ret;
  }

  buf[offset] = '\0';
  if (ret <= 0) {
    return SetupRequestResult::INVALID_REQUEST;
  }

#if SUPLA_CSRF_LOGIN_SETUP_PROTECTION
  char csrfToken[65] = {};
  httpd_query_key_value(buf, "csrf", csrfToken, sizeof(csrfToken));
  if (!isCsrfTokenValid(csrfToken)) {
    SUPLA_LOG_WARNING("Invalid CSRF token on setup request");
    return SetupRequestResult::INVALID_REQUEST;
  }
#endif

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
    addSecurityLog(req, "Password change failed: password too weak");
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
#if defined(CONFIG_LWIP_IPV6) && CONFIG_LWIP_IPV6
  struct sockaddr_storage addr = {};
  socklen_t addr_len = sizeof(addr);
  if (getpeername(
          sockfd, reinterpret_cast<struct sockaddr *>(&addr), &addr_len) == 0) {
    if (addr.ss_family == AF_INET6) {
      auto *addr6 = reinterpret_cast<struct sockaddr_in6 *>(&addr);
      ip = ntohl(addr6->sin6_addr.un.u32_addr[3]);
    } else if (addr.ss_family == AF_INET) {
      auto *addr4 = reinterpret_cast<struct sockaddr_in *>(&addr);
      ip = ntohl(addr4->sin_addr.s_addr);
    }
  }
#else
  struct sockaddr_in addr = {};
  socklen_t addr_len = sizeof(addr);
  if (getpeername(
          sockfd, reinterpret_cast<struct sockaddr *>(&addr), &addr_len) == 0) {
    ip = ntohl(addr.sin_addr.s_addr);
  }
#endif

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

char *Supla::EspIdfWebServer::getSendBufPtr() const {
  return sendBuf;
}

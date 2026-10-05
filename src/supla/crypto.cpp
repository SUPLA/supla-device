// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "crypto.h"

#if defined(ESP32) || defined(SUPLA_DEVICE_ESP32)
#include <mbedtls/error.h>
#if defined(__has_include)
#if __has_include(<mbedtls/pkcs5.h>)
#define SUPLA_HAVE_MBEDTLS_PKCS5 1
#include <mbedtls/pkcs5.h>
#elif __has_include(<psa/crypto.h>)
#define SUPLA_HAVE_PSA_CRYPTO 1
#include <psa/crypto.h>
#endif
#endif
#if !defined(SUPLA_HAVE_MBEDTLS_PKCS5) && !defined(SUPLA_HAVE_PSA_CRYPTO)
#define SUPLA_HAVE_MBEDTLS_PKCS5 1
#include <mbedtls/pkcs5.h>
#endif
#endif

#include <supla/log_wrapper.h>
#include <supla/tools.h>
#include <string.h>
#include <limits.h>

#if defined(ESP32) || defined(SUPLA_DEVICE_ESP32) || defined(ESP_PLATFORM)
#include <mbedtls/md.h>
#include <esp_random.h>
#elif defined(ARDUINO_ARCH_ESP8266) || defined(ESP8266)
#include <Esp.h>
#include <bearssl/bearssl.h>
#elif defined(SUPLA_LINUX) || defined(SUPLA_TEST)
#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/params.h>
#include <openssl/rand.h>
#endif

bool Supla::Crypto::pbkdf2Sha256(const char *password,
                                 const uint8_t *salt,
                                 size_t saltLen,
                                 uint32_t iterations,
                                 uint8_t *derivedKey,
                                 size_t derivedKeyLen) {
#if defined(ESP32) || defined(SUPLA_DEVICE_ESP32)
#if defined(SUPLA_HAVE_MBEDTLS_PKCS5)
  int ret = mbedtls_pkcs5_pbkdf2_hmac_ext(
      MBEDTLS_MD_SHA256,
      reinterpret_cast<const unsigned char *>(password),
      strlen(password),
      salt,
      saltLen,
      iterations,
      derivedKeyLen,
      derivedKey);
  if (ret != 0) {
    char errBuf[100];
    mbedtls_strerror(ret, errBuf, sizeof(errBuf));
    SUPLA_LOG_ERROR("PBKDF2 error: %s", errBuf);
    return false;
  }

  return true;
#elif defined(SUPLA_HAVE_PSA_CRYPTO)
  psa_status_t status = psa_crypto_init();
  if (status != PSA_SUCCESS) {
    SUPLA_LOG_ERROR("PBKDF2 error: PSA init status %d", status);
    return false;
  }

  psa_key_derivation_operation_t op = PSA_KEY_DERIVATION_OPERATION_INIT;

  status = psa_key_derivation_setup(
      &op, PSA_ALG_PBKDF2_HMAC(PSA_ALG_SHA_256));
  if (status != PSA_SUCCESS) {
    SUPLA_LOG_ERROR("PBKDF2 error: setup status %d", status);
    psa_key_derivation_abort(&op);
    return false;
  }

  status = psa_key_derivation_input_integer(
      &op, PSA_KEY_DERIVATION_INPUT_COST, iterations);
  if (status != PSA_SUCCESS) {
    SUPLA_LOG_ERROR("PBKDF2 error: input COST status %d", status);
    psa_key_derivation_abort(&op);
    return false;
  }

  status = psa_key_derivation_input_bytes(
      &op, PSA_KEY_DERIVATION_INPUT_SALT, salt, saltLen);
  if (status != PSA_SUCCESS) {
    SUPLA_LOG_ERROR("PBKDF2 error: input SALT status %d", status);
    psa_key_derivation_abort(&op);
    return false;
  }

  status = psa_key_derivation_input_bytes(
      &op,
      PSA_KEY_DERIVATION_INPUT_PASSWORD,
      reinterpret_cast<const uint8_t *>(password),
      strlen(password));
  if (status != PSA_SUCCESS) {
    SUPLA_LOG_ERROR("PBKDF2 error: input PASSWORD status %d", status);
    psa_key_derivation_abort(&op);
    return false;
  }

  status = psa_key_derivation_set_capacity(&op, derivedKeyLen);
  if (status != PSA_SUCCESS) {
    SUPLA_LOG_ERROR("PBKDF2 error: set capacity status %d", status);
    psa_key_derivation_abort(&op);
    return false;
  }

  status = psa_key_derivation_output_bytes(
      &op, derivedKey, derivedKeyLen);
  if (status != PSA_SUCCESS) {
    SUPLA_LOG_ERROR("PBKDF2 error: output bytes status %d", status);
    psa_key_derivation_abort(&op);
    return false;
  }

  psa_key_derivation_abort(&op);
  return true;
#else
  (void)(password);
  (void)(salt);
  (void)(saltLen);
  (void)(iterations);
  (void)(derivedKey);
  (void)(derivedKeyLen);
  SUPLA_LOG_ERROR("PBKDF2-SHA256 not implemented for this platform");
  return false;
#endif
#else
  (void)(password);
  (void)(salt);
  (void)(saltLen);
  (void)(iterations);
  (void)(derivedKey);
  (void)(derivedKeyLen);
  SUPLA_LOG_ERROR("PBKDF2-SHA256 not implemented for this platform");
  return false;
#endif
}


bool Supla::Crypto::hmacSha256(const uint8_t *key, size_t keyLen,
                               const uint8_t *data, size_t dataLen,
                               uint8_t output[32]) {
  if ((!key && keyLen) || (!data && dataLen) || !output) {
    return false;
  }
  static const uint8_t empty = 0;
  if (!key) {
    key = &empty;
  }
  if (!data) {
    data = &empty;
  }
#if defined(ESP32) || defined(SUPLA_DEVICE_ESP32) || defined(ESP_PLATFORM)
  const mbedtls_md_info_t *info =
      mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  return info && mbedtls_md_hmac(info, key, keyLen, data, dataLen, output) == 0;
#elif defined(ARDUINO_ARCH_ESP8266) || defined(ESP8266)
  br_hmac_key_context keyContext;
  br_hmac_context context;
  br_hmac_key_init(&keyContext, &br_sha256_vtable, key, keyLen);
  br_hmac_init(&context, &keyContext, 32);
  if (dataLen) {
    br_hmac_update(&context, data, dataLen);
  }
  bool result = br_hmac_out(&context, output) == 32;
  memset(&keyContext, 0, sizeof(keyContext));
  memset(&context, 0, sizeof(context));
  return result;
#elif defined(SUPLA_LINUX) || defined(SUPLA_TEST)
  EVP_MAC *mac = EVP_MAC_fetch(nullptr, "HMAC", nullptr);
  if (!mac) {
    return false;
  }
  EVP_MAC_CTX *context = EVP_MAC_CTX_new(mac);
  EVP_MAC_free(mac);
  if (!context) {
    return false;
  }
  char digestName[] = "SHA256";
  OSSL_PARAM params[] = {
      OSSL_PARAM_construct_utf8_string(OSSL_MAC_PARAM_DIGEST, digestName, 0),
      OSSL_PARAM_construct_end(),
  };
  size_t outputLength = 0;
  bool result = EVP_MAC_init(context, key, keyLen, params) == 1 &&
      EVP_MAC_update(context, data, dataLen) == 1 &&
      EVP_MAC_final(context, output, &outputLength, 32) == 1 &&
      outputLength == 32;
  EVP_MAC_CTX_free(context);
  return result;
#else
  return false;
#endif
}

bool Supla::Crypto::fillRandom(uint8_t *buffer, size_t size) {
  if ((!buffer && size) || size > INT_MAX) {
    return false;
  }
  if (!size) {
    return true;
  }
#if defined(ESP32) || defined(SUPLA_DEVICE_ESP32) || defined(ESP_PLATFORM)
  esp_fill_random(buffer, size);
  return true;
#elif defined(ARDUINO_ARCH_ESP8266) || defined(ESP8266)
  ESP.random(buffer, size);
  return true;
#elif defined(SUPLA_LINUX) || defined(SUPLA_TEST)
  return RAND_bytes(buffer, static_cast<int>(size)) == 1;
#else
  return false;
#endif
}

bool Supla::Crypto::hmacSha256Hex(const char *key, size_t keyLen,
                                  const char *data, size_t dataLen,
                                  char *output, size_t outputLen) {
  if (outputLen < 65 || !key || !data || !output) {
    return false;
  }
  uint8_t raw[32] = {};
  if (!hmacSha256(reinterpret_cast<const uint8_t *>(key), keyLen,
                  reinterpret_cast<const uint8_t *>(data), dataLen, raw)) {
    return false;
  }
  generateHexString(raw, output, sizeof(raw));
  return true;
}

// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "sha256.h"

#include <string.h>
#include <limits.h>
#include <new>

#if defined(ESP32) || defined(SUPLA_DEVICE_ESP32) || defined(ESP_PLATFORM)

#include <mbedtls/md.h>

Supla::Sha256::Sha256() : ctx(nullptr) {
  mbedtls_md_context_t *mdCtx = new (std::nothrow) mbedtls_md_context_t();
  if (!mdCtx) {
    return;
  }
  mbedtls_md_init(mdCtx);
  const mbedtls_md_info_t *mdInfo =
      mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  if (mdInfo == nullptr ||
      mbedtls_md_setup(mdCtx, mdInfo, 0) != 0 ||
      mbedtls_md_starts(mdCtx) != 0) {
    mbedtls_md_free(mdCtx);
    delete mdCtx;
    return;
  }
  ctx = mdCtx;
}

Supla::Sha256::~Sha256() {
  if (ctx == nullptr) {
    return;
  }
  mbedtls_md_context_t *mdCtx =
      static_cast<mbedtls_md_context_t *>(ctx);
  mbedtls_md_free(mdCtx);
  delete mdCtx;
  ctx = nullptr;
}

bool Supla::Sha256::update(const uint8_t *data, const int size) {
  if (ctx == nullptr || size < 0 || (data == nullptr && size)) {
    return false;
  }
  mbedtls_md_context_t *mdCtx =
      static_cast<mbedtls_md_context_t *>(ctx);
  return size == 0 || mbedtls_md_update(mdCtx, data, size) == 0;
}

bool Supla::Sha256::digest(uint8_t *output, int length) {
  if (ctx == nullptr || output == nullptr || length <= 0) {
    return false;
  }
  uint8_t fullDigest[32] = {};
  mbedtls_md_context_t tmp;
  mbedtls_md_init(&tmp);
  const mbedtls_md_info_t *mdInfo =
      mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  if (mdInfo == nullptr ||
      mbedtls_md_setup(&tmp, mdInfo, 0) != 0 ||
      mbedtls_md_clone(&tmp,
          static_cast<const mbedtls_md_context_t *>(ctx)) != 0 ||
      mbedtls_md_finish(&tmp, fullDigest) != 0) {
    mbedtls_md_free(&tmp);
    return false;
  }
  mbedtls_md_free(&tmp);

  if (length > 32) {
    length = 32;
  }
  memcpy(output, fullDigest, length);
  return true;
}

#elif defined(ESP8266) || defined(ARDUINO_ARCH_ESP8266)

#include <bearssl/bearssl_hash.h>

Supla::Sha256::Sha256() : ctx(nullptr) {
  br_sha256_context *shaCtx = new (std::nothrow) br_sha256_context();
  if (!shaCtx) {
    return;
  }
  br_sha256_init(shaCtx);
  ctx = shaCtx;
}

Supla::Sha256::~Sha256() {
  delete static_cast<br_sha256_context *>(ctx);
  ctx = nullptr;
}

bool Supla::Sha256::update(const uint8_t *data, const int size) {
  if (ctx == nullptr || size < 0 || (data == nullptr && size)) {
    return false;
  }
  if (size) {
    br_sha256_update(static_cast<br_sha256_context *>(ctx), data, size);
  }
  return true;
}

bool Supla::Sha256::digest(uint8_t *output, int length) {
  if (ctx == nullptr || output == nullptr || length <= 0) {
    return false;
  }

  uint8_t fullDigest[32] = {};
  br_sha256_context tmp = *static_cast<br_sha256_context *>(ctx);
  br_sha256_out(&tmp, fullDigest);

  if (length > 32) {
    length = 32;
  }
  memcpy(output, fullDigest, length);
  return true;
}

#elif defined(SUPLA_LINUX) || defined(SUPLA_TEST)

#include <openssl/evp.h>

Supla::Sha256::Sha256() : ctx(nullptr) {
  EVP_MD_CTX *mdCtx = EVP_MD_CTX_new();
  if (mdCtx == nullptr) {
    return;
  }
  if (EVP_DigestInit_ex(mdCtx, EVP_sha256(), nullptr) != 1) {
    EVP_MD_CTX_free(mdCtx);
    return;
  }
  ctx = mdCtx;
}

Supla::Sha256::~Sha256() {
  EVP_MD_CTX_free(static_cast<EVP_MD_CTX *>(ctx));
  ctx = nullptr;
}

bool Supla::Sha256::update(const uint8_t *data, const int size) {
  if (ctx == nullptr || size < 0 || (data == nullptr && size)) {
    return false;
  }
  return size == 0 ||
      EVP_DigestUpdate(static_cast<EVP_MD_CTX *>(ctx), data, size) == 1;
}

bool Supla::Sha256::digest(uint8_t *output, int length) {
  if (ctx == nullptr || output == nullptr || length <= 0) {
    return false;
  }

  uint8_t fullDigest[EVP_MAX_MD_SIZE] = {};
  unsigned int fullDigestLength = 0;
  EVP_MD_CTX *tmp = EVP_MD_CTX_new();
  if (tmp == nullptr) {
    return false;
  }
  if (EVP_MD_CTX_copy_ex(tmp, static_cast<EVP_MD_CTX *>(ctx)) != 1 ||
      EVP_DigestFinal_ex(tmp, fullDigest, &fullDigestLength) != 1) {
    EVP_MD_CTX_free(tmp);
    return false;
  }
  EVP_MD_CTX_free(tmp);

  if (length > static_cast<int>(fullDigestLength)) {
    length = static_cast<int>(fullDigestLength);
  }
  memcpy(output, fullDigest, length);
  return true;
}

#endif  // ESP32/SUPLA_DEVICE_ESP32/ESP8266/SUPLA_LINUX

#if defined(SUPLA_TEST) || defined(ESP32) || defined(SUPLA_DEVICE_ESP32) || \
    defined(ESP8266) || defined(ARDUINO_ARCH_ESP8266) || \
    defined(SUPLA_LINUX) || defined(ESP_PLATFORM)
bool Supla::Sha256::calculate(const uint8_t *data, size_t size,
                               uint8_t output[32]) {
  if ((!data && size) || !output || size > INT_MAX) {
    return false;
  }
  Sha256 hash;
  return hash.isValid() && hash.update(data, static_cast<int>(size)) &&
      hash.digest(output);
}
#endif

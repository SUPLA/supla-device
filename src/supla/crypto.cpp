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

#include "crypto.h"

#if defined(ESP32) || defined(SUPLA_DEVICE_ESP32)
#include <mbedtls/error.h>
#include <mbedtls/pkcs5.h>
#endif

#include <supla/log_wrapper.h>
#include <supla/tools.h>
#include <string.h>

bool Supla::Crypto::pbkdf2Sha256(const char *password,
                                 const uint8_t *salt,
                                 size_t saltLen,
                                 uint32_t iterations,
                                 uint8_t *derivedKey,
                                 size_t derivedKeyLen) {
#if defined(ESP32) || defined(SUPLA_DEVICE_ESP32)
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


bool Supla::Crypto::hmacSha256Hex(const char *key, size_t keyLen,
                                  const char *data, size_t dataLen,
                                  char *output, size_t outputLen) {
  if (outputLen < 65) {
    SUPLA_LOG_ERROR("HMAC-SHA256 output length must be 65 bytes");
    return false;
  }
  if (key == nullptr || data == nullptr || output == nullptr) {
    return false;
  }
#if defined(ESP32) || defined(SUPLA_DEVICE_ESP32)
  uint8_t outputRaw[32] = {};
  const mbedtls_md_info_t *mdInfo =
      mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  mbedtls_md_hmac(mdInfo,
                  reinterpret_cast<const unsigned char *>(key),
                  keyLen,
                  reinterpret_cast<const unsigned char *>(data),
                  dataLen,
                  outputRaw);

  generateHexString(outputRaw, output, 32);
  return true;
#else
  (void)(key);
  (void)(keyLen);
  (void)(data);
  (void)(dataLen);
  (void)(output);
  (void)(outputLen);
  SUPLA_LOG_ERROR("HMAC-SHA256 not implemented for this platform");
  return false;
#endif
}


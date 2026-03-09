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

#ifndef SUPLA_TEST

#include "rsa_verificator.h"

#if defined(ESP32) || defined(SUPLA_DEVICE_ESP32)

#include <mbedtls/rsa.h>
#include <mbedtls/version.h>

Supla::RsaVerificator::RsaVerificator(const uint8_t *publicKeyBytes)
    : rsa_ctx(nullptr), ready(false) {
  if (publicKeyBytes == nullptr) {
    return;
  }
  mbedtls_rsa_context *rsa = new mbedtls_rsa_context();
#if MBEDTLS_VERSION_NUMBER >= 0x03000000
  mbedtls_rsa_init(rsa);
  mbedtls_rsa_set_padding(rsa, MBEDTLS_RSA_PKCS_V15, MBEDTLS_MD_SHA256);
#else
  mbedtls_rsa_init(rsa, MBEDTLS_RSA_PKCS_V15, MBEDTLS_MD_SHA256);
#endif

  const unsigned char exponent[] = {0x01, 0x00, 0x01};
  int ret = mbedtls_rsa_import_raw(rsa,
                                   publicKeyBytes,
                                   RSA_NUM_BYTES,
                                   nullptr,
                                   0,
                                   nullptr,
                                   0,
                                   nullptr,
                                   0,
                                   exponent,
                                   sizeof(exponent));
  if (ret == 0) {
    ret = mbedtls_rsa_complete(rsa);
  }
  if (ret == 0) {
    ret = mbedtls_rsa_check_pubkey(rsa);
  }

  if (ret == 0) {
    rsa_ctx = rsa;
    ready = true;
    return;
  }

  mbedtls_rsa_free(rsa);
  delete rsa;
}

Supla::RsaVerificator::~RsaVerificator() {
  if (rsa_ctx == nullptr) {
    return;
  }
  mbedtls_rsa_context *rsa = static_cast<mbedtls_rsa_context *>(rsa_ctx);
  mbedtls_rsa_free(rsa);
  delete rsa;
  rsa_ctx = nullptr;
}

bool Supla::RsaVerificator::verify(Supla::Sha256 *hash,
                                   const uint8_t *signatureBytes) {
  if (!ready || hash == nullptr || signatureBytes == nullptr) {
    return false;
  }
  uint8_t digest[32] = {};
  hash->digest(digest, sizeof(digest));

  mbedtls_rsa_context *rsa = static_cast<mbedtls_rsa_context *>(rsa_ctx);
#if MBEDTLS_VERSION_NUMBER >= 0x03000000
  int result = mbedtls_rsa_pkcs1_verify(
      rsa, MBEDTLS_MD_SHA256, sizeof(digest), digest, signatureBytes);
#else
  int result = mbedtls_rsa_pkcs1_verify(rsa,
                                        nullptr,
                                        nullptr,
                                        MBEDTLS_RSA_PUBLIC,
                                        MBEDTLS_MD_SHA256,
                                        sizeof(digest),
                                        digest,
                                        signatureBytes);
#endif

  return result == 0;
}

#endif  // defined(ESP32) || defined(SUPLA_DEVICE_ESP32)

#endif  // SUPLA_TEST

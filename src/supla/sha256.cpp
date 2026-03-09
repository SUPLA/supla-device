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

#include "sha256.h"

#if defined(ESP32) || defined(SUPLA_DEVICE_ESP32)

#include <mbedtls/sha256.h>
#include <string.h>

Supla::Sha256::Sha256() : ctx(nullptr) {
  mbedtls_sha256_context *shaCtx = new mbedtls_sha256_context();
  mbedtls_sha256_init(shaCtx);
  mbedtls_sha256_starts(shaCtx, 0);
  ctx = shaCtx;
}

Supla::Sha256::~Sha256() {
  if (ctx == nullptr) {
    return;
  }
  mbedtls_sha256_context *shaCtx =
      static_cast<mbedtls_sha256_context *>(ctx);
  mbedtls_sha256_free(shaCtx);
  delete shaCtx;
  ctx = nullptr;
}

void Supla::Sha256::update(const uint8_t *data, const int size) {
  if (ctx == nullptr) {
    return;
  }
  mbedtls_sha256_context *shaCtx =
      static_cast<mbedtls_sha256_context *>(ctx);
  mbedtls_sha256_update(shaCtx,
                        static_cast<const unsigned char *>(data),
                        size);
}

void Supla::Sha256::digest(uint8_t *output, int length) {
  if (ctx == nullptr || output == nullptr || length <= 0) {
    return;
  }
  uint8_t fullDigest[32] = {};
  mbedtls_sha256_context tmp;
  mbedtls_sha256_init(&tmp);
  mbedtls_sha256_clone(&tmp,
      static_cast<const mbedtls_sha256_context *>(ctx));
  mbedtls_sha256_finish(&tmp, fullDigest);
  mbedtls_sha256_free(&tmp);

  if (length > 32) {
    length = 32;
  }
  memcpy(output, fullDigest, length);
}

#endif  // SUPLA_DEVICE_ESP32
#endif  // SUPLA_TEST

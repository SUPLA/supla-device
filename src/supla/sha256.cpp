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

#include <mbedtls/md.h>
#include <string.h>

Supla::Sha256::Sha256() : ctx(nullptr) {
  mbedtls_md_context_t *mdCtx = new mbedtls_md_context_t();
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

void Supla::Sha256::update(const uint8_t *data, const int size) {
  if (ctx == nullptr) {
    return;
  }
  mbedtls_md_context_t *mdCtx =
      static_cast<mbedtls_md_context_t *>(ctx);
  mbedtls_md_update(mdCtx,
                    static_cast<const unsigned char *>(data),
                    size);
}

void Supla::Sha256::digest(uint8_t *output, int length) {
  if (ctx == nullptr || output == nullptr || length <= 0) {
    return;
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
    return;
  }
  mbedtls_md_free(&tmp);

  if (length > 32) {
    length = 32;
  }
  memcpy(output, fullDigest, length);
}

#endif  // SUPLA_DEVICE_ESP32
#endif  // SUPLA_TEST

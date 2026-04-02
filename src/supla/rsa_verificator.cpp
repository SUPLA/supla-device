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

#include <mbedtls/md.h>
#include <mbedtls/pk.h>
#include <string.h>

namespace {

static size_t derLenBytes(size_t len) {
  if (len < 128) {
    return 1;
  }
  size_t n = 0;
  while (len > 0) {
    n++;
    len >>= 8;
  }
  return 1 + n;
}

static bool derWriteLen(uint8_t *out,
                        size_t outLen,
                        size_t *pos,
                        size_t len) {
  if (*pos >= outLen) {
    return false;
  }
  if (len < 128) {
    out[(*pos)++] = static_cast<uint8_t>(len);
    return true;
  }
  uint8_t lenBytes[8] = {};
  size_t lenLen = 0;
  while (len > 0) {
    if (lenLen >= sizeof(lenBytes)) {
      return false;
    }
    lenBytes[lenLen++] = static_cast<uint8_t>(len & 0xFF);
    len >>= 8;
  }
  if (*pos + 1 + lenLen > outLen) {
    return false;
  }
  out[(*pos)++] = static_cast<uint8_t>(0x80 | lenLen);
  for (size_t i = 0; i < lenLen; i++) {
    out[(*pos)++] = lenBytes[lenLen - 1 - i];
  }
  return true;
}

static bool derWriteTagLen(uint8_t *out,
                           size_t outLen,
                           size_t *pos,
                           uint8_t tag,
                           size_t len) {
  if (*pos + 1 > outLen) {
    return false;
  }
  out[(*pos)++] = tag;
  return derWriteLen(out, outLen, pos, len);
}

static bool buildRsaSpkiDer(const uint8_t *modulus,
                            size_t modulusLen,
                            uint8_t *out,
                            size_t outLen,
                            size_t *outWritten) {
  if (modulus == nullptr || modulusLen == 0 || out == nullptr ||
      outWritten == nullptr) {
    return false;
  }

  const bool modNeedsZero = (modulus[0] & 0x80) != 0;
  const size_t modValLen = modulusLen + (modNeedsZero ? 1 : 0);
  const size_t modTlvLen = 1 + derLenBytes(modValLen) + modValLen;

  const uint8_t exponent[] = {0x01, 0x00, 0x01};
  const size_t expValLen = sizeof(exponent);
  const size_t expTlvLen = 1 + derLenBytes(expValLen) + expValLen;

  const size_t rsaContentLen = modTlvLen + expTlvLen;
  const size_t rsaSeqLen = 1 + derLenBytes(rsaContentLen) + rsaContentLen;

  const size_t bitContentLen = 1 + rsaSeqLen;  // 1 byte for unused bits
  const size_t bitTlvLen = 1 + derLenBytes(bitContentLen) + bitContentLen;

  const uint8_t algId[] = {
      0x30, 0x0D, 0x06, 0x09, 0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01,
      0x01, 0x01, 0x05, 0x00};
  const size_t algIdLen = sizeof(algId);

  const size_t spkiContentLen = algIdLen + bitTlvLen;
  const size_t spkiSeqLen = 1 + derLenBytes(spkiContentLen) + spkiContentLen;

  if (spkiSeqLen > outLen) {
    return false;
  }

  size_t pos = 0;
  if (!derWriteTagLen(out, outLen, &pos, 0x30, spkiContentLen)) {
    return false;
  }
  memcpy(out + pos, algId, algIdLen);
  pos += algIdLen;

  if (!derWriteTagLen(out, outLen, &pos, 0x03, bitContentLen)) {
    return false;
  }
  out[pos++] = 0x00;  // unused bits

  if (!derWriteTagLen(out, outLen, &pos, 0x30, rsaContentLen)) {
    return false;
  }
  if (!derWriteTagLen(out, outLen, &pos, 0x02, modValLen)) {
    return false;
  }
  if (modNeedsZero) {
    out[pos++] = 0x00;
  }
  memcpy(out + pos, modulus, modulusLen);
  pos += modulusLen;

  if (!derWriteTagLen(out, outLen, &pos, 0x02, expValLen)) {
    return false;
  }
  memcpy(out + pos, exponent, expValLen);
  pos += expValLen;

  *outWritten = pos;
  return true;
}

}  // namespace

Supla::RsaVerificator::RsaVerificator(const uint8_t *publicKeyBytes)
    : rsa_ctx(nullptr), ready(false) {
  if (publicKeyBytes == nullptr) {
    return;
  }
  uint8_t spkiDer[800] = {};
  size_t spkiLen = 0;
  if (!buildRsaSpkiDer(publicKeyBytes,
                       RSA_NUM_BYTES,
                       spkiDer,
                       sizeof(spkiDer),
                       &spkiLen)) {
    return;
  }
  mbedtls_pk_context *pk = new mbedtls_pk_context();
  mbedtls_pk_init(pk);
  int ret = mbedtls_pk_parse_public_key(pk, spkiDer, spkiLen);
  if (ret == 0) {
    rsa_ctx = pk;
    ready = true;
    return;
  }
  mbedtls_pk_free(pk);
  delete pk;
}

Supla::RsaVerificator::~RsaVerificator() {
  if (rsa_ctx == nullptr) {
    return;
  }
  mbedtls_pk_context *pk = static_cast<mbedtls_pk_context *>(rsa_ctx);
  mbedtls_pk_free(pk);
  delete pk;
  rsa_ctx = nullptr;
}

bool Supla::RsaVerificator::verify(Supla::Sha256 *hash,
                                   const uint8_t *signatureBytes) {
  if (!ready || hash == nullptr || signatureBytes == nullptr) {
    return false;
  }
  uint8_t digest[32] = {};
  hash->digest(digest, sizeof(digest));

  mbedtls_pk_context *pk = static_cast<mbedtls_pk_context *>(rsa_ctx);
  int result = mbedtls_pk_verify(pk,
                                 MBEDTLS_MD_SHA256,
                                 digest,
                                 sizeof(digest),
                                 signatureBytes,
                                 RSA_NUM_BYTES);

  return result == 0;
}

#endif  // defined(ESP32) || defined(SUPLA_DEVICE_ESP32)

#endif  // SUPLA_TEST

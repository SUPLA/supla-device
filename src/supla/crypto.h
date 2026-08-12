// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_CRYPTO_H_
#define SRC_SUPLA_CRYPTO_H_

/*
 * Simple wrapper for cryptographic functions.
 */

#include <stdint.h>
#include <stddef.h>

namespace Supla {

namespace Crypto {

  /**
   * PBKDF2-SHA256 key derivation for password
   *
   * @param password null terminated password
   * @param salt salt
   * @param saltLen salt length
   * @param iterations number of iterations
   * @param derivedKey output key
   * @param derivedKeyLen output key length
   *
   * @return true on success
   */
bool pbkdf2Sha256(const char *password,
                   const uint8_t *salt, size_t saltLen, uint32_t iterations,
                   uint8_t *derivedKey, size_t derivedKeyLen);

bool hmacSha256Hex(const char *key, size_t keyLen,
                const char *data, size_t dataLen,
                char *output, size_t outputLen);

};  // namespace Crypto
};  // namespace Supla



#endif  // SRC_SUPLA_CRYPTO_H_

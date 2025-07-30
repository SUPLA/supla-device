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

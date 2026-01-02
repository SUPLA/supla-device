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

#ifndef SRC_SUPLA_RSA_VERIFICATOR_H_
#define SRC_SUPLA_RSA_VERIFICATOR_H_

#ifndef SUPLA_TEST

/*
 * Simple wrapper for Nettle RSA methods used to verify sha256 hash
 * against another hash signed with RSA private key.
 */

#define RSA_NUM_BYTES 512
#define RSA_PUBLIC_EXPONENT 65537

#include <supla/sha256.h>
#include <nettle/rsa.h>
#include <nettle/bignum.h>

namespace Supla {

class RsaVerificator {
 public:
  explicit RsaVerificator(const uint8_t* publicKeyBytes);
  ~RsaVerificator();
  bool verify(Supla::Sha256 *hash, const uint8_t *signatureBytes);
 protected:
  struct rsa_public_key publicKey;
  mpz_t signature;
};

};  // namespace Supla

#endif  // SUPLA_TEST
#endif  // SRC_SUPLA_RSA_VERIFICATOR_H_

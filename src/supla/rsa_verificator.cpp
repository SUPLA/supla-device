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

Supla::RsaVerificator::RsaVerificator(const uint8_t *publicKeyBytes) {
  rsa_public_key_init(&publicKey);
  nettle_mpz_set_str_256_u(publicKey.n, RSA_NUM_BYTES, publicKeyBytes);
  mpz_set_ui(publicKey.e, RSA_PUBLIC_EXPONENT);
  rsa_public_key_prepare(&publicKey);
}

Supla::RsaVerificator::~RsaVerificator() {
  rsa_public_key_clear(&publicKey);
}

bool Supla::RsaVerificator::verify(Supla::Sha256 *hash,
    const uint8_t *signatureBytes) {
  nettle_mpz_init_set_str_256_u(signature, RSA_NUM_BYTES, signatureBytes);
  int result = rsa_sha256_verify(&publicKey, hash->getHash(), signature);
  mpz_clear(signature);

  return result == 1;
}

#endif  // SUPLA_TEST


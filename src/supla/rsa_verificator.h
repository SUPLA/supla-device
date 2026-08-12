// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_RSA_VERIFICATOR_H_
#define SRC_SUPLA_RSA_VERIFICATOR_H_

#ifndef SUPLA_TEST

/*
 * Simple wrapper for mbedTLS RSA methods used to verify sha256 hash
 * against another hash signed with RSA private key.
 */

#define RSA_NUM_BYTES       512
#define RSA_PUBLIC_EXPONENT 65537

#include <stdint.h>
#include <supla/sha256.h>

namespace Supla {

class RsaVerificator {
 public:
  explicit RsaVerificator(const uint8_t *publicKeyBytes);
  ~RsaVerificator();
  bool verify(Supla::Sha256 *hash, const uint8_t *signatureBytes);

 protected:
  void *rsa_ctx;
  bool ready;
};

};  // namespace Supla

#endif  // SUPLA_TEST
#endif  // SRC_SUPLA_RSA_VERIFICATOR_H_

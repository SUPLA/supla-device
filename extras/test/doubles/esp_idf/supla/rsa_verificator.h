// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_TEST_DOUBLES_ESP_IDF_SUPLA_RSA_VERIFICATOR_H_
#define EXTRAS_TEST_DOUBLES_ESP_IDF_SUPLA_RSA_VERIFICATOR_H_

#include <stdint.h>

#define RSA_NUM_BYTES 512

namespace Supla {
class Sha256;

class RsaVerificator {
 public:
  explicit RsaVerificator(const uint8_t *publicKeyBytes);
  ~RsaVerificator();
  bool verify(Supla::Sha256 *hash, const uint8_t *signatureBytes);
};
}  // namespace Supla

#endif  // EXTRAS_TEST_DOUBLES_ESP_IDF_SUPLA_RSA_VERIFICATOR_H_

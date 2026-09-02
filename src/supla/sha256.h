// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_SHA256_H_
#define SRC_SUPLA_SHA256_H_

#include <stdint.h>

/*
 * Simple platform SHA256 wrapper without exposing platform-specific types.
 */

namespace Supla {

class Sha256 {
 public:
  Sha256();
  ~Sha256();
  void update(const uint8_t *data, const int size);
  void digest(uint8_t *output, int length = 32);

 protected:
#ifndef SUPLA_TEST
  void *ctx;
#else
  uint8_t state[32];
  uint32_t offset;
#endif
};

};  // namespace Supla

#endif  // SRC_SUPLA_SHA256_H_

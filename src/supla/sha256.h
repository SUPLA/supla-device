// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_SHA256_H_
#define SRC_SUPLA_SHA256_H_

#include <stdint.h>
#include <stddef.h>

/*
 * Simple platform SHA256 wrapper without exposing platform-specific types.
 */

namespace Supla {

class Sha256 {
 public:
  Sha256();
  ~Sha256();
  bool isValid() const { return ctx != nullptr; }
  bool update(const uint8_t *data, const int size);
  bool digest(uint8_t *output, int length = 32);
  static bool calculate(const uint8_t *data, size_t size, uint8_t output[32]);

 protected:
  void *ctx;
};

};  // namespace Supla

#endif  // SRC_SUPLA_SHA256_H_

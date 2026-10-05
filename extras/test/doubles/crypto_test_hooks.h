// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_TEST_DOUBLES_CRYPTO_TEST_HOOKS_H_
#define EXTRAS_TEST_DOUBLES_CRYPTO_TEST_HOOKS_H_

#include <stdint.h>

// Host-only linker interception; no callbacks or test seams in production code.
class ScopedCryptoTestState {
 public:
  explicit ScopedCryptoTestState(uint8_t seed);
  ~ScopedCryptoTestState();
  ScopedCryptoTestState(const ScopedCryptoTestState &) = delete;
  ScopedCryptoTestState &operator=(const ScopedCryptoTestState &) = delete;
  void resetCalls() { sha256Calls = hmacSha256Calls = 0; }

  uint32_t sha256Calls = 0;
  uint32_t hmacSha256Calls = 0;
  bool failRandom = false;
  uint32_t next;

 private:
  ScopedCryptoTestState *previous;
};

#endif  // EXTRAS_TEST_DOUBLES_CRYPTO_TEST_HOOKS_H_

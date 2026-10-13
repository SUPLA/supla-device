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

// Volatile stores retain the wipe even when the buffer is dead afterwards.
inline void secureClear(void *buffer, size_t size) {
  auto bytes = static_cast<volatile uint8_t *>(buffer);
  while (size--) *bytes++ = 0;
}

template <typename T>
class ScopedWipe {
 public:
  explicit ScopedWipe(T &value) : value_(value) {}
  ~ScopedWipe() { secureClear(&value_, sizeof(value_)); }
  ScopedWipe(const ScopedWipe &) = delete;
  ScopedWipe &operator=(const ScopedWipe &) = delete;

 private:
  T &value_;
};

// Compares all size bytes without stopping at the first difference.
// Both buffers must contain at least size bytes; NULL is rejected.
// Timing may depend on size, which must not depend on secret contents.
bool constantTimeEqual(const void *left, const void *right, size_t size);

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

// Binary HMAC output; nullptr input is allowed only for an empty input.
bool hmacSha256(const uint8_t *key, size_t keyLen,
                const uint8_t *data, size_t dataLen, uint8_t output[32]);

// CSPRNG with observable failure. Empty output succeeds without using the RNG.
bool fillRandom(uint8_t *buffer, size_t size);

bool hmacSha256Hex(const char *key, size_t keyLen,
                const char *data, size_t dataLen,
                char *output, size_t outputLen);

};  // namespace Crypto
};  // namespace Supla



#endif  // SRC_SUPLA_CRYPTO_H_

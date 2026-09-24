// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright 2026 AC SOFTWARE SP. Z O.O.

#ifndef EXTRAS_PORTING_ESP_IDF_SUPLAN_CRYPTO_MBEDTLS_H_
#define EXTRAS_PORTING_ESP_IDF_SUPLAN_CRYPTO_MBEDTLS_H_

#include <suplan/suplan_ports.h>
#include <suplan/suplan_config.h>

namespace Supla {
namespace SupLan {

class MbedTlsCryptoPort : public CryptoPort {
 public:
  bool sha256(const uint8_t *data, size_t length,
              uint8_t output[32]) override;
  bool hmacSha256(const uint8_t *key, size_t keyLength,
                  const uint8_t *data, size_t length,
                  uint8_t output[32]) override;
  bool aes128CcmEncrypt(const uint8_t key[16], const uint8_t nonce[12],
                        const uint8_t *aad, size_t aadLength,
                        const uint8_t *plain, size_t plainLength,
                        uint8_t *cipher, uint8_t tag[16]) override;
  bool aes128CcmDecrypt(const uint8_t key[16], const uint8_t nonce[12],
                        const uint8_t *aad, size_t aadLength,
                        const uint8_t *cipher, size_t cipherLength,
                        const uint8_t tag[16], uint8_t *plain) override;

 private:
  // PSA returns ciphertext followed by the tag; keep that bounded output off
  // the ESP task stack and split it into CryptoPort's separate spans.
  uint8_t aeadOutput_[SUPLAN_MAX_APPLICATION_BYTES + kAeadTagSize];
};

class EspIdfRandomPort : public RandomPort {
 public:
  bool fillRandom(uint8_t *buffer, size_t length) override;
};

}  // namespace SupLan
}  // namespace Supla

#endif  // EXTRAS_PORTING_ESP_IDF_SUPLAN_CRYPTO_MBEDTLS_H_

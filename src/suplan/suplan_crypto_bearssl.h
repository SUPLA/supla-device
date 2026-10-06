// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLAN_SUPLAN_CRYPTO_BEARSSL_H_
#define SRC_SUPLAN_SUPLAN_CRYPTO_BEARSSL_H_

#ifndef ARDUINO_ARCH_AVR

#if defined(ARDUINO_ARCH_ESP8266)
#include <bearssl/bearssl.h>
#include <string.h>
#include <suplan/suplan_ports.h>

namespace Supla {
namespace SupLan {
class BearSslCryptoPort : public Supla::SupLan::CryptoPort {
 public:
  bool aes128CcmEncrypt(const uint8_t key[16], const uint8_t nonce[12],
                        const uint8_t *aad, size_t aadLength,
                        const uint8_t *plain, size_t plainLength,
                        uint8_t *cipher, uint8_t tag[16]) override {
    return ccm(true, key, nonce, aad, aadLength, plain, plainLength,
               cipher, tag);
  }

  bool aes128CcmDecrypt(const uint8_t key[16], const uint8_t nonce[12],
                        const uint8_t *aad, size_t aadLength,
                        const uint8_t *cipher, size_t cipherLength,
                        const uint8_t tag[16], uint8_t *plain) override {
    uint8_t expected[16];
    if (!tag) {
      return false;
    }
    memcpy(expected, tag, sizeof(expected));
    return ccm(false, key, nonce, aad, aadLength, cipher, cipherLength,
               plain, expected);
  }

 private:
  bool ccm(bool encrypt, const uint8_t *key, const uint8_t *nonce,
           const uint8_t *aad, size_t aadLength, const uint8_t *input,
           size_t length, uint8_t *output, uint8_t *tag) {
    if (!key || !nonce || !tag || (!aad && aadLength) ||
        (!input && length) || (!output && length) ||
        length > SUPLAN_MAX_APPLICATION_BYTES) {
      return false;
    }
    br_aes_ct_ctrcbc_keys aes;
    br_ccm_context context;
    br_aes_ct_ctrcbc_init(&aes, key, 16);
    br_ccm_init(&context, &aes.vtable);
    bool valid = br_ccm_reset(&context, nonce, 12, aadLength, length, 16);
    if (valid) {
      if (aadLength) {
        br_ccm_aad_inject(&context, aad, aadLength);
      }
      br_ccm_flip(&context);
      if (length) {
        memmove(output, input, length);
        br_ccm_run(&context, encrypt, output, length);
      }
      valid = encrypt ? br_ccm_get_tag(&context, tag) == 16
                      : br_ccm_check_tag(&context, tag) == 1;
    }
    if (!valid && output && length) {
      memset(output, 0, length);
    }
    memset(&aes, 0, sizeof(aes));
    memset(&context, 0, sizeof(context));
    return valid;
  }
};

}  // namespace SupLan
}  // namespace Supla
#endif  // ARDUINO_ARCH_ESP8266
#endif  // !ARDUINO_ARCH_AVR

#endif  // SRC_SUPLAN_SUPLAN_CRYPTO_BEARSSL_H_

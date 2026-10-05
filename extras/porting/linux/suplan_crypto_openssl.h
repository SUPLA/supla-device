// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright 2026 AC SOFTWARE SP. Z O.O.

#ifndef EXTRAS_PORTING_LINUX_SUPLAN_CRYPTO_OPENSSL_H_
#define EXTRAS_PORTING_LINUX_SUPLAN_CRYPTO_OPENSSL_H_

#include <suplan/suplan_ports.h>

namespace Supla {
namespace SupLan {

class OpenSslCryptoPort : public CryptoPort {
 public:
  bool aes128CcmEncrypt(const uint8_t key[16], const uint8_t nonce[12],
                        const uint8_t *aad, size_t aadLength,
                        const uint8_t *plain, size_t plainLength,
                        uint8_t *cipher, uint8_t tag[16]) override;
  bool aes128CcmDecrypt(const uint8_t key[16], const uint8_t nonce[12],
                        const uint8_t *aad, size_t aadLength,
                        const uint8_t *cipher, size_t cipherLength,
                        const uint8_t tag[16], uint8_t *plain) override;
};

}  // namespace SupLan
}  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_SUPLAN_CRYPTO_OPENSSL_H_

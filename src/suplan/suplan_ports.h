// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLAN_SUPLAN_PORTS_H_
#define SRC_SUPLAN_SUPLAN_PORTS_H_

#include <stddef.h>
#include <stdint.h>

#include "suplan_types.h"

namespace Supla {
namespace SupLan {

class CryptoPort {
 public:
  virtual ~CryptoPort() {}
  virtual bool sha256(const uint8_t *data, size_t length,
                      uint8_t output[32]) = 0;
  virtual bool hmacSha256(const uint8_t *key, size_t keyLength,
                          const uint8_t *data, size_t length,
                          uint8_t output[32]) = 0;
  virtual bool aes128CcmEncrypt(const uint8_t key[16],
                                const uint8_t nonce[12],
                                const uint8_t *aad, size_t aadLength,
                                const uint8_t *plain, size_t plainLength,
                                uint8_t *cipher, uint8_t tag[16]) = 0;
  virtual bool aes128CcmDecrypt(const uint8_t key[16],
                                const uint8_t nonce[12],
                                const uint8_t *aad, size_t aadLength,
                                const uint8_t *cipher, size_t cipherLength,
                                const uint8_t tag[16], uint8_t *plain) = 0;
};

class RandomPort {
 public:
  virtual ~RandomPort() {}
  virtual bool fillRandom(uint8_t *buffer, size_t length) = 0;
};

class DatagramPort {
 public:
  virtual ~DatagramPort() {}
  virtual bool sendUnicast(const Endpoint &endpoint, const uint8_t *data,
                           size_t length) = 0;
  virtual bool sendLocateMulticast(const uint8_t *data, size_t length) = 0;
  virtual int pollReceive(uint8_t *buffer, size_t capacity,
                          Endpoint *endpoint) = 0;
  virtual size_t maxDatagramPayload() const = 0;
  virtual uint32_t nowMs() const = 0;
};

}  // namespace SupLan
}  // namespace Supla

#endif  // SRC_SUPLAN_SUPLAN_PORTS_H_

// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "crypto_test_hooks.h"

#include <openssl/evp.h>

namespace {
thread_local ScopedCryptoTestState *active = nullptr;
}

ScopedCryptoTestState::ScopedCryptoTestState(uint8_t seed)
    : next(seed ? seed : 1), previous(active) {
  active = this;
}

ScopedCryptoTestState::~ScopedCryptoTestState() {
  active = previous;
}

extern "C" {
int __real_RAND_bytes(unsigned char *buffer, int size);
int __real_EVP_DigestInit_ex(EVP_MD_CTX *, const EVP_MD *, ENGINE *);
int __real_EVP_MAC_init(EVP_MAC_CTX *, const unsigned char *, size_t,
                        const OSSL_PARAM *);

int __wrap_RAND_bytes(unsigned char *buffer, int size) {
  if (!active) {
    return __real_RAND_bytes(buffer, size);
  }
  if (active->failRandom) {
    return 0;
  }
  for (int i = 0; i < size; ++i) {
    active->next ^= active->next << 13;
    active->next ^= active->next >> 17;
    active->next ^= active->next << 5;
    buffer[i] = static_cast<uint8_t>(active->next);
  }
  return 1;
}

int __wrap_EVP_DigestInit_ex(EVP_MD_CTX *ctx, const EVP_MD *type,
                            ENGINE *engine) {
  if (active) {
    ++active->sha256Calls;
  }
  return __real_EVP_DigestInit_ex(ctx, type, engine);
}

int __wrap_EVP_MAC_init(EVP_MAC_CTX *ctx, const unsigned char *key,
                       size_t length, const OSSL_PARAM *params) {
  if (active) {
    ++active->hmacSha256Calls;
  }
  return __real_EVP_MAC_init(ctx, key, length, params);
}
}

// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright 2026 AC SOFTWARE SP. Z O.O.

#include "suplan_crypto_openssl.h"

#include <openssl/evp.h>

#include <climits>

namespace Supla {
namespace SupLan {

bool OpenSslCryptoPort::aes128CcmEncrypt(
    const uint8_t key[16], const uint8_t nonce[12], const uint8_t *aad,
    size_t aadLength, const uint8_t *plain, size_t plainLength,
    uint8_t *cipher, uint8_t tag[16]) {
  if (key == nullptr || nonce == nullptr || tag == nullptr ||
      (aad == nullptr && aadLength != 0) ||
      (plain == nullptr && plainLength != 0) ||
      (cipher == nullptr && plainLength != 0) ||
      plainLength > INT_MAX || aadLength > INT_MAX) {
    return false;
  }
  EVP_CIPHER_CTX *context = EVP_CIPHER_CTX_new();
  if (context == nullptr) {
    return false;
  }
  int amount = 0;
  int total = 0;
  bool result = EVP_EncryptInit_ex(context, EVP_aes_128_ccm(), nullptr,
                                   nullptr, nullptr) == 1 &&
      EVP_CIPHER_CTX_ctrl(context, EVP_CTRL_CCM_SET_IVLEN, 12, nullptr) == 1 &&
      EVP_CIPHER_CTX_ctrl(context, EVP_CTRL_CCM_SET_TAG, 16, nullptr) == 1 &&
      EVP_EncryptInit_ex(context, nullptr, nullptr, key, nonce) == 1 &&
      EVP_EncryptUpdate(context, nullptr, &amount, nullptr,
                        static_cast<int>(plainLength)) == 1;
  if (result && aadLength != 0) {
    result = EVP_EncryptUpdate(context, nullptr, &amount, aad,
                               static_cast<int>(aadLength)) == 1;
  }
  if (result && plainLength != 0) {
    result = EVP_EncryptUpdate(context, cipher, &amount, plain,
                               static_cast<int>(plainLength)) == 1;
    total = amount;
  }
  if (result) {
    result = EVP_EncryptFinal_ex(context,
                                 plainLength == 0 ? nullptr : cipher + total,
                                 &amount) == 1 &&
        EVP_CIPHER_CTX_ctrl(context, EVP_CTRL_CCM_GET_TAG, 16, tag) == 1;
  }
  EVP_CIPHER_CTX_free(context);
  return result;
}

bool OpenSslCryptoPort::aes128CcmDecrypt(
    const uint8_t key[16], const uint8_t nonce[12], const uint8_t *aad,
    size_t aadLength, const uint8_t *cipher, size_t cipherLength,
    const uint8_t tag[16], uint8_t *plain) {
  if (key == nullptr || nonce == nullptr || tag == nullptr ||
      (aad == nullptr && aadLength != 0) ||
      (cipher == nullptr && cipherLength != 0) ||
      (plain == nullptr && cipherLength != 0) ||
      cipherLength > INT_MAX || aadLength > INT_MAX) {
    return false;
  }
  EVP_CIPHER_CTX *context = EVP_CIPHER_CTX_new();
  if (context == nullptr) {
    return false;
  }
  int amount = 0;
  bool result = EVP_DecryptInit_ex(context, EVP_aes_128_ccm(), nullptr,
                                   nullptr, nullptr) == 1 &&
      EVP_CIPHER_CTX_ctrl(context, EVP_CTRL_CCM_SET_IVLEN, 12, nullptr) == 1 &&
      EVP_CIPHER_CTX_ctrl(context, EVP_CTRL_CCM_SET_TAG, 16,
                          const_cast<uint8_t *>(tag)) == 1 &&
      EVP_DecryptInit_ex(context, nullptr, nullptr, key, nonce) == 1 &&
      EVP_DecryptUpdate(context, nullptr, &amount, nullptr,
                        static_cast<int>(cipherLength)) == 1;
  if (result && aadLength != 0) {
    result = EVP_DecryptUpdate(context, nullptr, &amount, aad,
                               static_cast<int>(aadLength)) == 1;
  }
  if (result && cipherLength != 0) {
    result = EVP_DecryptUpdate(context, plain, &amount, cipher,
                               static_cast<int>(cipherLength)) == 1;
  } else if (result) {
    // OpenSSL CCM authenticates an empty message at the final update.
    result = EVP_DecryptUpdate(context, nullptr, &amount, nullptr, 0) == 1;
  }
  EVP_CIPHER_CTX_free(context);
  return result;
}

}  // namespace SupLan
}  // namespace Supla

// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright 2026 AC SOFTWARE SP. Z O.O.

#include "suplan_crypto_mbedtls.h"

#include <string.h>

#include <esp_random.h>
#include <psa/crypto.h>

namespace Supla {
namespace SupLan {

namespace {

static bool importKey(psa_key_type_t type, const uint8_t *data,
                      size_t length, size_t bits, psa_key_usage_t usage,
                      psa_algorithm_t algorithm,
                      mbedtls_svc_key_id_t *keyId) {
  if (data == nullptr || length == 0 || keyId == nullptr ||
      psa_crypto_init() != PSA_SUCCESS) {
    return false;
  }
  psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
  psa_set_key_type(&attributes, type);
  psa_set_key_bits(&attributes, bits);
  psa_set_key_usage_flags(&attributes, usage);
  psa_set_key_algorithm(&attributes, algorithm);
  const psa_status_t status = psa_import_key(
      &attributes, data, length, keyId);
  psa_reset_key_attributes(&attributes);
  return status == PSA_SUCCESS;
}

static const uint8_t *nonNullInput(const uint8_t *data, size_t length) {
  static const uint8_t empty = 0;
  return data == nullptr && length == 0 ? &empty : data;
}

}  // namespace

bool MbedTlsCryptoPort::sha256(const uint8_t *data, size_t length,
                              uint8_t output[32]) {
  if ((data == nullptr && length != 0) || output == nullptr ||
      psa_crypto_init() != PSA_SUCCESS) {
    return false;
  }
  size_t outputLength = 0;
  return psa_hash_compute(PSA_ALG_SHA_256, nonNullInput(data, length), length,
                          output, 32, &outputLength) == PSA_SUCCESS &&
      outputLength == 32;
}

bool MbedTlsCryptoPort::hmacSha256(const uint8_t *key, size_t keyLength,
                                  const uint8_t *data, size_t length,
                                  uint8_t output[32]) {
  if (key == nullptr || keyLength == 0 || output == nullptr ||
      (data == nullptr && length != 0) || keyLength > SIZE_MAX / 8 ||
      psa_crypto_init() != PSA_SUCCESS) {
    return false;
  }
  mbedtls_svc_key_id_t keyId = MBEDTLS_SVC_KEY_ID_INIT;
  if (!importKey(PSA_KEY_TYPE_HMAC, key, keyLength, keyLength * 8,
                 PSA_KEY_USAGE_SIGN_MESSAGE,
                 PSA_ALG_HMAC(PSA_ALG_SHA_256), &keyId)) {
    return false;
  }
  size_t outputLength = 0;
  const psa_status_t status = psa_mac_compute(
      keyId, PSA_ALG_HMAC(PSA_ALG_SHA_256), nonNullInput(data, length),
      length, output, 32, &outputLength);
  const psa_status_t destroyStatus = psa_destroy_key(keyId);
  return status == PSA_SUCCESS && destroyStatus == PSA_SUCCESS &&
      outputLength == 32;
}

bool MbedTlsCryptoPort::aes128CcmEncrypt(
    const uint8_t key[16], const uint8_t nonce[12], const uint8_t *aad,
    size_t aadLength, const uint8_t *plain, size_t plainLength,
    uint8_t *cipher, uint8_t tag[16]) {
  if (key == nullptr || nonce == nullptr || tag == nullptr ||
      (aad == nullptr && aadLength != 0) ||
      (plain == nullptr && plainLength != 0) ||
      (cipher == nullptr && plainLength != 0) ||
      plainLength > SUPLAN_MAX_APPLICATION_BYTES ||
      psa_crypto_init() != PSA_SUCCESS) {
    return false;
  }
  mbedtls_svc_key_id_t keyId = MBEDTLS_SVC_KEY_ID_INIT;
  if (!importKey(PSA_KEY_TYPE_AES, key, 16, 128, PSA_KEY_USAGE_ENCRYPT,
                 PSA_ALG_CCM, &keyId)) {
    return false;
  }
  size_t outputLength = 0;
  const psa_status_t status = psa_aead_encrypt(
      keyId, PSA_ALG_CCM, nonce, 12, nonNullInput(aad, aadLength), aadLength,
      nonNullInput(plain, plainLength), plainLength, aeadOutput_,
      sizeof(aeadOutput_), &outputLength);
  const psa_status_t destroyStatus = psa_destroy_key(keyId);
  const bool valid = status == PSA_SUCCESS && destroyStatus == PSA_SUCCESS &&
      outputLength == plainLength + kAeadTagSize;
  if (valid) {
    if (plainLength != 0) {
      memcpy(cipher, aeadOutput_, plainLength);
    }
    memcpy(tag, aeadOutput_ + plainLength, kAeadTagSize);
  }
  memset(aeadOutput_, 0, sizeof(aeadOutput_));
  return valid;
}

bool MbedTlsCryptoPort::aes128CcmDecrypt(
    const uint8_t key[16], const uint8_t nonce[12], const uint8_t *aad,
    size_t aadLength, const uint8_t *cipher, size_t cipherLength,
    const uint8_t tag[16], uint8_t *plain) {
  if (key == nullptr || nonce == nullptr || tag == nullptr ||
      (aad == nullptr && aadLength != 0) ||
      (cipher == nullptr && cipherLength != 0) ||
      (plain == nullptr && cipherLength != 0) ||
      cipherLength > SUPLAN_MAX_APPLICATION_BYTES ||
      psa_crypto_init() != PSA_SUCCESS) {
    return false;
  }
  if (cipherLength != 0) {
    memcpy(aeadOutput_, cipher, cipherLength);
  }
  memcpy(aeadOutput_ + cipherLength, tag, kAeadTagSize);
  mbedtls_svc_key_id_t keyId = MBEDTLS_SVC_KEY_ID_INIT;
  if (!importKey(PSA_KEY_TYPE_AES, key, 16, 128, PSA_KEY_USAGE_DECRYPT,
                 PSA_ALG_CCM, &keyId)) {
    memset(aeadOutput_, 0, sizeof(aeadOutput_));
    return false;
  }
  size_t outputLength = 0;
  const psa_status_t status = psa_aead_decrypt(
      keyId, PSA_ALG_CCM, nonce, 12, nonNullInput(aad, aadLength), aadLength,
      aeadOutput_, cipherLength + kAeadTagSize, plain, cipherLength,
      &outputLength);
  const psa_status_t destroyStatus = psa_destroy_key(keyId);
  memset(aeadOutput_, 0, sizeof(aeadOutput_));
  return status == PSA_SUCCESS && destroyStatus == PSA_SUCCESS &&
      outputLength == cipherLength;
}

bool EspIdfRandomPort::fillRandom(uint8_t *buffer, size_t length) {
  if (buffer == nullptr && length != 0) {
    return false;
  }
  if (length != 0) {
    esp_fill_random(buffer, length);
  }
  return true;
}

}  // namespace SupLan
}  // namespace Supla

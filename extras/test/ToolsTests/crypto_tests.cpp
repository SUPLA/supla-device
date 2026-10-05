// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>
#include <crypto_test_hooks.h>
#include <supla/crypto.h>

#include <limits.h>
#include <stdint.h>
#include <string.h>

TEST(CryptoTests, HmacRfc4231AndHexWrapper) {
  uint8_t key[20];
  memset(key, 0x0b, sizeof(key));
  const char data[] = "Hi There";
  const uint8_t expected[32] = {
      0xb0, 0x34, 0x4c, 0x61, 0xd8, 0xdb, 0x38, 0x53,
      0x5c, 0xa8, 0xaf, 0xce, 0xaf, 0x0b, 0xf1, 0x2b,
      0x88, 0x1d, 0xc2, 0x00, 0xc9, 0x83, 0x3d, 0xa7,
      0x26, 0xe9, 0x37, 0x6c, 0x2e, 0x32, 0xcf, 0xf7};
  uint8_t output[32] = {};
  ASSERT_TRUE(Supla::Crypto::hmacSha256(
      key, sizeof(key), reinterpret_cast<const uint8_t *>(data),
      sizeof(data) - 1, output));
  EXPECT_EQ(memcmp(output, expected, sizeof(output)), 0);
  char hex[65] = {};
  ASSERT_TRUE(Supla::Crypto::hmacSha256Hex(
      reinterpret_cast<const char *>(key), sizeof(key), data,
      sizeof(data) - 1, hex, sizeof(hex)));
  EXPECT_STREQ(hex,
      "B0344C61D8DB38535CA8AFCEAF0BF12B"
      "881DC200C9833DA726E9376C2E32CFF7");
}

TEST(CryptoTests, HmacEmptyInputAndValidation) {
  const uint8_t expected[32] = {
      0xb6, 0x13, 0x67, 0x9a, 0x08, 0x14, 0xd9, 0xec,
      0x77, 0x2f, 0x95, 0xd7, 0x78, 0xc3, 0x5f, 0xc5,
      0xff, 0x16, 0x97, 0xc4, 0x93, 0x71, 0x56, 0x53,
      0xc6, 0xc7, 0x12, 0x14, 0x42, 0x92, 0xc5, 0xad};
  uint8_t output[32] = {};
  ASSERT_TRUE(Supla::Crypto::hmacSha256(nullptr, 0, nullptr, 0, output));
  EXPECT_EQ(memcmp(output, expected, sizeof(output)), 0);
  EXPECT_FALSE(Supla::Crypto::hmacSha256(nullptr, 1, output, 1, output));
  EXPECT_FALSE(Supla::Crypto::hmacSha256(output, 1, nullptr, 1, output));
  EXPECT_FALSE(Supla::Crypto::hmacSha256(output, 1, output, 1, nullptr));
  char hex[65] = {};
  EXPECT_FALSE(Supla::Crypto::hmacSha256Hex("k", 1, "d", 1, hex, 64));
  EXPECT_FALSE(Supla::Crypto::hmacSha256Hex("k", 1, "d", 1, nullptr, 65));
  EXPECT_FALSE(Supla::Crypto::hmacSha256Hex(nullptr, 0, "d", 1, hex, 65));
}

TEST(CryptoTests, CheckedRandomBoundariesAndGeneration) {
  EXPECT_TRUE(Supla::Crypto::fillRandom(nullptr, 0));
  EXPECT_FALSE(Supla::Crypto::fillRandom(nullptr, 1));
  uint8_t output[32] = {};
  EXPECT_FALSE(Supla::Crypto::fillRandom(output,
                                      static_cast<size_t>(INT_MAX) + 1));
  EXPECT_TRUE(Supla::Crypto::fillRandom(output, sizeof(output)));
}

TEST(CryptoTests, ProviderRandomFailureIsObservable) {
  ScopedCryptoTestState rng(5);
  rng.failRandom = true;
  uint8_t output[32] = {};
  EXPECT_FALSE(Supla::Crypto::fillRandom(output, sizeof(output)));
  EXPECT_TRUE(Supla::Crypto::fillRandom(nullptr, 0));
}

// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>
#include <crypto_test_hooks.h>
#include <supla/crypto.h>
#include <suplan/suplan_acl.h>

#include <limits.h>
#include <stdint.h>
#include <string.h>

TEST(CryptoTests, ScopedWipeCoversSuccessAndEarlyReturn) {
  uint8_t temporary[32], live[32];
  memset(live, 0x42, sizeof(live));
  const auto operation = [&](bool success) {
    Supla::Crypto::ScopedWipe<decltype(temporary)> wipe(temporary);
    memcpy(temporary, live, sizeof(temporary));
    if (!success) return false;
    temporary[0] ^= 1;
    return true;
  };
  for (bool success : {false, true}) {
    EXPECT_EQ(operation(success), success);
    const uint8_t zero[32] = {};
    EXPECT_EQ(memcmp(temporary, zero, sizeof(zero)), 0);
    for (auto byte : live) EXPECT_EQ(byte, 0x42);
  }
}
TEST(CryptoTests, EveryPeerDerivationMacFailureRejectsWithoutLiveCredential) {
  ScopedCryptoTestState provider(5);
  const Supla::SupLan::PeerContext context = {1, 0, {1, 101}, {1, 202}, 1, 1};
  const Supla::SupLan::AclEntry acl = {{2, 101}, 1};
  uint8_t root[32];
  memset(root, 0x42, sizeof(root));
  uint8_t index;
  Supla::SupLan::PeerTable baseline;
  ASSERT_TRUE(baseline.addPeerFromRoot(&context, root, 1, &acl, 1, &index));
  const auto calls = provider.hmacSha256Calls;
  ASSERT_GT(calls, 0U);
  for (uint32_t failure = 1; failure <= calls; ++failure) {
    provider.resetCalls();
    provider.failHmacCall = failure;
    Supla::SupLan::PeerTable rejected;
    EXPECT_FALSE(rejected.addPeerFromRoot(&context, root, 1, &acl, 1, &index));
    EXPECT_EQ(rejected.size(), 0);
    EXPECT_EQ(rejected.aclEntryCount(), 0);
    EXPECT_EQ(rejected.get(0), nullptr);
    for (auto byte : root) EXPECT_EQ(byte, 0x42);
  }
}

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

TEST(CryptoTests, HmacRfc4231LongKeyAndOversizedKeyRejection) {
  ScopedCryptoTestState provider(5);
  uint8_t key[131];
  memset(key, 0xaa, sizeof(key));
  const char data[] = "Test Using Larger Than Block-Size Key - Hash Key First";
  char hex[65] = {};
  ASSERT_TRUE(Supla::Crypto::hmacSha256Hex(
      reinterpret_cast<const char *>(key), sizeof(key), data,
      sizeof(data) - 1, hex, sizeof(hex)));
  EXPECT_STREQ(hex,
      "60E431591EE0B67F0D8A26AACBF5B77F"
      "8E0BC6213728C5140546040F0EE37F54");

  uint8_t output[32];
  memset(output, 0x42, sizeof(output));
  const auto calls = provider.hmacSha256Calls;
  EXPECT_FALSE(Supla::Crypto::hmacSha256(
      key, static_cast<size_t>(INT_MAX) + 1, nullptr, 0, output));
  EXPECT_EQ(provider.hmacSha256Calls, calls);
  for (auto byte : output) EXPECT_EQ(byte, 0x42);
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

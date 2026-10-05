// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include <stdint.h>
#include <string.h>

#include <supla/sha256.h>

TEST(Sha256Tests, TestImplementationMatchesSingleAndChunkedUpdate) {
  const uint8_t input[] = {'a', 'b', 'c', 'd', 'e'};
  uint8_t single[32] = {};
  uint8_t chunked[32] = {};

  Supla::Sha256 singleSha;
  singleSha.update(input, sizeof(input));
  singleSha.digest(single);

  Supla::Sha256 chunkedSha;
  chunkedSha.update(input, 2);
  chunkedSha.update(input + 2, sizeof(input) - 2);
  chunkedSha.digest(chunked);

  EXPECT_EQ(memcmp(single, chunked, sizeof(single)), 0);
}

TEST(Sha256Tests, TestImplementationReturnsRequestedPrefix) {
  const uint8_t input[] = {'a', 'b', 'c', 'd', 'e'};
  uint8_t full[32] = {};
  uint8_t prefix[16] = {};

  Supla::Sha256 fullSha;
  fullSha.update(input, sizeof(input));
  fullSha.digest(full);

  Supla::Sha256 prefixSha;
  prefixSha.update(input, sizeof(input));
  prefixSha.digest(prefix, sizeof(prefix));

  EXPECT_EQ(memcmp(full, prefix, sizeof(prefix)), 0);
}

TEST(Sha256Tests, TestImplementationIgnoresInvalidInput) {
  uint8_t output[32] = {};
  uint8_t expected[32] = {};
  ASSERT_TRUE(Supla::Sha256::calculate(nullptr, 0, expected));

  Supla::Sha256 sha;
  EXPECT_FALSE(sha.update(nullptr, 4));
  sha.update(reinterpret_cast<const uint8_t *>("abc"), 0);
  EXPECT_FALSE(sha.digest(nullptr));
  sha.digest(output);

  EXPECT_EQ(memcmp(output, expected, sizeof(output)), 0);
}

TEST(Sha256Tests, KnownAnswerAndOneShotValidation) {
  const uint8_t expected[32] = {
      0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea,
      0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23,
      0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c,
      0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad};
  uint8_t output[32] = {};
  ASSERT_TRUE(Supla::Sha256::calculate(
      reinterpret_cast<const uint8_t *>("abc"), 3, output));
  EXPECT_EQ(memcmp(output, expected, 32), 0);
  EXPECT_FALSE(Supla::Sha256::calculate(nullptr, 1, output));
  EXPECT_FALSE(Supla::Sha256::calculate(nullptr, 0, nullptr));
  Supla::Sha256 streaming;
  ASSERT_TRUE(streaming.update(reinterpret_cast<const uint8_t *>("a"), 1));
  ASSERT_TRUE(streaming.update(reinterpret_cast<const uint8_t *>("bc"), 2));
  ASSERT_TRUE(streaming.digest(output));
  EXPECT_EQ(memcmp(output, expected, 32), 0);
  ASSERT_TRUE(streaming.digest(output));
  EXPECT_EQ(memcmp(output, expected, 32), 0);
  EXPECT_FALSE(streaming.update(nullptr, -1));
}

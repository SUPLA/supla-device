// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include "../../porting/esp-idf/web_login_limiter.h"

namespace {

using Limiter = Supla::WebLoginLimiter;

Limiter::Address ipv4(uint8_t lastByte) {
  Limiter::Address address;
  address.bytes[10] = 0xff;
  address.bytes[11] = 0xff;
  address.bytes[12] = 192;
  address.bytes[13] = 168;
  address.bytes[14] = 1;
  address.bytes[15] = lastByte;
  return address;
}

void block(Limiter *limiter, const Limiter::Address &address, uint32_t now) {
  for (unsigned i = 0; i < Limiter::MAX_ATTEMPTS; i++) {
    ASSERT_FALSE(limiter->isBlocked(address, now));
    EXPECT_EQ(limiter->failedAttempt(address, now), i + 1);
  }
  ASSERT_TRUE(limiter->isBlocked(address, now));
}

}  // namespace

TEST(WebLoginLimiterTests, FifthFailureBlocksOnlyItsSource) {
  Limiter limiter;
  const auto a = ipv4(1);
  const auto b = ipv4(2);
  block(&limiter, a, 100);
  EXPECT_FALSE(limiter.isBlocked(b, 100));
  // Successful B authentication resets B only. A must still be rejected
  // before validating its password, even if that password would be correct.
  limiter.reset(b);
  EXPECT_TRUE(limiter.isBlocked(a, 100));
}

TEST(WebLoginLimiterTests, SuccessfulLoginResetsOnlyMatchingCounter) {
  Limiter limiter;
  const auto a = ipv4(1);
  const auto b = ipv4(2);
  EXPECT_EQ(limiter.failedAttempt(a, 0), 1);
  EXPECT_EQ(limiter.failedAttempt(b, 0), 1);
  limiter.reset(a);
  EXPECT_EQ(limiter.failedAttempt(a, 1), 1);
  EXPECT_EQ(limiter.failedAttempt(b, 1), 2);
}

TEST(WebLoginLimiterTests, ExpiresAtSixtySecondsWithoutExtendingBlock) {
  Limiter limiter;
  const auto a = ipv4(1);
  block(&limiter, a, 100);
  EXPECT_TRUE(limiter.isBlocked(a, 60099));
  EXPECT_EQ(limiter.failedAttempt(a, 60099), Limiter::MAX_ATTEMPTS);
  EXPECT_FALSE(limiter.isBlocked(a, 60100));
  EXPECT_EQ(limiter.failedAttempt(a, 60100), 1);
}

TEST(WebLoginLimiterTests, ClockWrapDoesNotReleaseBlockEarly) {
  Limiter limiter;
  const auto a = ipv4(1);
  const uint32_t start = UINT32_MAX - 1000;
  block(&limiter, a, start);
  EXPECT_TRUE(limiter.isBlocked(a, start + uint32_t{59999}));
  EXPECT_FALSE(limiter.isBlocked(a, start + uint32_t{60000}));
}

TEST(WebLoginLimiterTests, FullTableRejectsNewSourcesWithoutEviction) {
  Limiter limiter;
  for (unsigned i = 0; i < Limiter::CAPACITY; i++) {
    block(&limiter, ipv4(i), 0);
  }
  for (unsigned i = Limiter::CAPACITY; i < 256; i++) {
    EXPECT_TRUE(limiter.isBlocked(ipv4(i), 1));
    EXPECT_EQ(limiter.failedAttempt(ipv4(i), 1), Limiter::MAX_ATTEMPTS);
  }
  for (unsigned i = 0; i < Limiter::CAPACITY; i++) {
    EXPECT_TRUE(limiter.isBlocked(ipv4(i), 59999));
  }
  EXPECT_FALSE(limiter.isBlocked(ipv4(100), 60000));
  EXPECT_EQ(limiter.failedAttempt(ipv4(100), 60000), 1);
  EXPECT_LE(sizeof(Limiter), 256u);
}

TEST(WebLoginLimiterTests, PartialCountersExpireAndSuccessfulSourcesFreeSlots) {
  Limiter limiter;
  for (unsigned i = 0; i < Limiter::CAPACITY; i++) {
    EXPECT_EQ(limiter.failedAttempt(ipv4(i), 0), 1);
  }
  EXPECT_TRUE(limiter.isBlocked(ipv4(100), 1));
  EXPECT_FALSE(limiter.isBlocked(ipv4(0), 1));
  limiter.reset(ipv4(0));
  EXPECT_FALSE(limiter.isBlocked(ipv4(100), 1));
  EXPECT_EQ(limiter.failedAttempt(ipv4(100), 1), 1);
  EXPECT_FALSE(limiter.isBlocked(ipv4(101), 60000));
}

TEST(WebLoginLimiterTests, UsesEntireIpv6AddressAndScope) {
  Limiter limiter;
  Limiter::Address a;
  a.bytes[0] = 0x20;
  a.bytes[1] = 0x01;
  a.bytes[15] = 1;
  auto b = a;
  b.bytes[7] = 1;  // Same final 32 bits, different IPv6 address.
  auto otherScope = a;
  otherScope.scope = 2;
  block(&limiter, a, 0);
  EXPECT_FALSE(limiter.isBlocked(b, 0));
  EXPECT_FALSE(limiter.isBlocked(otherScope, 0));
  EXPECT_FALSE(limiter.isBlocked(ipv4(1), 0));
}

TEST(WebLoginLimiterTests, Ipv4MappedIpv6SharesTheIpv4Counter) {
  Limiter limiter;
  const auto a = ipv4(1);
  Limiter::Address mapped;
  const uint8_t bytes[16] =
      {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff, 192, 168, 1, 1};
  memcpy(mapped.bytes, bytes, sizeof(bytes));
  block(&limiter, a, 0);
  EXPECT_TRUE(limiter.isBlocked(mapped, 0));
}

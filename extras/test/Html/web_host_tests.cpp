// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>
#include <supla/network/web_host.h>

#include <cstring>
#include <string>

TEST(WebHostTests, MatchesOnlyDeviceNameAndLocalAlias) {
  Supla::WebHost host;
  for (const char *value : {"SUPLA-123", "supla-123.local",
                            "Supla-123.LOCAL.:443"}) {
    ASSERT_TRUE(host.parse(value));
    EXPECT_TRUE(host.matchesName("SUPLA-123"));
    EXPECT_FALSE(host.matchesName("SUPLA-124"));
    EXPECT_FALSE(host.matchesName(""));
    EXPECT_FALSE(host.matchesName(nullptr));
  }
  for (const char *value : {"evil.example", "supla-123.local.evil.example",
                            "supla-1234", "prefix-supla-123"}) {
    ASSERT_TRUE(host.parse(value));
    EXPECT_FALSE(host.matchesName("SUPLA-123"));
  }
}

TEST(WebHostTests, ParsesAddressesAndPortsWithoutTruncation) {
  Supla::WebHost host;
  ASSERT_TRUE(host.parse("192.168.4.1:80"));
  EXPECT_TRUE(host.ipv4);
  EXPECT_EQ(host.address[0], 192);
  EXPECT_EQ(host.address[3], 1);
  EXPECT_TRUE(host.hasPort);
  EXPECT_EQ(host.port, 80);
  EXPECT_FALSE(host.matchesName("192.168.4.1"));
  ASSERT_TRUE(host.parse("[2001:db8::1234]:443"));
  EXPECT_TRUE(host.ipv6);
  EXPECT_STREQ(host.name, "2001:db8::1234");
  EXPECT_EQ(host.port, 443);
  ASSERT_TRUE(host.parse("supla-123"));
  EXPECT_FALSE(host.ipv6);
  EXPECT_FALSE(host.ipv4);
  EXPECT_FALSE(host.hasPort);
}

TEST(WebHostTests, RejectsMalformedAndOversizedAuthorities) {
  Supla::WebHost host;
  EXPECT_FALSE(host.parse(nullptr));
  for (const char *value : {"", ".", "...", "256.1.2.3", "127.1",
                            "192.168.004.1", "192.168.4.1.",
                            "supla-123:", "supla-123:0", "supla-123:65536",
                            "supla-123:99999999999999999999", "supla-123:abc",
                            "supla-123:80:443", "user@supla-123",
                            "supla-123/path", "supla-123\\path",
                            "supla-123\r\nInjected: value", "supla-123 ",
                            "a..b", "2001:db8::1", "[::1", "[]", "[::1]x",
                            "[fe80::1%25eth0]"}) {
    EXPECT_FALSE(host.parse(value)) << value;
  }
  std::string oversized(254, 'a');
  EXPECT_FALSE(host.parse(oversized.c_str()));
  char unterminated[262];
  memset(unterminated, 'a', sizeof(unterminated));
  EXPECT_FALSE(host.parse(unterminated));
}

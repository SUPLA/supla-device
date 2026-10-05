// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

// ESP8266 NONOS headers included by Arduino define LOCAL as static.
#define LOCAL static
#include <supla/device/server_identity.h>
LOCAL constexpr int localMacroValue = 9;
#undef LOCAL

TEST(ServerIdentityPortabilityTests, SupportsEsp8266LocalMacro) {
  EXPECT_EQ(localMacroValue, 9);
  EXPECT_EQ(static_cast<int>(Supla::Device::ServerChannelLocation::kLocal), 0);
  EXPECT_EQ(static_cast<int>(Supla::Device::ServerChannelLocation::kRemote), 1);
  EXPECT_EQ(static_cast<int>(Supla::Device::ServerChannelLocation::kUnresolved),
            2);
}

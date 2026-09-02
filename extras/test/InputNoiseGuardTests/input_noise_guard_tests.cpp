// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>
#include <simple_time.h>
#include <supla/input_noise_guard.h>

class InputNoiseGuardFixture : public testing::Test {
 protected:
  void SetUp() override {
    time.value = 0;
    Supla::InputNoiseGuard::Clear();
    Supla::InputNoiseGuard::SetWifiTransitionGuardMs(
        SUPLA_INPUT_NOISE_GUARD_WIFI_TRANSITION_MS);
  }

  void TearDown() override {
    Supla::InputNoiseGuard::Clear();
    Supla::InputNoiseGuard::SetWifiTransitionGuardMs(
        SUPLA_INPUT_NOISE_GUARD_WIFI_TRANSITION_MS);
  }

  SimpleTime time;
};

TEST_F(InputNoiseGuardFixture, IgnoreForMsActivatesGuardUntilTimeout) {
  Supla::InputNoiseGuard::IgnoreForMs(100);

  EXPECT_TRUE(Supla::InputNoiseGuard::IsActive());
  EXPECT_EQ(100u, Supla::InputNoiseGuard::RemainingMs());

  time.advance(99);
  EXPECT_TRUE(Supla::InputNoiseGuard::IsActive());
  EXPECT_EQ(1u, Supla::InputNoiseGuard::RemainingMs());

  time.advance(1);
  EXPECT_FALSE(Supla::InputNoiseGuard::IsActive());
  EXPECT_EQ(0u, Supla::InputNoiseGuard::RemainingMs());
}

TEST_F(InputNoiseGuardFixture, ShorterIgnoreForMsDoesNotShortenActiveGuard) {
  Supla::InputNoiseGuard::IgnoreForMs(1000);
  time.advance(100);
  Supla::InputNoiseGuard::IgnoreForMs(100);

  EXPECT_TRUE(Supla::InputNoiseGuard::IsActive());
  EXPECT_EQ(900u, Supla::InputNoiseGuard::RemainingMs());
}

TEST_F(InputNoiseGuardFixture, LongerIgnoreForMsExtendsActiveGuard) {
  Supla::InputNoiseGuard::IgnoreForMs(100);
  time.advance(50);
  Supla::InputNoiseGuard::IgnoreForMs(200);

  EXPECT_TRUE(Supla::InputNoiseGuard::IsActive());
  EXPECT_EQ(200u, Supla::InputNoiseGuard::RemainingMs());
}

TEST_F(InputNoiseGuardFixture, ClearDisablesGuard) {
  Supla::InputNoiseGuard::IgnoreForMs(100);

  Supla::InputNoiseGuard::Clear();

  EXPECT_FALSE(Supla::InputNoiseGuard::IsActive());
  EXPECT_EQ(0u, Supla::InputNoiseGuard::RemainingMs());
}

TEST_F(InputNoiseGuardFixture, NotifyWifiTransitionUsesConfiguredTimeout) {
  Supla::InputNoiseGuard::SetWifiTransitionGuardMs(750);

  Supla::InputNoiseGuard::NotifyWifiTransition();

  EXPECT_TRUE(Supla::InputNoiseGuard::IsActive());
  EXPECT_EQ(750u, Supla::InputNoiseGuard::RemainingMs());
  EXPECT_EQ(750u, Supla::InputNoiseGuard::GetWifiTransitionGuardMs());
}

TEST_F(InputNoiseGuardFixture, WifiDisconnectsExtendGuardUpToThreeTimeouts) {
  Supla::InputNoiseGuard::SetWifiTransitionGuardMs(100);

  Supla::InputNoiseGuard::NotifyWifiStaDisconnected();
  EXPECT_EQ(100u, Supla::InputNoiseGuard::RemainingMs());

  time.advance(80);
  Supla::InputNoiseGuard::NotifyWifiStaDisconnected();
  EXPECT_EQ(100u, Supla::InputNoiseGuard::RemainingMs());

  time.advance(80);
  Supla::InputNoiseGuard::NotifyWifiStaDisconnected();
  EXPECT_EQ(100u, Supla::InputNoiseGuard::RemainingMs());

  time.advance(80);
  Supla::InputNoiseGuard::NotifyWifiStaDisconnected();
  EXPECT_EQ(60u, Supla::InputNoiseGuard::RemainingMs());

  time.advance(60);
  EXPECT_FALSE(Supla::InputNoiseGuard::IsActive());

  Supla::InputNoiseGuard::NotifyWifiStaDisconnected();
  EXPECT_FALSE(Supla::InputNoiseGuard::IsActive());
}

TEST_F(InputNoiseGuardFixture, WifiConnectionStartsNewDisconnectSeries) {
  Supla::InputNoiseGuard::SetWifiTransitionGuardMs(100);
  Supla::InputNoiseGuard::NotifyWifiStaDisconnected();
  time.advance(300);

  Supla::InputNoiseGuard::NotifyWifiStaConnected();
  Supla::InputNoiseGuard::NotifyWifiStaDisconnected();

  EXPECT_TRUE(Supla::InputNoiseGuard::IsActive());
  EXPECT_EQ(100u, Supla::InputNoiseGuard::RemainingMs());
}

TEST_F(InputNoiseGuardFixture, WifiConnectionDoesNotShortenActiveGuard) {
  Supla::InputNoiseGuard::SetWifiTransitionGuardMs(100);
  Supla::InputNoiseGuard::NotifyWifiStaDisconnected();
  time.advance(40);

  Supla::InputNoiseGuard::NotifyWifiStaConnected();

  EXPECT_TRUE(Supla::InputNoiseGuard::IsActive());
  EXPECT_EQ(60u, Supla::InputNoiseGuard::RemainingMs());
}

TEST_F(InputNoiseGuardFixture, WifiTransitionStartsNewDisconnectSeries) {
  Supla::InputNoiseGuard::SetWifiTransitionGuardMs(100);
  Supla::InputNoiseGuard::NotifyWifiStaDisconnected();
  time.advance(300);
  EXPECT_FALSE(Supla::InputNoiseGuard::IsActive());

  Supla::InputNoiseGuard::NotifyWifiTransition();
  time.advance(100);
  EXPECT_FALSE(Supla::InputNoiseGuard::IsActive());

  Supla::InputNoiseGuard::NotifyWifiStaDisconnected();
  EXPECT_TRUE(Supla::InputNoiseGuard::IsActive());
  EXPECT_EQ(100u, Supla::InputNoiseGuard::RemainingMs());
}

TEST_F(InputNoiseGuardFixture, ZeroTimeoutDoesNotActivateGuard) {
  Supla::InputNoiseGuard::IgnoreForMs(0);

  EXPECT_FALSE(Supla::InputNoiseGuard::IsActive());
  EXPECT_EQ(0u, Supla::InputNoiseGuard::RemainingMs());
}

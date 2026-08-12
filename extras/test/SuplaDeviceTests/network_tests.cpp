// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>
#include <supla/network/network.h>

namespace {

class CountingNetwork : public Supla::Network {
 public:
  explicit CountingNetwork(Supla::Network::IntfType type) {
    intfType = type;
  }

  void setup() override {
    setupCount++;
  }

  void disable() override {
    disableCount++;
  }

  bool isReady() override {
    return ready;
  }

  bool iterate() override {
    iterateCount++;
    return iterateResult;
  }

  void setDisabledInConfig(bool disabled) {
    intfDisabledInConfig = disabled;
  }

  int setupCount = 0;
  int disableCount = 0;
  int iterateCount = 0;
  bool ready = false;
  bool iterateResult = false;
};

}  // namespace

TEST(NetworkTests, SetupDisablesInterfaceDisabledAfterConfigMode) {
  CountingNetwork eth(Supla::Network::IntfType::Ethernet);
  CountingNetwork wifi(Supla::Network::IntfType::WiFi);
  wifi.setDisabledInConfig(true);

  Supla::Network::SetConfigMode();
  ASSERT_TRUE(Supla::Network::PopSetupNeeded());
  Supla::Network::Setup();

  EXPECT_EQ(1, eth.setupCount);
  EXPECT_EQ(1, wifi.setupCount);
  EXPECT_EQ(0, wifi.disableCount);

  Supla::Network::SetNormalMode();
  ASSERT_TRUE(Supla::Network::PopSetupNeeded());
  Supla::Network::Setup();

  EXPECT_EQ(2, eth.setupCount);
  EXPECT_EQ(1, wifi.setupCount);
  EXPECT_EQ(1, wifi.disableCount);
}

TEST(NetworkTests, DisabledInterfaceStillTriggersSetupNeededForCleanup) {
  CountingNetwork wifi(Supla::Network::IntfType::WiFi);
  wifi.setDisabledInConfig(true);

  Supla::Network::SetNormalMode();

  ASSERT_TRUE(Supla::Network::PopSetupNeeded());
  Supla::Network::Setup();

  EXPECT_EQ(0, wifi.setupCount);
  EXPECT_EQ(1, wifi.disableCount);
}

TEST(NetworkTests, DisabledInterfaceIsNotUsedAsReadyNetwork) {
  CountingNetwork eth(Supla::Network::IntfType::Ethernet);
  CountingNetwork wifi(Supla::Network::IntfType::WiFi);
  eth.ready = false;
  wifi.ready = true;
  wifi.iterateResult = true;
  wifi.setDisabledInConfig(true);

  EXPECT_FALSE(Supla::Network::IsReady());
  EXPECT_FALSE(Supla::Network::Iterate());
  EXPECT_EQ(0, wifi.iterateCount);
}

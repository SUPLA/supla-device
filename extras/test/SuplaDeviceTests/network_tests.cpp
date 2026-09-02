// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>
#include <string.h>
#include <supla/network/network.h>

namespace {

class HostnameNetwork : public Supla::Network {
 public:
  explicit HostnameNetwork(const uint8_t mac[6]) {
    memcpy(macAddress, mac, sizeof(macAddress));
  }

  void setup() override {}
  void disable() override {}
  bool isReady() override {
    return false;
  }
  bool getMacAddr(uint8_t *output) override {
    memcpy(output, macAddress, sizeof(macAddress));
    return true;
  }

 private:
  uint8_t macAddress[6] = {};
};

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

TEST(NetworkTests, GenerateHostnameHandlesPrefixAndMacBoundaries) {
  const uint8_t mac[] = {0x00, 0x11, 0x22, 0x33, 0xA1, 0xB2};
  HostnameNetwork network(mac);
  char output[33] = {};
  output[32] = static_cast<char>(0x5A);

  network.generateHostname("sensor", 2, output);
  EXPECT_STREQ(output, "sensor-A1B2");

  network.generateHostname("sensor-", 2, output);
  EXPECT_STREQ(output, "sensor-A1B2");

  network.generateHostname("", 2, output);
  EXPECT_STREQ(output, "SUPLA-A1B2");

  network.generateHostname("", 0, output);
  EXPECT_STREQ(output, "SUPLA");

  char longPrefix[64];
  memset(longPrefix, 'X', sizeof(longPrefix) - 1);
  longPrefix[sizeof(longPrefix) - 1] = '\0';
  network.generateHostname(longPrefix, 6, output);
  EXPECT_STREQ(output, "XXXXXXXXXXXXXXXXXX-00112233A1B2");
  EXPECT_EQ(strlen(output), 31U);
  EXPECT_EQ(output[31], '\0');
  EXPECT_EQ(output[32], static_cast<char>(0x5A));

  char maxPrefix[32];
  memset(maxPrefix, 'Y', sizeof(maxPrefix) - 1);
  maxPrefix[sizeof(maxPrefix) - 1] = '\0';
  network.generateHostname(maxPrefix, 2, output);
  EXPECT_STREQ(output, "YYYYYYYYYYYYYYYYYYYYYYYYYY-A1B2");
  EXPECT_EQ(strlen(output), 31U);
  EXPECT_EQ(output[31], '\0');
  EXPECT_EQ(output[32], static_cast<char>(0x5A));

  network.generateHostname("sensor", -1, output);
  EXPECT_STREQ(output, "sensor");

  network.generateHostname("sensor", 7, output);
  EXPECT_STREQ(output, "sensor-00112233A1B2");
  EXPECT_EQ(output[31], '\0');
  EXPECT_EQ(output[32], static_cast<char>(0x5A));
}

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

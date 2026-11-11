// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include <cstring>
#include <string>

#include <supla/channels/channel.h>
#include <supla/sensor/general_purpose_text.h>

#include "simple_time.h"

class GeneralPurposeTextTestsFixture : public ::testing::Test {
 protected:
  void SetUp() override {
    Supla::Channel::resetToDefaults();
  }

  void TearDown() override {
    Supla::Channel::resetToDefaults();
  }
};

TEST_F(GeneralPurposeTextTestsFixture, channelIsReadOnlyGeneralPurposeText) {
  Supla::Sensor::GeneralPurposeText sensor;
  auto channel = sensor.getChannel();

  EXPECT_EQ(SUPLA_CHANNELTYPE_GENERAL_PURPOSE_TEXT, channel->getChannelType());
  EXPECT_EQ(SUPLA_CHANNELFNC_GENERAL_PURPOSE_TEXT,
            channel->getDefaultFunction());
  EXPECT_TRUE(
      channel->isFunctionValid(SUPLA_CHANNELFNC_GENERAL_PURPOSE_TEXT));
  EXPECT_FALSE(channel->isFunctionValid(SUPLA_CHANNELFNC_THERMOMETER));
}

TEST_F(GeneralPurposeTextTestsFixture, publishesTextAndSkipsUnchangedValue) {
  SimpleTime time;
  Supla::Sensor::GeneralPurposeText sensor;
  auto channel = sensor.getChannel();

  sensor.setValue("hello");
  ASSERT_TRUE(channel->isUpdateReady());
  ASSERT_NE(nullptr, channel->getExtValue());
  EXPECT_EQ(EV_TYPE_GENERAL_PURPOSE_TEXT, channel->getExtValue()->type);
  EXPECT_EQ(5U, channel->getExtValue()->size);
  EXPECT_EQ(0, std::memcmp(channel->getExtValue()->value, "hello", 5));

  auto sequence = channel->getValueInt64();
  channel->clearSendValue();
  sensor.setValue("hello");
  EXPECT_FALSE(channel->isUpdateReady());
  EXPECT_EQ(sequence, channel->getValueInt64());

  sensor.setValue("world");
  EXPECT_TRUE(channel->isUpdateReady());
  EXPECT_EQ(sequence + 1, channel->getValueInt64());
  EXPECT_EQ(0, std::memcmp(channel->getExtValue()->value, "world", 5));
}

TEST_F(GeneralPurposeTextTestsFixture, handlesEmptyAndTruncatedText) {
  SimpleTime time;
  Supla::Sensor::GeneralPurposeText sensor;
  auto channel = sensor.getChannel();

  sensor.setValue(nullptr);
  ASSERT_TRUE(channel->isUpdateReady());
  ASSERT_NE(nullptr, channel->getExtValue());
  EXPECT_EQ(EV_TYPE_GENERAL_PURPOSE_TEXT, channel->getExtValue()->type);
  EXPECT_EQ(0U, channel->getExtValue()->size);

  channel->clearSendValue();
  std::string longText(SUPLA_GENERAL_PURPOSE_TEXT_MAX_SIZE + 10, 'x');
  sensor.setValue(longText.c_str());
  EXPECT_TRUE(channel->isUpdateReady());
  EXPECT_EQ(SUPLA_GENERAL_PURPOSE_TEXT_MAX_SIZE, channel->getExtValue()->size);
  EXPECT_EQ(0, std::memcmp(channel->getExtValue()->value,
                           longText.data(),
                           SUPLA_GENERAL_PURPOSE_TEXT_MAX_SIZE));
}

TEST_F(GeneralPurposeTextTestsFixture, periodicallyResendsTextUsingConfig) {
  SimpleTime time;
  Supla::Sensor::GeneralPurposeText sensor;
  auto channel = sensor.getChannel();

  TSD_ChannelConfig channelConfig = {};
  channelConfig.ConfigType = SUPLA_CONFIG_TYPE_DEFAULT;
  channelConfig.ConfigSize = sizeof(TChannelConfig_GeneralPurposeText);
  auto config = reinterpret_cast<TChannelConfig_GeneralPurposeText *>(
      channelConfig.Config);
  config->RefreshIntervalMs = 200;

  EXPECT_EQ(Supla::ApplyConfigResult::Success,
            sensor.applyChannelConfig(&channelConfig, false));

  TChannelConfig_GeneralPurposeText filledConfig = {};
  int filledConfigSize = 0;
  sensor.fillChannelConfig(&filledConfig, &filledConfigSize,
                           SUPLA_CONFIG_TYPE_DEFAULT);
  EXPECT_EQ(sizeof(filledConfig), filledConfigSize);
  EXPECT_EQ(200, filledConfig.RefreshIntervalMs);
  EXPECT_EQ(0, filledConfig.Reserved[0]);

  sensor.setValue("current state");
  ASSERT_TRUE(channel->isUpdateReady());
  auto sequence = channel->getValueInt64();
  channel->clearSendValue();

  time.advance(199);
  sensor.iterateAlways();
  EXPECT_FALSE(channel->isUpdateReady());

  time.advance(1);
  sensor.iterateAlways();
  EXPECT_TRUE(channel->isUpdateReady());
  EXPECT_EQ(sequence + 1, channel->getValueInt64());
  EXPECT_EQ(13U, channel->getExtValue()->size);
  EXPECT_EQ(0, std::memcmp(channel->getExtValue()->value,
                           "current state",
                           13));
}

TEST_F(GeneralPurposeTextTestsFixture, zeroRefreshIntervalUsesDefault) {
  SimpleTime time;
  Supla::Sensor::GeneralPurposeText sensor;
  auto channel = sensor.getChannel();

  sensor.setValue("current state");
  channel->clearSendValue();

  time.advance(4999);
  sensor.iterateAlways();
  EXPECT_FALSE(channel->isUpdateReady());

  time.advance(1);
  sensor.iterateAlways();
  EXPECT_TRUE(channel->isUpdateReady());
}

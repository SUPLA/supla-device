// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include <supla/channels/channel.h>
#include <supla/sensor/general_purpose_text.h>

#include <cstring>
#include <string>

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
  EXPECT_EQ(SUPLA_GENERAL_PURPOSE_TEXT_PAYLOAD_HEADER_SIZE + 5,
            channel->getExtValue()->size);
  EXPECT_EQ(0, channel->getExtValue()->value[0]);
  EXPECT_EQ(0, channel->getExtValue()->value[1]);
  EXPECT_EQ(0, channel->getExtValue()->value[2]);
  EXPECT_EQ(0, channel->getExtValue()->value[3]);
  EXPECT_EQ(0, std::memcmp(
                   channel->getExtValue()->value +
                       SUPLA_GENERAL_PURPOSE_TEXT_PAYLOAD_HEADER_SIZE,
                   "hello", 5));

  auto sequence = channel->getValueInt64();
  channel->clearSendValue();
  sensor.setValue("hello");
  EXPECT_FALSE(channel->isUpdateReady());
  EXPECT_EQ(sequence, channel->getValueInt64());

  sensor.setValue("world");
  EXPECT_TRUE(channel->isUpdateReady());
  EXPECT_EQ(sequence + 1, channel->getValueInt64());
  EXPECT_EQ(
      0, std::memcmp(
             channel->getExtValue()->value +
                 SUPLA_GENERAL_PURPOSE_TEXT_PAYLOAD_HEADER_SIZE,
             "world", 5));
}

TEST_F(GeneralPurposeTextTestsFixture, handlesEmptyAndTruncatedText) {
  SimpleTime time;
  Supla::Sensor::GeneralPurposeText sensor;
  auto channel = sensor.getChannel();

  sensor.setValue(nullptr);
  ASSERT_TRUE(channel->isUpdateReady());
  ASSERT_NE(nullptr, channel->getExtValue());
  EXPECT_EQ(EV_TYPE_GENERAL_PURPOSE_TEXT, channel->getExtValue()->type);
  EXPECT_EQ(SUPLA_GENERAL_PURPOSE_TEXT_PAYLOAD_HEADER_SIZE,
            channel->getExtValue()->size);

  channel->clearSendValue();
  std::string longText(SUPLA_GENERAL_PURPOSE_TEXT_MAX_SIZE + 10, 'x');
  sensor.setValue(longText.c_str());
  EXPECT_TRUE(channel->isUpdateReady());
  EXPECT_EQ(SUPLA_GENERAL_PURPOSE_TEXT_PAYLOAD_HEADER_SIZE +
                SUPLA_GENERAL_PURPOSE_TEXT_MAX_SIZE,
            channel->getExtValue()->size);
  EXPECT_EQ(0, std::memcmp(
                   channel->getExtValue()->value +
                       SUPLA_GENERAL_PURPOSE_TEXT_PAYLOAD_HEADER_SIZE,
                   longText.data(), SUPLA_GENERAL_PURPOSE_TEXT_MAX_SIZE));
}

TEST_F(GeneralPurposeTextTestsFixture, doesNotSupportChannelConfig) {
  Supla::Sensor::GeneralPurposeText sensor;
  TSD_ChannelConfig channelConfig = {};
  EXPECT_EQ(Supla::ApplyConfigResult::NotSupported,
            sensor.applyChannelConfig(&channelConfig, false));
  EXPECT_EQ(Supla::ApplyConfigResult::NotSupported,
            sensor.applyChannelConfig(nullptr, false));

  int configSize = 1;
  sensor.fillChannelConfig(nullptr, &configSize, SUPLA_CONFIG_TYPE_DEFAULT);
  EXPECT_EQ(0, configSize);
}

TEST_F(GeneralPurposeTextTestsFixture, truncatesUtf8AtCodePointBoundary) {
  SimpleTime time;
  Supla::Sensor::GeneralPurposeText sensor;
  auto channel = sensor.getChannel();

  std::string exactFit(SUPLA_GENERAL_PURPOSE_TEXT_MAX_SIZE - 3, 'x');
  exactFit.append("\xE2\x82\xAC");
  sensor.setValue(exactFit.c_str());
  ASSERT_EQ(SUPLA_GENERAL_PURPOSE_TEXT_PAYLOAD_HEADER_SIZE +
                SUPLA_GENERAL_PURPOSE_TEXT_MAX_SIZE,
            channel->getExtValue()->size);
  EXPECT_EQ(0, std::memcmp(
                   channel->getExtValue()->value +
                       SUPLA_GENERAL_PURPOSE_TEXT_PAYLOAD_HEADER_SIZE,
                   exactFit.data(), SUPLA_GENERAL_PURPOSE_TEXT_MAX_SIZE));

  std::string overLimit(SUPLA_GENERAL_PURPOSE_TEXT_MAX_SIZE - 2, 'y');
  overLimit.append("\xE2\x82\xAC");
  sensor.setValue(overLimit.c_str());
  EXPECT_EQ(SUPLA_GENERAL_PURPOSE_TEXT_PAYLOAD_HEADER_SIZE +
                SUPLA_GENERAL_PURPOSE_TEXT_MAX_SIZE - 2,
            channel->getExtValue()->size);
  EXPECT_EQ(0, std::memcmp(
                   channel->getExtValue()->value +
                       SUPLA_GENERAL_PURPOSE_TEXT_PAYLOAD_HEADER_SIZE,
                   overLimit.data(), SUPLA_GENERAL_PURPOSE_TEXT_MAX_SIZE - 2));
}

TEST_F(GeneralPurposeTextTestsFixture,
       periodicallyResendsTextUsingDefaultInterval) {
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

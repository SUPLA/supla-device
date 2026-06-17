/*
 Copyright (C) AC SOFTWARE SP. Z O.O.

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include <gtest/gtest.h>
#include <simple_time.h>
#include <supla/channels/channel.h>
#include <supla/element.h>
#include <supla/sensor/thermometer_driver.h>
#include <supla/suplet/manager.h>
#include <supla/suplet/runtime.h>
#include <supla/suplet/thermometer_group.h>
#include <supla/suplet/virtual_channel.h>

namespace {

class SupletThermometerGroupFixture : public testing::Test {
 protected:
  SimpleTime time;

  void SetUp() override {
    Supla::Channel::resetToDefaults();
  }

  void TearDown() override {
    while (Supla::Element::begin() != nullptr) {
      delete Supla::Element::begin();
    }
    Supla::Channel::resetToDefaults();
  }

  Supla::Suplet::VirtualThermometer *addSource(int channelNumber,
                                               double value,
                                               bool online = true) {
    auto thermometer = new Supla::Suplet::VirtualThermometer(1, channelNumber);
    thermometer->setValue(value);
    thermometer->getChannel()->setNewValue(value);
    if (online) {
      thermometer->getChannel()->setStateOnline();
    } else {
      thermometer->getChannel()->setStateOffline();
    }
    return thermometer;
  }
};

Supla::Suplet::ThermometerGroupConfig makeConfig(
    Supla::Suplet::ThermometerGroupMode mode) {
  Supla::Suplet::ThermometerGroupConfig config = {};
  config.mode = mode;
  config.refreshIntervalMs = 200;
  config.sourceCount = 3;
  config.sourceChannels[0] = 2;
  config.sourceChannels[1] = 4;
  config.sourceChannels[2] = 6;
  return config;
}

}  // namespace

TEST(SupletThermometerGroupConfigTests, SerializesAndParsesConfig) {
  auto config = makeConfig(Supla::Suplet::ThermometerGroupMode::Max);
  uint8_t buffer[SUPLA_SUPLET_MAX_CONFIG_SIZE] = {};
  uint16_t writtenSize = 0;

  ASSERT_TRUE(Supla::Suplet::serializeThermometerGroupConfig(
      config, buffer, sizeof(buffer), &writtenSize));
  EXPECT_GT(writtenSize, 0);

  Supla::Suplet::ThermometerGroupConfig parsed = {};
  ASSERT_TRUE(
      Supla::Suplet::parseThermometerGroupConfig(buffer, writtenSize, &parsed));
  EXPECT_EQ(parsed.mode, Supla::Suplet::ThermometerGroupMode::Max);
  EXPECT_EQ(parsed.refreshIntervalMs, 200);
  EXPECT_EQ(parsed.sourceCount, 3);
  EXPECT_EQ(parsed.sourceChannels[0], 2);
  EXPECT_EQ(parsed.sourceChannels[1], 4);
  EXPECT_EQ(parsed.sourceChannels[2], 6);
}

TEST(SupletThermometerGroupConfigTests, RejectsInvalidConfig) {
  Supla::Suplet::ThermometerGroupConfig config = {};
  uint8_t buffer[SUPLA_SUPLET_MAX_CONFIG_SIZE] = {};

  EXPECT_FALSE(Supla::Suplet::serializeThermometerGroupConfig(
      config, buffer, sizeof(buffer)));

  config = makeConfig(Supla::Suplet::ThermometerGroupMode::Avg);
  config.sourceChannels[1] = -1;
  EXPECT_FALSE(Supla::Suplet::serializeThermometerGroupConfig(
      config, buffer, sizeof(buffer)));

  EXPECT_FALSE(Supla::Suplet::parseThermometerGroupConfig(
      buffer, sizeof(buffer), &config));
}

TEST_F(SupletThermometerGroupFixture, CalculatesAverageMinAndMax) {
  addSource(2, 21.5);
  addSource(4, TEMPERATURE_NOT_AVAILABLE);
  addSource(6, 25.5);

  auto avgConfig = makeConfig(Supla::Suplet::ThermometerGroupMode::Avg);
  auto avg = new Supla::Suplet::ThermometerGroup(2, 10, avgConfig);
  EXPECT_DOUBLE_EQ(avg->calculateValue(), 23.5);

  auto minConfig = makeConfig(Supla::Suplet::ThermometerGroupMode::Min);
  auto min = new Supla::Suplet::ThermometerGroup(2, 11, minConfig);
  EXPECT_DOUBLE_EQ(min->calculateValue(), 21.5);

  auto maxConfig = makeConfig(Supla::Suplet::ThermometerGroupMode::Max);
  auto max = new Supla::Suplet::ThermometerGroup(2, 12, maxConfig);
  EXPECT_DOUBLE_EQ(max->calculateValue(), 25.5);
}

TEST_F(SupletThermometerGroupFixture, IgnoresOfflineSources) {
  addSource(2, 10.0, false);
  addSource(4, 20.0);
  addSource(6, 30.0, false);

  auto config = makeConfig(Supla::Suplet::ThermometerGroupMode::Avg);
  auto group = new Supla::Suplet::ThermometerGroup(2, 10, config);
  EXPECT_DOUBLE_EQ(group->calculateValue(), 20.0);

  time.advance(201);
  group->iterateAlways();
  ASSERT_NE(group->getChannel(), nullptr);
  EXPECT_TRUE(group->getChannel()->isStateOnline());
  EXPECT_DOUBLE_EQ(group->getChannel()->getValueDouble(), 20.0);
}

TEST_F(SupletThermometerGroupFixture, GoesOfflineWhenAllSourcesAreOffline) {
  addSource(2, 10.0, false);
  addSource(4, 20.0, false);
  addSource(6, 30.0, false);

  auto config = makeConfig(Supla::Suplet::ThermometerGroupMode::Avg);
  auto group = new Supla::Suplet::ThermometerGroup(2, 10, config);

  time.advance(201);
  group->iterateAlways();
  ASSERT_NE(group->getChannel(), nullptr);
  EXPECT_FALSE(group->getChannel()->isStateOnline());
  EXPECT_LE(group->getChannel()->getValueDouble(), TEMPERATURE_NOT_AVAILABLE);
}

TEST_F(SupletThermometerGroupFixture,
       RuntimeCreatesGroupFromDefinitionAndConfig) {
  addSource(2, 18.0);
  addSource(4, 20.0);
  addSource(6, 22.0);

  Supla::Suplet::ChannelDefinition channel = {
      Supla::Suplet::channelKeyFromString("avg_temperature"),
      Supla::Suplet::ChannelKind::VirtualThermometer,
      SUPLA_CHANNELFNC_THERMOMETER,
      nullptr};
  Supla::Suplet::Definition definition = {};
  definition.definitionId = 700;
  definition.definitionVersion = 1;
  definition.category = Supla::Suplet::Category::Aggregate;
  definition.kind = Supla::Suplet::Kind::ThermometerGroup;
  definition.channels = &channel;
  definition.channelCount = 1;
  ASSERT_TRUE(Supla::Suplet::Runtime::validateDefinition(definition));

  Supla::Suplet::InstanceRecord instance = {};
  instance.instanceId = 1;
  instance.definitionId = definition.definitionId;
  instance.definitionVersion = definition.definitionVersion;
  instance.subDeviceId = 3;
  instance.state = Supla::Suplet::InstanceState::Active;
  ASSERT_TRUE(instance.channelMap.add(channel.channelKey, 8));

  auto config = makeConfig(Supla::Suplet::ThermometerGroupMode::Avg);
  ASSERT_TRUE(Supla::Suplet::serializeThermometerGroupConfig(
      config, instance.config, sizeof(instance.config), &instance.configSize));

  Supla::Element *created[1] = {};
  ASSERT_TRUE(
      Supla::Suplet::Runtime::createElements(definition, instance, created, 1));
  ASSERT_NE(created[0], nullptr);
  EXPECT_EQ(created[0]->getChannelNumber(), 8);
  EXPECT_EQ(created[0]->getChannel()->getSubDeviceId(), 3);

  time.advance(201);
  created[0]->iterateAlways();
  EXPECT_DOUBLE_EQ(created[0]->getChannel()->getValueDouble(), 20.0);
}

TEST_F(SupletThermometerGroupFixture, RuntimeRejectsMissingGroupConfig) {
  Supla::Suplet::ChannelDefinition channel = {
      Supla::Suplet::channelKeyFromString("avg_temperature"),
      Supla::Suplet::ChannelKind::VirtualThermometer,
      SUPLA_CHANNELFNC_THERMOMETER,
      nullptr};
  Supla::Suplet::Definition definition = {};
  definition.definitionId = 701;
  definition.definitionVersion = 1;
  definition.category = Supla::Suplet::Category::Aggregate;
  definition.kind = Supla::Suplet::Kind::ThermometerGroup;
  definition.channels = &channel;
  definition.channelCount = 1;

  Supla::Suplet::InstanceRecord instance = {};
  instance.instanceId = 1;
  instance.subDeviceId = 3;
  ASSERT_TRUE(instance.channelMap.add(channel.channelKey, 8));

  Supla::Element *created[1] = {};
  EXPECT_FALSE(
      Supla::Suplet::Runtime::createElements(definition, instance, created, 1));
  EXPECT_EQ(created[0], nullptr);
}

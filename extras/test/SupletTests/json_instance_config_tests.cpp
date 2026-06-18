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
#include <string.h>
#include <supla/channels/channel.h>
#include <supla/element.h>
#include <supla/storage/config.h>
#include <supla/suplet/json_definition.h>
#include <supla/suplet/json_instance_config.h>
#include <supla/suplet/manager.h>
#include <supla/suplet/runtime.h>
#include <supla/suplet/thermometer_group.h>
#include <supla/suplet/virtual_channel.h>

namespace {

class NoopConfig : public Supla::Config {
 public:
  bool init() override {
    return true;
  }
  void removeAll() override {
  }
  bool setString(const char *, const char *) override {
    return false;
  }
  bool getString(const char *, char *, size_t) override {
    return false;
  }
  int getStringSize(const char *) override {
    return -1;
  }
  bool setBlob(const char *, const char *, size_t) override {
    return true;
  }
  bool getBlob(const char *, char *, size_t) override {
    return false;
  }
  int getBlobSize(const char *) override {
    return -1;
  }
  bool getInt8(const char *, int8_t *) override {
    return false;
  }
  bool getUInt8(const char *, uint8_t *) override {
    return false;
  }
  bool getInt32(const char *, int32_t *) override {
    return false;
  }
  bool getUInt32(const char *, uint32_t *) override {
    return false;
  }
  bool setInt8(const char *, const int8_t) override {
    return false;
  }
  bool setUInt8(const char *, const uint8_t) override {
    return true;
  }
  bool setInt32(const char *, const int32_t) override {
    return false;
  }
  bool setUInt32(const char *, const uint32_t) override {
    return false;
  }
  bool eraseKey(const char *) override {
    return true;
  }
};

class SupletJsonInstanceFixture : public testing::Test {
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
                                               double value) {
    auto thermometer = new Supla::Suplet::VirtualThermometer(1, channelNumber);
    thermometer->setValue(value);
    thermometer->getChannel()->setNewValue(value);
    return thermometer;
  }
};

const char thermometerGroupDefinitionJson[] =
"{"
"\"definitionId\":900,"
"\"definitionVersion\":4,"
"\"category\":\"aggregate\","
"\"kind\":\"thermometerGroup\","
"\"channels\":["
"{\"key\":\"temperature\",\"kind\":\"virtualThermometer\","
"\"function\":\"thermometer\"}"
"]"
"}";

}  // namespace

TEST(SupletJsonInstanceConfigTests, ParsesThermometerGroupInstanceConfig) {
  Supla::Suplet::JsonDefinition jsonDefinition;
  ASSERT_TRUE(Supla::Suplet::JsonDefinitionParser::parse(
      thermometerGroupDefinitionJson, &jsonDefinition));

  const char instanceJson[] =
      "{"
      "\"instanceId\":123,"
      "\"definitionId\":900,"
      "\"definitionVersion\":4,"
      "\"state\":\"active\","
      "\"subDeviceId\":17,"
      "\"config\":{"
      "\"mode\":\"min\","
      "\"refreshIntervalMs\":250,"
      "\"sourceChannels\":[2,4,6],"
      "\"ignored\":{\"future\":true}"
      "}"
      "}";

  Supla::Suplet::InstanceRecord record = {};
  ASSERT_TRUE(Supla::Suplet::JsonInstanceConfigParser::parse(
      instanceJson, *jsonDefinition.getDefinition(), &record));
  EXPECT_EQ(record.instanceId, 123u);
  EXPECT_EQ(record.definitionId, 900u);
  EXPECT_EQ(record.definitionVersion, 4u);
  EXPECT_EQ(record.state, Supla::Suplet::InstanceState::Active);
  EXPECT_EQ(record.subDeviceId, 17);
  EXPECT_GT(record.configSize, 0);

  Supla::Suplet::ThermometerGroupConfig config = {};
  ASSERT_TRUE(Supla::Suplet::parseThermometerGroupConfig(
      record.config, record.configSize, &config));
  EXPECT_EQ(config.mode, Supla::Suplet::ThermometerGroupMode::Min);
  EXPECT_EQ(config.refreshIntervalMs, 250);
  EXPECT_EQ(config.sourceCount, 3);
  EXPECT_EQ(config.sourceChannels[0], 2);
  EXPECT_EQ(config.sourceChannels[1], 4);
  EXPECT_EQ(config.sourceChannels[2], 6);
}

TEST(SupletJsonInstanceConfigTests, RejectsMismatchedDefinitionAndBadConfig) {
  Supla::Suplet::JsonDefinition jsonDefinition;
  ASSERT_TRUE(Supla::Suplet::JsonDefinitionParser::parse(
      thermometerGroupDefinitionJson, &jsonDefinition));
  Supla::Suplet::InstanceRecord record = {};

  EXPECT_FALSE(Supla::Suplet::JsonInstanceConfigParser::parse(
      "{\"instanceId\":1,\"definitionId\":901,\"definitionVersion\":4,"
      "\"config\":{\"mode\":\"avg\",\"sourceChannels\":[1]}}",
      *jsonDefinition.getDefinition(),
      &record));
  EXPECT_FALSE(Supla::Suplet::JsonInstanceConfigParser::parse(
      "{\"instanceId\":1,\"definitionId\":900,\"definitionVersion\":4,"
      "\"config\":{\"mode\":\"median\",\"sourceChannels\":[1]}}",
      *jsonDefinition.getDefinition(),
      &record));
  EXPECT_FALSE(Supla::Suplet::JsonInstanceConfigParser::parse(
      "{\"instanceId\":1,\"definitionId\":900,\"definitionVersion\":4,"
      "\"config\":{\"mode\":\"avg\",\"sourceChannels\":[]}}",
      *jsonDefinition.getDefinition(),
      &record));
}

TEST_F(SupletJsonInstanceFixture, BuildsRuntimeFromDefinitionAndInstanceJson) {
  addSource(2, 10.0);
  addSource(4, 20.0);
  addSource(6, 30.0);

  Supla::Suplet::JsonDefinition jsonDefinition;
  ASSERT_TRUE(Supla::Suplet::JsonDefinitionParser::parse(
      thermometerGroupDefinitionJson, &jsonDefinition));

  const char instanceJson[] =
      "{"
      "\"instanceId\":77,"
      "\"definitionId\":900,"
      "\"definitionVersion\":4,"
      "\"config\":{"
      "\"mode\":\"avg\","
      "\"refreshIntervalMs\":100,"
      "\"sourceChannels\":[2,4,6]"
      "}"
      "}";
  Supla::Suplet::InstanceRecord instance = {};
  ASSERT_TRUE(Supla::Suplet::JsonInstanceConfigParser::parse(
      instanceJson, *jsonDefinition.getDefinition(), &instance));

  NoopConfig config;
  Supla::Suplet::Manager manager(&config);
  Supla::Suplet::ChannelAllocator occupied;
  ASSERT_TRUE(occupied.markOccupied(0));
  ASSERT_TRUE(occupied.markOccupied(2));
  ASSERT_TRUE(occupied.markOccupied(4));
  ASSERT_TRUE(occupied.markOccupied(6));
  ASSERT_TRUE(manager.addInstanceFromDefinition(
      instance, *jsonDefinition.getDefinition(), occupied));

  const auto *record = manager.getInstanceTable()->findByInstanceId(77);
  ASSERT_NE(record, nullptr);
  EXPECT_NE(record->subDeviceId, 0);
  EXPECT_EQ(record->channelMap.getChannelNumber(
                Supla::Suplet::channelKeyFromString("temperature")),
            1);

  Supla::Element *created[1] = {};
  ASSERT_TRUE(Supla::Suplet::Runtime::createElements(
      *jsonDefinition.getDefinition(), *record, created, 1));

  time.advance(101);
  created[0]->iterateAlways();
  EXPECT_DOUBLE_EQ(created[0]->getChannel()->getValueDouble(), 20.0);
}

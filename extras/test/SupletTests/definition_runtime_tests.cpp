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
#include <supla/control/virtual_relay.h>
#include <supla/element.h>
#include <supla/sensor/virtual_binary.h>
#include <supla/storage/config.h>
#include <supla/suplet/definition.h>
#include <supla/suplet/manager.h>
#include <supla/suplet/runtime.h>

namespace {

class SupletRuntimeFixture : public testing::Test {
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
};

}  // namespace

TEST(SupletDefinitionTests, ExtractsAndValidatesRequiredChannelIds) {
  Supla::Suplet::ChannelDefinition channels[] = {
      {10, Supla::Suplet::ChannelKind::VirtualRelay, 0, nullptr},
      {20, Supla::Suplet::ChannelKind::VirtualBinarySensor, 0, nullptr},
  };
  Supla::Suplet::Definition definition = {};
  definition.definitionId = 1;
  definition.definitionVersion = 1;
  definition.category = Supla::Suplet::Category::Virtual;
  definition.kind = Supla::Suplet::Kind::VirtualRelay;
  definition.channels = channels;
  definition.channelCount = 2;
  uint8_t keys[2] = {};

  EXPECT_TRUE(Supla::Suplet::Runtime::validateDefinition(definition));
  EXPECT_TRUE(Supla::Suplet::getRequiredChannelIds(definition, keys, 2));
  EXPECT_EQ(keys[0], 10u);
  EXPECT_EQ(keys[1], 20u);

  channels[1].channelId = 10;
  EXPECT_FALSE(Supla::Suplet::Runtime::validateDefinition(definition));
}

TEST_F(SupletRuntimeFixture, CreatesVirtualRelayWithStableChannelAndSubdevice) {
  Supla::Suplet::ChannelDefinition channel = {
      1,
      Supla::Suplet::ChannelKind::VirtualRelay,
      SUPLA_CHANNELFNC_POWERSWITCH,
      nullptr};
  Supla::Suplet::Definition definition = {};
  definition.definitionId = 1;
  definition.definitionVersion = 1;
  definition.category = Supla::Suplet::Category::Virtual;
  definition.kind = Supla::Suplet::Kind::VirtualRelay;
  definition.channels = &channel;
  definition.channelCount = 1;
  Supla::Suplet::InstanceRecord instance = {};
  instance.instanceId = 11;
  instance.subDeviceId = 7;
  ASSERT_TRUE(instance.channelMap.add(channel.channelId, 4));

  Supla::Element *created[1] = {};
  ASSERT_TRUE(
      Supla::Suplet::Runtime::createElements(definition, instance, created, 1));
  ASSERT_NE(created[0], nullptr);

  EXPECT_EQ(created[0]->getChannelNumber(), 4);
  EXPECT_EQ(created[0]->getChannel()->getSubDeviceId(), 7);
  EXPECT_TRUE(created[0]->isOwnerOfSubDeviceId(7));
  EXPECT_EQ(Supla::Element::getOwnerOfSubDeviceId(7), created[0]);

  auto relay = reinterpret_cast<Supla::Control::VirtualRelay *>(created[0]);
  EXPECT_FALSE(relay->isOn());
  relay->turnOn();
  EXPECT_TRUE(relay->isOn());
}

TEST_F(SupletRuntimeFixture, AppliesChannelCaptionFromDefinition) {
  Supla::Suplet::ChannelDefinition channel = {
      1,
      Supla::Suplet::ChannelKind::VirtualRelay,
      SUPLA_CHANNELFNC_POWERSWITCH,
      "Suplet relay"};
  Supla::Suplet::Definition definition = {};
  definition.definitionId = 5;
  definition.definitionVersion = 1;
  definition.category = Supla::Suplet::Category::Virtual;
  definition.kind = Supla::Suplet::Kind::VirtualRelay;
  definition.channels = &channel;
  definition.channelCount = 1;
  Supla::Suplet::InstanceRecord instance = {};
  instance.instanceId = 15;
  instance.subDeviceId = 10;
  ASSERT_TRUE(instance.channelMap.add(channel.channelId, 8));

  Supla::Element *created[1] = {};
  ASSERT_TRUE(
      Supla::Suplet::Runtime::createElements(definition, instance, created, 1));
  ASSERT_NE(created[0], nullptr);
  ASSERT_NE(created[0]->getChannel(), nullptr);
  ASSERT_TRUE(created[0]->getChannel()->isInitialCaptionSet());
  EXPECT_STREQ(created[0]->getChannel()->getInitialCaption(), "Suplet relay");
}

TEST_F(SupletRuntimeFixture,
       CreatesVirtualBinaryWithStableChannelAndSubdevice) {
  Supla::Suplet::ChannelDefinition channel = {
      2,
      Supla::Suplet::ChannelKind::VirtualBinarySensor,
      SUPLA_CHANNELFNC_OPENINGSENSOR_DOOR,
      nullptr};
  Supla::Suplet::Definition definition = {};
  definition.definitionId = 2;
  definition.definitionVersion = 1;
  definition.category = Supla::Suplet::Category::Virtual;
  definition.kind = Supla::Suplet::Kind::VirtualBinarySensor;
  definition.channels = &channel;
  definition.channelCount = 1;
  Supla::Suplet::InstanceRecord instance = {};
  instance.instanceId = 12;
  instance.subDeviceId = 8;
  ASSERT_TRUE(instance.channelMap.add(channel.channelId, 6));

  Supla::Element *created[1] = {};
  ASSERT_TRUE(
      Supla::Suplet::Runtime::createElements(definition, instance, created, 1));
  ASSERT_NE(created[0], nullptr);

  EXPECT_EQ(created[0]->getChannelNumber(), 6);
  EXPECT_EQ(created[0]->getChannel()->getSubDeviceId(), 8);
  EXPECT_TRUE(created[0]->isOwnerOfSubDeviceId(8));

  auto sensor = reinterpret_cast<Supla::Sensor::VirtualBinary *>(created[0]);
  EXPECT_FALSE(sensor->getValue());
  sensor->set();
  EXPECT_TRUE(sensor->getValue());
}

TEST_F(SupletRuntimeFixture, CreateElementsRollsBackOnUnsupportedChannelKind) {
  Supla::Suplet::ChannelDefinition channels[] = {
      {10, Supla::Suplet::ChannelKind::VirtualRelay, 0, nullptr},
      {20, Supla::Suplet::ChannelKind::Unknown, 0, nullptr},
  };
  Supla::Suplet::Definition definition = {};
  definition.definitionId = 3;
  definition.definitionVersion = 1;
  definition.category = Supla::Suplet::Category::Virtual;
  definition.kind = Supla::Suplet::Kind::VirtualRelay;
  definition.channels = channels;
  definition.channelCount = 2;
  Supla::Suplet::InstanceRecord instance = {};
  instance.instanceId = 13;
  instance.subDeviceId = 9;
  ASSERT_TRUE(instance.channelMap.add(10, 1));
  ASSERT_TRUE(instance.channelMap.add(20, 2));
  Supla::Element *created[2] = {};

  EXPECT_FALSE(
      Supla::Suplet::Runtime::createElements(definition, instance, created, 2));
  EXPECT_EQ(created[0], nullptr);
  EXPECT_EQ(created[1], nullptr);
  EXPECT_EQ(Supla::Element::begin(), nullptr);
}

TEST_F(SupletRuntimeFixture, ManagerAddsDefinitionAndRuntimeCreatesChannels) {
  Supla::Suplet::ChannelDefinition channels[] = {
      {1,
       Supla::Suplet::ChannelKind::VirtualRelay,
       SUPLA_CHANNELFNC_POWERSWITCH,
       nullptr},
      {2,
       Supla::Suplet::ChannelKind::VirtualBinarySensor,
       SUPLA_CHANNELFNC_OPENINGSENSOR_DOOR,
       nullptr},
  };
  Supla::Suplet::Definition definition = {};
  definition.definitionId = 77;
  definition.definitionVersion = 3;
  definition.category = Supla::Suplet::Category::Virtual;
  definition.kind = Supla::Suplet::Kind::VirtualRelay;
  definition.channels = channels;
  definition.channelCount = 2;

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
  } config;

  Supla::Suplet::Manager manager(&config);
  Supla::Suplet::ChannelAllocator occupied;
  ASSERT_TRUE(occupied.markOccupied(0));
  ASSERT_TRUE(occupied.markOccupied(2));

  Supla::Suplet::InstanceRecord instance = {};
  instance.instanceId = 22;
  ASSERT_TRUE(
      manager.addInstanceFromDefinition(instance, definition, occupied));

  auto record = manager.getInstanceTable()->findByInstanceId(22);
  ASSERT_NE(record, nullptr);
  EXPECT_EQ(record->definitionId, 77u);
  EXPECT_EQ(record->definitionVersion, 3u);
  EXPECT_NE(record->subDeviceId, 0);
  EXPECT_EQ(record->channelMap.getChannelNumber(channels[0].channelId), 1);
  EXPECT_EQ(record->channelMap.getChannelNumber(channels[1].channelId), 3);

  Supla::Element *created[2] = {};
  ASSERT_TRUE(
      Supla::Suplet::Runtime::createElements(definition, *record, created, 2));
  EXPECT_EQ(created[0]->getChannelNumber(), 1);
  EXPECT_EQ(created[1]->getChannelNumber(), 3);
  EXPECT_EQ(created[0]->getChannel()->getSubDeviceId(), record->subDeviceId);
  EXPECT_EQ(created[1]->getChannel()->getSubDeviceId(), record->subDeviceId);
}

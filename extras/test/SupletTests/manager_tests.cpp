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
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <supla/channels/channel.h>
#include <supla/element.h>
#include <supla/storage/config.h>
#include <supla/suplet/manager.h>
#include <supla/suplet/registry.h>

#include <map>
#include <string>
#include <vector>

namespace {

class InMemoryConfig : public Supla::Config {
 public:
  bool init() override {
    return true;
  }
  void removeAll() override {
    blobs.clear();
    uint8Values.clear();
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
  bool setBlob(const char *key, const char *value, size_t blobSize) override {
    if (key == nullptr || value == nullptr) {
      return false;
    }
    blobs[key] = std::vector<char>(value, value + blobSize);
    return true;
  }
  bool getBlob(const char *key, char *value, size_t blobSize) override {
    if (key == nullptr || value == nullptr || blobs.count(key) == 0 ||
        blobs[key].size() != blobSize) {
      return false;
    }
    memcpy(value, blobs[key].data(), blobSize);
    return true;
  }
  int getBlobSize(const char *key) override {
    if (key == nullptr || blobs.count(key) == 0) {
      return -1;
    }
    return blobs[key].size();
  }
  bool getInt8(const char *, int8_t *) override {
    return false;
  }
  bool getUInt8(const char *key, uint8_t *result) override {
    if (key == nullptr || result == nullptr || uint8Values.count(key) == 0) {
      return false;
    }
    *result = uint8Values[key];
    return true;
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
  bool setUInt8(const char *key, const uint8_t value) override {
    if (key == nullptr) {
      return false;
    }
    uint8Values[key] = value;
    return true;
  }
  bool setInt32(const char *, const int32_t) override {
    return false;
  }
  bool setUInt32(const char *, const uint32_t) override {
    return false;
  }
  bool eraseKey(const char *key) override {
    if (key == nullptr) {
      return false;
    }
    bool erased = blobs.erase(key) > 0;
    erased = (uint8Values.erase(key) > 0) || erased;
    return erased;
  }
  void commit() override {
    commitCount++;
  }

  std::map<std::string, std::vector<char>> blobs;
  std::map<std::string, uint8_t> uint8Values;
  int commitCount = 0;
};

class SubDeviceChannelOwner {
 public:
  explicit SubDeviceChannelOwner(uint8_t subDeviceId) {
    channel.setSubDeviceId(subDeviceId);
  }

  Supla::Channel channel;
};

Supla::Suplet::InstanceRecord makeRecord(uint32_t instanceId,
                                         uint8_t subDeviceId,
                                         int channelA,
                                         int channelB) {
  Supla::Suplet::InstanceRecord record = {};
  record.instanceId = instanceId;
  record.definitionId = 100 + instanceId;
  record.definitionVersion = 1;
  record.subDeviceId = subDeviceId;
  record.channelMap.add(1, channelA);
  record.channelMap.add(2, channelB);
  return record;
}

}  // namespace

TEST(SupletManagerTests, AddInstancePersistsTable) {
  InMemoryConfig config;
  Supla::Suplet::Manager manager(&config);

  EXPECT_TRUE(manager.addInstance(makeRecord(1, 2, 4, 8)));

  Supla::Suplet::Manager loaded(&config);
  EXPECT_TRUE(loaded.load());
  ASSERT_EQ(loaded.getInstanceTable()->getCount(), 1);
  auto record = loaded.getInstanceTable()->findByInstanceId(1);
  ASSERT_NE(record, nullptr);
  EXPECT_EQ(record->subDeviceId, 1);
  EXPECT_EQ(record->channelMap.getChannelNumber(1), 4);
  EXPECT_EQ(record->channelMap.getChannelNumber(2), 8);
}

TEST(SupletManagerTests, LoadKeepsInstanceConfigOutOfIndexTable) {
  InMemoryConfig config;
  Supla::Suplet::Manager manager(&config);
  Supla::Suplet::InstanceRecord record = {};
  record.instanceId = 1;
  record.definitionId = 1000;
  record.definitionVersion = 1;
  record.subDeviceId = 1;
  const uint8_t params[] = {'{', '}'};
  ASSERT_TRUE(record.setConfig(params, sizeof(params)));
  ASSERT_TRUE(manager.addInstance(record));

  Supla::Suplet::Manager loadedManager(&config);
  ASSERT_TRUE(loadedManager.load());
  auto table = loadedManager.getInstanceTable();
  ASSERT_NE(table, nullptr);
  auto loaded = table->findByInstanceId(1);
  ASSERT_NE(loaded, nullptr);
  EXPECT_EQ(loaded->configSize, sizeof(params));
  EXPECT_EQ(loaded->config, nullptr);

  Supla::Suplet::InstanceRecord fullRecord;
  ASSERT_TRUE(loadedManager.loadInstance(1, &fullRecord));
  ASSERT_NE(fullRecord.config, nullptr);
  EXPECT_EQ(fullRecord.configSize, sizeof(params));
  EXPECT_EQ(memcmp(fullRecord.config, params, sizeof(params)), 0);
}

TEST(SupletManagerTests, AddInstanceFromDefinitionDoesNotPreallocateChannels) {
  InMemoryConfig config;
  Supla::Suplet::Manager manager(&config);
  Supla::Channel occupied0(0);

  Supla::Suplet::ChannelDefinition channels[] = {
      {1, Supla::Suplet::ChannelKind::VirtualRelay},
      {2, Supla::Suplet::ChannelKind::VirtualBinarySensor},
  };
  Supla::Suplet::Definition definition = {};
  definition.category = Supla::Suplet::Category::Virtual;
  definition.kind = Supla::Suplet::Kind::VirtualRelay;
  definition.definitionId = 102;
  definition.definitionVersion = 1;
  definition.channels = channels;
  definition.channelCount = 2;
  Supla::Suplet::InstanceRecord newRecord = {};
  newRecord.instanceId = 2;

  EXPECT_TRUE(manager.addInstanceFromDefinition(newRecord, definition));

  auto record = manager.getInstanceTable()->findByInstanceId(2);
  ASSERT_NE(record, nullptr);
  EXPECT_EQ(record->subDeviceId, 2);
  EXPECT_EQ(record->definitionId, 102u);
  EXPECT_EQ(record->definitionVersion, 1u);
  EXPECT_EQ(record->channelMap.getChannelNumber(1),
            Supla::Suplet::kInvalidChannelNumber);
  EXPECT_EQ(record->channelMap.getChannelNumber(2),
            Supla::Suplet::kInvalidChannelNumber);
}

TEST(SupletManagerTests, SubDeviceAllocationSkipsExistingChannelSubdevices) {
  Supla::Channel::resetToDefaults();
  SubDeviceChannelOwner owner1(1);
  SubDeviceChannelOwner owner3(3);
  InMemoryConfig config;
  Supla::Suplet::Manager manager(&config);

  EXPECT_EQ(manager.getFirstFreeSubDeviceId(), 2);

  Supla::Suplet::InstanceRecord record = makeRecord(0, 0, 4, 8);
  EXPECT_TRUE(manager.addInstance(record));

  auto stored = manager.getInstanceTable()->findByInstanceId(2);
  ASSERT_NE(stored, nullptr);
  EXPECT_EQ(stored->subDeviceId, 2);
  EXPECT_EQ(manager.getFirstFreeSubDeviceId(), 4);
  Supla::Channel::resetToDefaults();
}

TEST(SupletManagerTests, ConflictMissingAllSubdeviceChannelsRemovesSuplet) {
  InMemoryConfig config;
  Supla::Suplet::Manager manager(&config);
  ASSERT_TRUE(manager.addInstance(makeRecord(1, 2, 4, 8)));
  ASSERT_TRUE(manager.addInstance(makeRecord(2, 3, 5, 9)));

  uint8_t report[10] = {};
  report[5] = CHANNEL_REPORT_CHANNEL_REGISTERED;
  report[9] = CHANNEL_REPORT_CHANNEL_REGISTERED;

  EXPECT_FALSE(manager.onChannelConflictReport(
      report, sizeof(report), false, true, false));

  EXPECT_EQ(manager.getInstanceTable()->findByInstanceId(1), nullptr);
  EXPECT_NE(manager.getInstanceTable()->findByInstanceId(2), nullptr);

  Supla::Suplet::Manager loaded(&config);
  ASSERT_TRUE(loaded.load());
  EXPECT_EQ(loaded.getInstanceTable()->findByInstanceId(1), nullptr);
  EXPECT_NE(loaded.getInstanceTable()->findByInstanceId(2), nullptr);
}

TEST(SupletManagerTests, ConflictMissingPartialSubdeviceDoesNotRemoveSuplet) {
  InMemoryConfig config;
  Supla::Suplet::Manager manager(&config);
  ASSERT_TRUE(manager.addInstance(makeRecord(1, 2, 4, 8)));

  uint8_t report[9] = {};
  report[4] = CHANNEL_REPORT_CHANNEL_REGISTERED;

  EXPECT_FALSE(manager.onChannelConflictReport(
      report, sizeof(report), false, true, false));

  EXPECT_NE(manager.getInstanceTable()->findByInstanceId(1), nullptr);
}

TEST(SupletManagerTests, InvalidTypeAndMissingOnDeviceDoNotRemoveSuplets) {
  InMemoryConfig config;
  Supla::Suplet::Manager manager(&config);
  ASSERT_TRUE(manager.addInstance(makeRecord(1, 2, 4, 8)));
  uint8_t report[1] = {};

  EXPECT_FALSE(manager.onChannelConflictReport(
      report, sizeof(report), true, true, false));
  EXPECT_NE(manager.getInstanceTable()->findByInstanceId(1), nullptr);

  EXPECT_FALSE(manager.onChannelConflictReport(
      report, sizeof(report), false, true, true));
  EXPECT_NE(manager.getInstanceTable()->findByInstanceId(1), nullptr);
}

TEST(SupletManagerTests, CreatesRuntimeElementsForStoredInstances) {
  SimpleTime time;
  Supla::Channel::resetToDefaults();
  InMemoryConfig config;
  Supla::Suplet::Manager manager(&config);

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
  definition.definitionId = 500;
  definition.definitionVersion = 2;
  definition.category = Supla::Suplet::Category::Virtual;
  definition.kind = Supla::Suplet::Kind::VirtualRelay;
  definition.channels = channels;
  definition.channelCount = 2;

  Supla::Suplet::Registry registry;
  ASSERT_TRUE(registry.add(&definition, 4));

  for (uint8_t i = 0; i < 4; i++) {
    Supla::Suplet::InstanceRecord record = {};
    record.instanceId = i + 1;
    record.definitionId = 500;
    record.definitionVersion = 2;
    record.subDeviceId = i + 1;
    ASSERT_TRUE(record.channelMap.add(channels[0].channelId, 4 + i * 2));
    ASSERT_TRUE(record.channelMap.add(channels[1].channelId, 5 + i * 2));
    ASSERT_TRUE(manager.addInstance(record));
  }

  Supla::Element *created[8] = {};
  uint16_t createdCount = 0;
  EXPECT_TRUE(manager.createElementsFromRegistry(
      registry, created, sizeof(created) / sizeof(created[0]), &createdCount));
  ASSERT_EQ(createdCount, 8);
  EXPECT_EQ(created[0]->getChannelNumber(), 4);
  EXPECT_EQ(created[1]->getChannelNumber(), 5);
  EXPECT_EQ(created[6]->getChannelNumber(), 10);
  EXPECT_EQ(created[7]->getChannelNumber(), 11);
  EXPECT_EQ(created[0]->getChannel()->getSubDeviceId(), 1);
  EXPECT_EQ(created[1]->getChannel()->getSubDeviceId(), 1);
  EXPECT_EQ(created[6]->getChannel()->getSubDeviceId(), 4);
  EXPECT_EQ(created[7]->getChannel()->getSubDeviceId(), 4);

  while (Supla::Element::begin() != nullptr) {
    delete Supla::Element::begin();
  }
  Supla::Channel::resetToDefaults();
}

TEST(SupletManagerTests, OwnsRuntimeElementsAndDeletesThem) {
  SimpleTime time;
  Supla::Channel::resetToDefaults();
  InMemoryConfig config;
  Supla::Suplet::Manager manager(&config);

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
  definition.definitionId = 501;
  definition.definitionVersion = 1;
  definition.category = Supla::Suplet::Category::Virtual;
  definition.kind = Supla::Suplet::Kind::VirtualRelay;
  definition.channels = channels;
  definition.channelCount = 2;

  Supla::Suplet::Registry registry;
  ASSERT_TRUE(registry.add(&definition, 4));

  Supla::Suplet::InstanceRecord active = {};
  active.instanceId = 1;
  active.definitionId = 501;
  active.definitionVersion = 1;
  active.subDeviceId = 1;
  ASSERT_TRUE(active.channelMap.add(channels[0].channelId, 4));
  ASSERT_TRUE(active.channelMap.add(channels[1].channelId, 8));
  ASSERT_TRUE(manager.addInstance(active));

  ASSERT_TRUE(manager.loadRuntimeElementsFromRegistry(registry));
  EXPECT_EQ(manager.getRuntimeElementCount(), 2);
  EXPECT_NE(Supla::Element::begin(), nullptr);

  manager.deleteRuntimeElements();
  EXPECT_EQ(manager.getRuntimeElementCount(), 0);
  EXPECT_EQ(Supla::Element::begin(), nullptr);

  Supla::Channel::resetToDefaults();
}

TEST(SupletManagerTests, RuntimeCreationRollsBackWhenDefinitionIsMissing) {
  SimpleTime time;
  Supla::Channel::resetToDefaults();
  InMemoryConfig config;
  Supla::Suplet::Manager manager(&config);
  ASSERT_TRUE(manager.addInstance(makeRecord(1, 2, 4, 8)));

  Supla::Suplet::Registry registry;
  Supla::Element *created[2] = {};
  uint16_t createdCount = 99;

  EXPECT_FALSE(manager.createElementsFromRegistry(
      registry, created, sizeof(created) / sizeof(created[0]), &createdCount));
  EXPECT_EQ(createdCount, 0);
  EXPECT_EQ(Supla::Element::begin(), nullptr);

  Supla::Channel::resetToDefaults();
}

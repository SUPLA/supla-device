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
#include <stdint.h>
#include <string.h>
#include <map>
#include <string>
#include <vector>
#include <supla/storage/config.h>
#include <supla/suplet/assignment_applier.h>
#include <supla/suplet/thermometer_group.h>

namespace {

class InMemoryConfig : public Supla::Config {
 public:
  bool init() override { return true; }
  void removeAll() override {
    blobs.clear();
    uint8Values.clear();
  }
  bool setString(const char *, const char *) override { return false; }
  bool getString(const char *, char *, size_t) override { return false; }
  int getStringSize(const char *) override { return -1; }
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
  bool getInt8(const char *, int8_t *) override { return false; }
  bool getUInt8(const char *key, uint8_t *result) override {
    if (key == nullptr || result == nullptr || uint8Values.count(key) == 0) {
      return false;
    }
    *result = uint8Values[key];
    return true;
  }
  bool getInt32(const char *, int32_t *) override { return false; }
  bool getUInt32(const char *, uint32_t *) override { return false; }
  bool setInt8(const char *, const int8_t) override { return false; }
  bool setUInt8(const char *key, const uint8_t value) override {
    if (key == nullptr) {
      return false;
    }
    uint8Values[key] = value;
    return true;
  }
  bool setInt32(const char *, const int32_t) override { return false; }
  bool setUInt32(const char *, const uint32_t) override { return false; }
  bool eraseKey(const char *key) override {
    if (key == nullptr) {
      return false;
    }
    bool erased = blobs.erase(key) > 0;
    erased = (uint8Values.erase(key) > 0) || erased;
    return erased;
  }

  std::map<std::string, std::vector<char>> blobs;
  std::map<std::string, uint8_t> uint8Values;
};

Supla::Suplet::ChannelDefinition thermometerGroupChannel[] = {
    {1,
     Supla::Suplet::ChannelKind::VirtualThermometer,
     SUPLA_CHANNELFNC_THERMOMETER,
     nullptr},
};

Supla::Suplet::Definition makeThermometerGroupDefinition() {
  Supla::Suplet::Definition definition = {};
  definition.definitionId = 900;
  definition.definitionVersion = 4;
  definition.category = Supla::Suplet::Category::Aggregate;
  definition.kind = Supla::Suplet::Kind::ThermometerGroup;
  definition.channels = thermometerGroupChannel;
  definition.channelCount = 1;
  return definition;
}

const char instanceJsonAvg[] =
"{"
"\"instanceId\":77,"
"\"definitionId\":900,"
"\"definitionVersion\":4,"
"\"config\":{\"mode\":\"avg\",\"sourceChannels\":[2,4,6]}"
"}";

const char instanceJsonMax[] =
"{"
"\"instanceId\":77,"
"\"definitionId\":900,"
"\"definitionVersion\":4,"
"\"config\":{\"mode\":\"max\",\"sourceChannels\":[2,4,6]}"
"}";

const char instanceJsonSecond[] =
"{"
"\"instanceId\":78,"
"\"definitionId\":900,"
"\"definitionVersion\":4,"
"\"config\":{\"mode\":\"avg\",\"sourceChannels\":[2,4,6]}"
"}";

}  // namespace

TEST(SupletAssignmentApplierTests, AddsAssignmentAndPersistsIt) {
  InMemoryConfig config;
  Supla::Suplet::Manager manager(&config);
  Supla::Suplet::Registry registry;
  auto definition = makeThermometerGroupDefinition();
  ASSERT_TRUE(registry.add(&definition, 4));
  Supla::Suplet::AssignmentApplier applier(&manager, &registry);

  Supla::Suplet::ChannelAllocator occupied;
  ASSERT_TRUE(occupied.markOccupied(0));
  ASSERT_TRUE(occupied.markOccupied(2));
  ASSERT_TRUE(occupied.markOccupied(4));
  ASSERT_TRUE(occupied.markOccupied(6));

  EXPECT_EQ(applier.applyJson(instanceJsonAvg, 900, 4, occupied),
            Supla::Suplet::AssignmentResult::Applied);

  auto record = manager.getInstanceTable()->findByInstanceId(77);
  ASSERT_NE(record, nullptr);
  EXPECT_NE(record->subDeviceId, 0);
  EXPECT_EQ(record->channelMap.getChannelNumber(
                1),
            Supla::Suplet::kInvalidChannelNumber);

  Supla::Suplet::Manager loaded(&config);
  ASSERT_TRUE(loaded.load());
  EXPECT_NE(loaded.getInstanceTable()->findByInstanceId(77), nullptr);
}

TEST(SupletAssignmentApplierTests, UpdatesAssignmentWithoutChangingChannelMap) {
  InMemoryConfig config;
  Supla::Suplet::Manager manager(&config);
  Supla::Suplet::Registry registry;
  auto definition = makeThermometerGroupDefinition();
  ASSERT_TRUE(registry.add(&definition, 4));
  Supla::Suplet::AssignmentApplier applier(&manager, &registry);
  Supla::Suplet::ChannelAllocator occupied;
  ASSERT_TRUE(occupied.markOccupied(0));
  ASSERT_TRUE(occupied.markOccupied(2));
  ASSERT_TRUE(occupied.markOccupied(4));
  ASSERT_TRUE(occupied.markOccupied(6));

  ASSERT_EQ(applier.applyJson(instanceJsonAvg, 900, 4, occupied),
            Supla::Suplet::AssignmentResult::Applied);
  auto firstRecord = manager.getInstanceTable()->findByInstanceId(77);
  ASSERT_NE(firstRecord, nullptr);
  int firstChannel = firstRecord->channelMap.getChannelNumber(
      1);
  uint8_t firstSubdevice = firstRecord->subDeviceId;

  EXPECT_EQ(applier.applyJson(instanceJsonMax, 900, 4, occupied),
            Supla::Suplet::AssignmentResult::Applied);

  auto updatedRecord = manager.getInstanceTable()->findByInstanceId(77);
  ASSERT_NE(updatedRecord, nullptr);
  EXPECT_EQ(updatedRecord->subDeviceId, firstSubdevice);
  EXPECT_EQ(updatedRecord->channelMap.getChannelNumber(
                1),
            firstChannel);

  Supla::Suplet::ThermometerGroupConfig parsed = {};
  ASSERT_TRUE(Supla::Suplet::parseThermometerGroupConfig(
      updatedRecord->config, updatedRecord->configSize, &parsed));
  EXPECT_EQ(parsed.mode, Supla::Suplet::ThermometerGroupMode::Max);
}

TEST(SupletAssignmentApplierTests,
     EnforcesMaxInstancesButAllowsExistingInstanceUpdate) {
  InMemoryConfig config;
  Supla::Suplet::Manager manager(&config);
  Supla::Suplet::Registry registry;
  auto definition = makeThermometerGroupDefinition();
  ASSERT_TRUE(registry.add(&definition, 1));
  Supla::Suplet::AssignmentApplier applier(&manager, &registry);
  Supla::Suplet::ChannelAllocator occupied;

  EXPECT_EQ(applier.applyJson(instanceJsonAvg, 900, 4, occupied),
            Supla::Suplet::AssignmentResult::Applied);
  EXPECT_EQ(applier.validateJson(instanceJsonSecond, 900, 4, occupied),
            Supla::Suplet::AssignmentResult::InstanceLimitExceeded);
  EXPECT_EQ(applier.applyJson(instanceJsonSecond, 900, 4, occupied),
            Supla::Suplet::AssignmentResult::InstanceLimitExceeded);

  EXPECT_EQ(applier.validateJson(instanceJsonMax, 900, 4, occupied),
            Supla::Suplet::AssignmentResult::Applied);
  EXPECT_EQ(applier.applyJson(instanceJsonMax, 900, 4, occupied),
            Supla::Suplet::AssignmentResult::Applied);
}

TEST(SupletAssignmentApplierTests, RejectsUnsupportedDefinitionAndBadJson) {
  InMemoryConfig config;
  Supla::Suplet::Manager manager(&config);
  Supla::Suplet::Registry registry;
  auto definition = makeThermometerGroupDefinition();
  ASSERT_TRUE(registry.add(&definition, 4));
  Supla::Suplet::AssignmentApplier applier(&manager, &registry);
  Supla::Suplet::ChannelAllocator occupied;

  EXPECT_EQ(applier.applyJson(instanceJsonAvg, 901, 4, occupied),
            Supla::Suplet::AssignmentResult::DefinitionNotSupported);
  EXPECT_EQ(applier.applyJson(
                "{\"instanceId\":77,\"definitionId\":900,"
                "\"definitionVersion\":4,"
                "\"config\":{\"mode\":\"bad\",\"sourceChannels\":[2]}}",
                900,
                4,
                occupied),
            Supla::Suplet::AssignmentResult::InvalidConfig);
}

TEST(SupletAssignmentApplierTests, RemovesAssignment) {
  InMemoryConfig config;
  Supla::Suplet::Manager manager(&config);
  Supla::Suplet::Registry registry;
  auto definition = makeThermometerGroupDefinition();
  ASSERT_TRUE(registry.add(&definition, 4));
  Supla::Suplet::AssignmentApplier applier(&manager, &registry);
  Supla::Suplet::ChannelAllocator occupied;
  ASSERT_TRUE(occupied.markOccupied(0));

  ASSERT_EQ(applier.applyJson(instanceJsonAvg, 900, 4, occupied),
            Supla::Suplet::AssignmentResult::Applied);
  ASSERT_NE(manager.getInstanceTable()->findByInstanceId(77), nullptr);

  EXPECT_EQ(applier.remove(77), Supla::Suplet::AssignmentResult::Removed);
  EXPECT_EQ(manager.getInstanceTable()->findByInstanceId(77), nullptr);
}

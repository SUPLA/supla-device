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
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <map>
#include <string>
#include <vector>
#include <supla/storage/config.h>
#include <supla/storage/key_value.h>
#include <supla/suplet/storage.h>

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

  bool setString(const char *key, const char *value) override {
    if (key == nullptr || value == nullptr) {
      return false;
    }
    strings[key] = value;
    return true;
  }

  bool getString(const char *key, char *value, size_t maxSize) override {
    if (key == nullptr || value == nullptr || strings.count(key) == 0) {
      return false;
    }
    snprintf(value, maxSize, "%s", strings[key].c_str());
    return true;
  }

  int getStringSize(const char *key) override {
    if (key == nullptr || strings.count(key) == 0) {
      return -1;
    }
    return strings[key].size() + 1;
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
    erased = (strings.erase(key) > 0) || erased;
    return erased;
  }

  void commit() override {
    commitCount++;
  }

  std::map<std::string, std::vector<char>> blobs;
  std::map<std::string, uint8_t> uint8Values;
  std::map<std::string, std::string> strings;
  int commitCount = 0;
};

class KeyValueConfig : public Supla::KeyValue {
 public:
  bool init() override {
    return true;
  }

  void removeAll() override {
    removeAllMemory();
  }

  void commit() override {
    commitCount++;
  }

  int commitCount = 0;
};

Supla::Suplet::InstanceRecord makeRecord(uint8_t instanceId,
                                         int firstChannel) {
  Supla::Suplet::InstanceRecord record = {};
  record.instanceId = instanceId;
  record.definitionId = 0x1000 + instanceId;
  record.definitionVersion = 0x0102;
  record.subDeviceId = instanceId;
  const uint8_t config[] = {9, 8, 7};
  record.setConfig(config, sizeof(config));
  record.channelMap.add(0xA0 + instanceId, firstChannel);
  record.channelMap.add(0xB0 + instanceId, firstChannel + 1);
  return record;
}

bool hasBlob(const InMemoryConfig &config, const char *key) {
  return config.blobs.count(key) > 0;
}

}  // namespace

TEST(SupletInstanceTableTests, RejectsDuplicateInstanceAndSubdevice) {
  Supla::Suplet::InstanceTable table;
  auto first = makeRecord(1, 10);
  auto duplicateInstance = makeRecord(1, 12);
  auto duplicateSubdevice = makeRecord(2, 14);
  duplicateSubdevice.subDeviceId = 1;

  EXPECT_TRUE(table.add(first));
  EXPECT_FALSE(table.add(duplicateInstance));
  EXPECT_FALSE(table.add(duplicateSubdevice));
  EXPECT_EQ(table.getCount(), 1);
}

TEST(SupletStorageTests, EmptyKeyValueStorageDoesNotCleanupMissingSlots) {
  KeyValueConfig config;
  Supla::Suplet::Storage storage(&config);
  Supla::Suplet::InstanceTable loaded;

  EXPECT_FALSE(storage.loadIndex(&loaded));
  EXPECT_EQ(loaded.getCount(), 0);
  EXPECT_EQ(config.commitCount, 0);
}

TEST(SupletStorageTests, SavesAndLoadsInstanceTable) {
  InMemoryConfig config;
  Supla::Suplet::InstanceTable table;
  ASSERT_TRUE(table.add(makeRecord(1, 10)));
  ASSERT_TRUE(table.add(makeRecord(5, 20)));

  Supla::Suplet::Storage storage(&config);
  ASSERT_TRUE(storage.save(table));
  EXPECT_TRUE(hasBlob(config, "1_splt_1"));
  EXPECT_TRUE(hasBlob(config, "1_splt_1_ch"));
  EXPECT_TRUE(hasBlob(config, "1_splt_1_cfg"));
  EXPECT_TRUE(hasBlob(config, "5_splt_1"));
  EXPECT_FALSE(hasBlob(config, "1_splt_2"));
  EXPECT_EQ(config.uint8Values["1_splt_act"], 1);
  EXPECT_EQ(config.uint8Values["5_splt_act"], 1);

  Supla::Suplet::InstanceTable loaded;
  ASSERT_TRUE(storage.load(&loaded));

  ASSERT_EQ(loaded.getCount(), 2);
  auto first = loaded.findByInstanceId(1);
  ASSERT_NE(first, nullptr);
  EXPECT_EQ(first->definitionId, 0x1001u);
  EXPECT_EQ(first->subDeviceId, 1);
  EXPECT_EQ(first->configSize, 3);
  EXPECT_EQ(first->config[0], 9);
  EXPECT_EQ(first->channelMap.getChannelNumber(0xA1), 10);
  EXPECT_EQ(first->channelMap.getChannelNumber(0xB1), 11);
  EXPECT_NE(loaded.findByInstanceId(5), nullptr);
}

TEST(SupletStorageTests, LoadsIndexWithoutConfigPayload) {
  InMemoryConfig config;
  Supla::Suplet::InstanceTable table;
  ASSERT_TRUE(table.add(makeRecord(1, 10)));

  Supla::Suplet::Storage storage(&config);
  ASSERT_TRUE(storage.save(table));

  Supla::Suplet::InstanceTable index;
  ASSERT_TRUE(storage.loadIndex(&index));

  ASSERT_EQ(index.getCount(), 1);
  auto record = index.findByInstanceId(1);
  ASSERT_NE(record, nullptr);
  EXPECT_EQ(record->configSize, 3);
  EXPECT_EQ(record->config, nullptr);
  EXPECT_EQ(record->channelMap.getChannelNumber(0xA1), 10);

  Supla::Suplet::InstanceRecord fullRecord;
  ASSERT_TRUE(storage.loadInstance(1, &fullRecord));
  ASSERT_NE(fullRecord.config, nullptr);
  EXPECT_EQ(fullRecord.configSize, 3);
  EXPECT_EQ(fullRecord.config[0], 9);
}

TEST(SupletStorageTests, UpsertAlternatesVariantsAndDeletesOldVariant) {
  InMemoryConfig config;
  Supla::Suplet::Storage storage(&config);
  Supla::Suplet::InstanceTable table;

  ASSERT_TRUE(table.add(makeRecord(1, 10)));
  ASSERT_TRUE(storage.save(table));
  EXPECT_EQ(config.uint8Values["1_splt_act"], 1);
  EXPECT_TRUE(hasBlob(config, "1_splt_1"));
  EXPECT_FALSE(hasBlob(config, "1_splt_2"));

  table.clear();
  auto updated = makeRecord(1, 30);
  const uint8_t updatedConfig[] = {4, 8, 7};
  ASSERT_TRUE(updated.setConfig(updatedConfig, sizeof(updatedConfig)));
  ASSERT_TRUE(table.add(updated));
  ASSERT_TRUE(storage.save(table));
  EXPECT_EQ(config.uint8Values["1_splt_act"], 2);
  EXPECT_FALSE(hasBlob(config, "1_splt_1"));
  EXPECT_TRUE(hasBlob(config, "1_splt_2"));

  Supla::Suplet::InstanceTable loaded;
  ASSERT_TRUE(storage.load(&loaded));

  ASSERT_EQ(loaded.getCount(), 1);
  auto record = loaded.findByInstanceId(1);
  ASSERT_NE(record, nullptr);
  EXPECT_EQ(record->config[0], 4);
  EXPECT_EQ(record->channelMap.getChannelNumber(0xA1), 30);
}

TEST(SupletStorageTests, SavingIndexRecordWithoutConfigDoesNotEraseTargetSlot) {
  InMemoryConfig config;
  Supla::Suplet::Storage storage(&config);
  Supla::Suplet::InstanceTable table;

  ASSERT_TRUE(table.add(makeRecord(1, 10)));
  ASSERT_TRUE(storage.save(table));

  Supla::Suplet::InstanceTable index;
  ASSERT_TRUE(storage.loadIndex(&index));
  auto indexed = index.findByInstanceId(1);
  ASSERT_NE(indexed, nullptr);
  ASSERT_EQ(indexed->config, nullptr);
  ASSERT_EQ(indexed->configSize, 3);
  indexed->channelMap.clear();
  ASSERT_TRUE(indexed->channelMap.add(0xA1, 30));
  ASSERT_TRUE(indexed->channelMap.add(0xB1, 31));

  ASSERT_TRUE(storage.save(index));
  EXPECT_EQ(config.uint8Values["1_splt_act"], 2);
  EXPECT_FALSE(hasBlob(config, "1_splt_1"));
  EXPECT_TRUE(hasBlob(config, "1_splt_2"));
  EXPECT_TRUE(hasBlob(config, "1_splt_2_ch"));
  EXPECT_TRUE(hasBlob(config, "1_splt_2_cfg"));

  Supla::Suplet::InstanceTable loaded;
  ASSERT_TRUE(storage.load(&loaded));

  ASSERT_EQ(loaded.getCount(), 1);
  auto record = loaded.findByInstanceId(1);
  ASSERT_NE(record, nullptr);
  ASSERT_NE(record->config, nullptr);
  EXPECT_EQ(record->configSize, 3);
  EXPECT_EQ(record->config[0], 9);
  EXPECT_EQ(record->channelMap.getChannelNumber(0xA1), 30);
}

TEST(SupletStorageTests, IgnoresInactiveVariantWhenActWasNotUpdated) {
  InMemoryConfig config;
  Supla::Suplet::Storage storage(&config);
  Supla::Suplet::InstanceTable table;

  ASSERT_TRUE(table.add(makeRecord(1, 10)));
  ASSERT_TRUE(storage.save(table));

  auto activeVariant1 = config.blobs;
  table.clear();
  ASSERT_TRUE(table.add(makeRecord(1, 20)));
  ASSERT_TRUE(storage.save(table));
  config.blobs["1_splt_1"] = activeVariant1["1_splt_1"];
  config.blobs["1_splt_1_ch"] = activeVariant1["1_splt_1_ch"];
  config.blobs["1_splt_1_cfg"] = activeVariant1["1_splt_1_cfg"];
  config.uint8Values["1_splt_act"] = 1;

  Supla::Suplet::InstanceTable loaded;
  ASSERT_TRUE(storage.load(&loaded));

  EXPECT_EQ(loaded.getCount(), 1);
  auto record = loaded.findByInstanceId(1);
  ASSERT_NE(record, nullptr);
  EXPECT_EQ(record->channelMap.getChannelNumber(0xA1), 10);
}

TEST(SupletStorageTests, FallsBackWhenActiveVariantIsCorrupted) {
  InMemoryConfig config;
  Supla::Suplet::Storage storage(&config);
  Supla::Suplet::InstanceTable table;

  ASSERT_TRUE(table.add(makeRecord(1, 10)));
  ASSERT_TRUE(storage.save(table));

  auto fallbackBlobs = config.blobs;
  table.clear();
  ASSERT_TRUE(table.add(makeRecord(1, 20)));
  ASSERT_TRUE(storage.save(table));
  ASSERT_EQ(config.uint8Values["1_splt_act"], 2);

  config.blobs["1_splt_1"] = fallbackBlobs["1_splt_1"];
  config.blobs["1_splt_1_ch"] = fallbackBlobs["1_splt_1_ch"];
  config.blobs["1_splt_1_cfg"] = fallbackBlobs["1_splt_1_cfg"];
  config.blobs["1_splt_2"].clear();

  Supla::Suplet::InstanceTable loaded;
  ASSERT_TRUE(storage.load(&loaded));

  EXPECT_EQ(loaded.getCount(), 1);
  auto record = loaded.findByInstanceId(1);
  ASSERT_NE(record, nullptr);
  EXPECT_EQ(record->channelMap.getChannelNumber(0xA1), 10);
  EXPECT_EQ(config.uint8Values["1_splt_act"], 1);
  EXPECT_FALSE(hasBlob(config, "1_splt_2"));
}

TEST(SupletStorageTests, SaveRemovesMissingInstances) {
  InMemoryConfig config;
  Supla::Suplet::Storage storage(&config);
  Supla::Suplet::InstanceTable table;
  ASSERT_TRUE(table.add(makeRecord(1, 10)));
  ASSERT_TRUE(table.add(makeRecord(2, 20)));
  ASSERT_TRUE(storage.save(table));

  table.clear();
  ASSERT_TRUE(table.add(makeRecord(2, 20)));
  ASSERT_TRUE(storage.save(table));

  EXPECT_EQ(config.uint8Values.count("1_splt_act"), 0u);
  EXPECT_FALSE(hasBlob(config, "1_splt_1"));
  EXPECT_NE(config.uint8Values.count("2_splt_act"), 0u);
}

TEST(SupletStorageTests, EraseRemovesInstanceSlots) {
  InMemoryConfig config;
  Supla::Suplet::Storage storage(&config);
  Supla::Suplet::InstanceTable table;
  ASSERT_TRUE(table.add(makeRecord(1, 10)));
  ASSERT_TRUE(storage.save(table));

  EXPECT_TRUE(storage.erase());
  EXPECT_EQ(config.blobs.size(), 0u);
  EXPECT_EQ(config.uint8Values.size(), 0u);
}

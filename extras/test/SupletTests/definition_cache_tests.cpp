// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>
#include <stdint.h>
#include <string.h>
#include <supla/storage/config.h>
#include <supla/storage/key_value.h>
#include <supla/suplet/definition_cache.h>

#include <algorithm>
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
    operations.push_back(std::string("blob:") + key);
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
    operations.push_back(std::string("u8:") + key + "=" +
                         std::to_string(value));
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
    operations.push_back(std::string("erase:") + key);
    bool erased = blobs.erase(key) > 0;
    erased = (uint8Values.erase(key) > 0) || erased;
    return erased;
  }
  void commit() override {
    operations.push_back("commit");
    commitCount++;
  }

  std::map<std::string, std::vector<char>> blobs;
  std::map<std::string, uint8_t> uint8Values;
  std::vector<std::string> operations;
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
};

constexpr size_t kDefinitionCacheHeaderSize = 22;

}  // namespace

TEST(SupletDefinitionCacheTests, SavesLoadsAndReportsInfo) {
  InMemoryConfig config;
  Supla::Suplet::DefinitionCache cache(&config);
  const char json[] = "{\"definitionId\":10,\"definitionVersion\":1}";

  ASSERT_TRUE(cache.save(10, 1, json));
  EXPECT_GE(config.commitCount, 1);
  ASSERT_GT(config.uint8Values.count("spld0_act"), 0);
  EXPECT_EQ(config.uint8Values["spld0_act"], 1);
  ASSERT_GT(config.blobs.count("spld0_1"), 0);
  ASSERT_GT(config.blobs.count("spld0_1c0"), 0);
  EXPECT_EQ(config.blobs["spld0_1"].size(), kDefinitionCacheHeaderSize);
  EXPECT_EQ(config.blobs["spld0_1c0"].size(), strlen(json));

  char output[128] = {};
  Supla::Suplet::CachedDefinitionInfo info = {};
  ASSERT_TRUE(cache.load(10, 1, output, sizeof(output), &info));
  EXPECT_STREQ(output, json);
  EXPECT_EQ(info.definitionId, 10u);
  EXPECT_EQ(info.definitionVersion, 1u);
  EXPECT_EQ(info.jsonSize, strlen(json));
  EXPECT_NE(info.crc32, 0u);

  Supla::Suplet::CachedDefinitionInfo slotInfo = {};
  ASSERT_TRUE(cache.getInfo(0, &slotInfo));
  EXPECT_EQ(slotInfo.definitionId, 10u);
}

TEST(SupletDefinitionCacheTests, MissingKeyValueSlotIsNotAnEmptyBlob) {
  KeyValueConfig config;
  Supla::Suplet::DefinitionCache cache(&config);

  EXPECT_FALSE(cache.contains(0x2010, 1));
}

TEST(SupletDefinitionCacheTests, StoresVariableSizeBlobs) {
  InMemoryConfig config;
  Supla::Suplet::DefinitionCache cache(&config);
  const char shortJson[] = "{\"definitionId\":20,\"definitionVersion\":1}";
  const char longJson[] =
      "{\"definitionId\":21,\"definitionVersion\":1,\"channels\":[{"
      "\"channelId\":1,\"key\":\"relay\",\"kind\":\"virtualRelay\","
      "\"function\":\"powerSwitch\"}]}";
  ASSERT_TRUE(cache.save(20, 1, shortJson));
  ASSERT_TRUE(cache.save(21, 1, longJson));

  ASSERT_GT(config.blobs.count("spld0_1"), 0);
  ASSERT_GT(config.blobs.count("spld1_1"), 0);
  ASSERT_GT(config.blobs.count("spld0_1c0"), 0);
  ASSERT_GT(config.blobs.count("spld1_1c0"), 0);
  EXPECT_EQ(config.blobs["spld0_1"].size(), kDefinitionCacheHeaderSize);
  EXPECT_EQ(config.blobs["spld1_1"].size(), kDefinitionCacheHeaderSize);
  EXPECT_EQ(config.blobs["spld0_1c0"].size(), strlen(shortJson));
  EXPECT_EQ(config.blobs["spld1_1c0"].size(), strlen(longJson));
  EXPECT_LT(config.blobs["spld0_1"].size(), 256u);
}

TEST(SupletDefinitionCacheTests, StoresPayloadInTwoKilobyteChunks) {
  InMemoryConfig config;
  Supla::Suplet::DefinitionCache cache(&config);
  std::string json = "{\"definitionId\":22,\"definitionVersion\":1,\"data\":\"";
  json += std::string(SUPLA_SUPLET_DEFINITION_CACHE_CHUNK_SIZE + 17, 'x');
  json += "\"}";
  ASSERT_TRUE(cache.save(22, 1, json.c_str()));
  ASSERT_GT(config.blobs.count("spld0_1"), 0);
  ASSERT_GT(config.blobs.count("spld0_1c0"), 0);
  ASSERT_GT(config.blobs.count("spld0_1c1"), 0);
  EXPECT_EQ(config.blobs["spld0_1"].size(), kDefinitionCacheHeaderSize);
  EXPECT_EQ(config.blobs["spld0_1c0"].size(),
            static_cast<size_t>(SUPLA_SUPLET_DEFINITION_CACHE_CHUNK_SIZE));
  EXPECT_EQ(config.blobs["spld0_1c1"].size(),
            json.size() - SUPLA_SUPLET_DEFINITION_CACHE_CHUNK_SIZE);

  std::string output(json.size() + 1, '\0');
  ASSERT_TRUE(cache.load(22, 1, output.data(), output.size()));
  EXPECT_STREQ(output.c_str(), json.c_str());
}

TEST(SupletDefinitionCacheTests, UsesSlotsAboveThree) {
  InMemoryConfig config;
  Supla::Suplet::DefinitionCache cache(&config);

  for (uint8_t i = 0; i < 5; i++) {
    std::string json = "{\"definitionId\":";
    json += std::to_string(30 + i);
    json += ",\"definitionVersion\":1}";
    ASSERT_TRUE(cache.save(30 + i, 1, json.c_str()));
  }

  ASSERT_GT(config.blobs.count("spld4_1"), 0);
  ASSERT_GT(config.blobs.count("spld4_1c0"), 0);
  char output[128] = {};
  ASSERT_TRUE(cache.load(34, 1, output, sizeof(output)));
  EXPECT_STREQ(output, "{\"definitionId\":34,\"definitionVersion\":1}");
}

TEST(SupletDefinitionCacheTests, RejectsSaveWhenCacheIsFull) {
  InMemoryConfig config;
  Supla::Suplet::DefinitionCache cache(&config);

  for (uint8_t i = 0; i < SUPLA_SUPLET_MAX_CACHED_DEFINITIONS; i++) {
    std::string json = "{\"definitionId\":";
    json += std::to_string(100 + i);
    json += ",\"definitionVersion\":1}";
    ASSERT_TRUE(cache.save(100 + i, 1, json.c_str()));
  }

  const char overflowJson[] = "{\"definitionId\":999,\"definitionVersion\":1}";
  EXPECT_FALSE(cache.save(999, 1, overflowJson));
}

TEST(SupletDefinitionCacheTests, RejectsSmallOutputBuffer) {
  InMemoryConfig config;
  Supla::Suplet::DefinitionCache cache(&config);
  const char json[] = "{\"definitionId\":11,\"definitionVersion\":1}";
  ASSERT_TRUE(cache.save(11, 1, json));
  char small[4] = {};
  EXPECT_FALSE(cache.load(11, 1, small, sizeof(small)));
}

TEST(SupletDefinitionCacheTests, DetectsCorruptedStoredJson) {
  InMemoryConfig config;
  Supla::Suplet::DefinitionCache cache(&config);
  const char json[] = "{\"definitionId\":12,\"definitionVersion\":1}";
  ASSERT_TRUE(cache.save(12, 1, json));

  ASSERT_GT(config.blobs.count("spld0_1c0"), 0);
  for (auto &byte : config.blobs["spld0_1c0"]) {
    if (byte == '{') {
      byte = '[';
      break;
    }
  }

  char output[128] = {};
  EXPECT_FALSE(cache.load(12, 1, output, sizeof(output)));
}

TEST(SupletDefinitionCacheTests, RejectsInvalidStoredHeaderAndTerminator) {
  InMemoryConfig config;
  Supla::Suplet::DefinitionCache cache(&config);
  const char json[] = "{\"definitionId\":14,\"definitionVersion\":1}";
  ASSERT_TRUE(cache.save(14, 1, json));

  ASSERT_GT(config.blobs.count("spld0_1"), 0);
  auto original = config.blobs["spld0_1"];
  char output[128] = {};

  config.blobs["spld0_1"][4] = 1;  // version
  EXPECT_FALSE(cache.load(14, 1, output, sizeof(output)));

  config.blobs["spld0_1"] = original;
  config.blobs["spld0_1"][6] = 0x7F;  // jsonSize low byte
  EXPECT_FALSE(cache.load(14, 1, output, sizeof(output)));

  config.blobs["spld0_1"] = original;
  config.blobs["spld0_1"][14] = 0;  // chunkCount low byte
  EXPECT_FALSE(cache.load(14, 1, output, sizeof(output)));
}

TEST(SupletDefinitionCacheTests, UpdatesExistingSlotAndErasesIt) {
  InMemoryConfig config;
  Supla::Suplet::DefinitionCache cache(&config);
  const char jsonA[] = "{\"definitionId\":13,\"definitionVersion\":1}";
  const char jsonB[] = "{\"definitionId\":13,\"definitionVersion\":1,\"x\":2}";
  ASSERT_TRUE(cache.save(13, 1, jsonA));
  ASSERT_GT(config.blobs.count("spld0_1"), 0);
  ASSERT_GT(config.blobs.count("spld0_1c0"), 0);
  ASSERT_TRUE(cache.save(13, 1, jsonB));

  char output[128] = {};
  EXPECT_TRUE(cache.contains(13, 1));
  ASSERT_TRUE(cache.load(13, 1, output, sizeof(output)));
  EXPECT_STREQ(output, jsonB);
  EXPECT_EQ(config.uint8Values["spld0_act"], 2);
  EXPECT_EQ(config.blobs.count("spld0_1"), 0);
  EXPECT_EQ(config.blobs.count("spld0_1c0"), 0);
  ASSERT_GT(config.blobs.count("spld0_2"), 0);
  ASSERT_GT(config.blobs.count("spld0_2c0"), 0);
  EXPECT_EQ(config.blobs.size(), 2u);

  EXPECT_TRUE(cache.erase(13, 1));
  EXPECT_FALSE(cache.contains(13, 1));
  EXPECT_FALSE(cache.load(13, 1, output, sizeof(output)));
  EXPECT_TRUE(config.blobs.empty());
  EXPECT_TRUE(config.uint8Values.empty());
}

TEST(SupletDefinitionCacheTests, CommitsActivationBeforeErasingOldVariant) {
  InMemoryConfig config;
  Supla::Suplet::DefinitionCache cache(&config);
  const char jsonA[] = "{\"definitionId\":17,\"definitionVersion\":1}";
  const char jsonB[] = "{\"definitionId\":17,\"definitionVersion\":1,\"x\":2}";
  ASSERT_TRUE(cache.save(17, 1, jsonA));
  config.operations.clear();

  ASSERT_TRUE(cache.save(17, 1, jsonB));

  auto setActive = std::find(config.operations.begin(),
                             config.operations.end(),
                             "u8:spld0_act=2");
  ASSERT_NE(setActive, config.operations.end());
  auto activationCommit =
    std::find(setActive, config.operations.end(), "commit");
  ASSERT_NE(activationCommit, config.operations.end());
  auto eraseOldHeader = std::find(config.operations.begin(),
                                  config.operations.end(),
                                  "erase:spld0_1");
  ASSERT_NE(eraseOldHeader, config.operations.end());
  EXPECT_LT(activationCommit, eraseOldHeader);
}

TEST(SupletDefinitionCacheTests, GetInfoDoesNotRepairOrCleanupVariants) {
  InMemoryConfig config;
  Supla::Suplet::DefinitionCache cache(&config);
  const char json[] = "{\"definitionId\":18,\"definitionVersion\":1}";
  ASSERT_TRUE(cache.save(18, 1, json));
  config.blobs["spld0_2"] = config.blobs["spld0_1"];
  config.blobs["spld0_2c0"] = config.blobs["spld0_1c0"];
  config.operations.clear();

  Supla::Suplet::CachedDefinitionInfo info = {};
  ASSERT_TRUE(cache.getInfo(0, &info));
  EXPECT_EQ(config.blobs.count("spld0_2"), 1);
  EXPECT_EQ(config.blobs.count("spld0_2c0"), 1);
  EXPECT_TRUE(config.operations.empty());
}

TEST(SupletDefinitionCacheTests, FallsBackToOtherVariantWhenActiveIsCorrupted) {
  InMemoryConfig config;
  Supla::Suplet::DefinitionCache cache(&config);
  const char jsonA[] = "{\"definitionId\":15,\"definitionVersion\":1}";
  const char jsonB[] = "{\"definitionId\":15,\"definitionVersion\":1,\"x\":2}";
  ASSERT_TRUE(cache.save(15, 1, jsonA));
  auto headerA = config.blobs["spld0_1"];
  auto chunkA = config.blobs["spld0_1c0"];

  ASSERT_TRUE(cache.save(15, 1, jsonB));
  ASSERT_EQ(config.uint8Values["spld0_act"], 2);

  config.blobs["spld0_1"] = headerA;
  config.blobs["spld0_1c0"] = chunkA;
  config.blobs["spld0_2"][4] = 1;  // corrupt active header version

  char output[128] = {};
  ASSERT_TRUE(cache.load(15, 1, output, sizeof(output)));
  EXPECT_STREQ(output, jsonA);
  EXPECT_EQ(config.uint8Values["spld0_act"], 1);
  EXPECT_EQ(config.blobs.count("spld0_2"), 0);
  EXPECT_EQ(config.blobs.count("spld0_2c0"), 0);
}

TEST(SupletDefinitionCacheTests, OrphanedSlotWithoutActiveMarkerIsCleaned) {
  InMemoryConfig config;
  Supla::Suplet::DefinitionCache cache(&config);
  const char json[] = "{\"definitionId\":16,\"definitionVersion\":1}";
  ASSERT_TRUE(cache.save(16, 1, json));
  ASSERT_FALSE(config.blobs.empty());
  ASSERT_FALSE(config.uint8Values.empty());

  config.uint8Values.erase("spld0_act");
  Supla::Suplet::CachedDefinitionInfo info = {};
  EXPECT_FALSE(cache.getInfoAndRepair(0, &info));
  EXPECT_TRUE(config.blobs.empty());
  EXPECT_TRUE(config.uint8Values.empty());
}

TEST(SupletDefinitionCacheTests,
     LegacySlotWithoutActiveMarkerIsNotDeletedByRepairLookup) {
  InMemoryConfig config;
  Supla::Suplet::DefinitionCache cache(&config);

  const char legacyHeader[] = "legacy-header";
  const char legacyChunk[] = "{\"definitionId\":42,\"definitionVersion\":7}";
  ASSERT_TRUE(config.setBlob("spld0", legacyHeader, sizeof(legacyHeader)));
  ASSERT_TRUE(config.setBlob("spld0c0", legacyChunk, sizeof(legacyChunk)));
  ASSERT_EQ(config.blobs.count("spld0"), 1u);
  ASSERT_EQ(config.blobs.count("spld0c0"), 1u);

  Supla::Suplet::CachedDefinitionInfo info = {};
  EXPECT_FALSE(cache.getInfoAndRepair(0, &info));
  EXPECT_EQ(config.blobs.count("spld0"), 1u);
  EXPECT_EQ(config.blobs.count("spld0c0"), 1u);
  EXPECT_EQ(config.uint8Values.count("spld0_act"), 0u);
}

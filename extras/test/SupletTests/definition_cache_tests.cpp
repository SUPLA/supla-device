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
#include <supla/storage/config.h>
#include <supla/storage/key_value.h>
#include <supla/suplet/definition_cache.h>

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

class KeyValueConfig : public Supla::KeyValue {
 public:
  bool init() override {
    return true;
  }

  void removeAll() override {
    removeAllMemory();
  }
};

class FakeSha256Provider : public Supla::Suplet::Sha256Provider {
 public:
  bool calculate(const uint8_t *data,
                 size_t dataSize,
                 uint8_t *output,
                 size_t outputSize) override {
    if (data == nullptr || output == nullptr || outputSize < 32) {
      return false;
    }
    uint8_t sum = 0;
    uint8_t x = 0x5A;
    for (size_t i = 0; i < dataSize; i++) {
      sum = static_cast<uint8_t>(sum + data[i]);
      x = static_cast<uint8_t>((x << 1) ^ data[i] ^ (x >> 7));
    }
    for (uint8_t i = 0; i < 32; i++) {
      output[i] = static_cast<uint8_t>(sum + x + i + dataSize);
    }
    return true;
  }
};

void makeSha(FakeSha256Provider *provider, const char *json, uint8_t *sha) {
  ASSERT_TRUE(provider->calculate(
      reinterpret_cast<const uint8_t *>(json), strlen(json), sha, 32));
}

constexpr size_t kDefinitionCacheHeaderSize = 50;

}  // namespace

TEST(SupletDefinitionCacheTests, SavesLoadsAndReportsInfo) {
  InMemoryConfig config;
  FakeSha256Provider shaProvider;
  Supla::Suplet::DefinitionCache cache(&config, &shaProvider);
  const char json[] = "{\"definitionId\":10,\"definitionVersion\":1}";
  uint8_t sha[32] = {};
  makeSha(&shaProvider, json, sha);

  ASSERT_TRUE(cache.save(10, 1, json, sha));
  EXPECT_GE(config.commitCount, 1);
  ASSERT_GT(config.blobs.count("spld0"), 0);
  ASSERT_GT(config.blobs.count("spld0c0"), 0);
  EXPECT_EQ(config.blobs["spld0"].size(), kDefinitionCacheHeaderSize);
  EXPECT_EQ(config.blobs["spld0c0"].size(), strlen(json));

  char output[128] = {};
  Supla::Suplet::CachedDefinitionInfo info = {};
  ASSERT_TRUE(cache.load(10, 1, output, sizeof(output), &info));
  EXPECT_STREQ(output, json);
  EXPECT_EQ(info.definitionId, 10u);
  EXPECT_EQ(info.definitionVersion, 1u);
  EXPECT_EQ(info.jsonSize, strlen(json));
  EXPECT_EQ(memcmp(info.sha256, sha, sizeof(sha)), 0);

  Supla::Suplet::CachedDefinitionInfo slotInfo = {};
  ASSERT_TRUE(cache.getInfo(0, &slotInfo));
  EXPECT_EQ(slotInfo.definitionId, 10u);
}

TEST(SupletDefinitionCacheTests, MissingKeyValueSlotIsNotAnEmptyBlob) {
  KeyValueConfig config;
  FakeSha256Provider shaProvider;
  Supla::Suplet::DefinitionCache cache(&config, &shaProvider);

  EXPECT_FALSE(cache.contains(0x2010, 1));
}

TEST(SupletDefinitionCacheTests, StoresVariableSizeBlobs) {
  InMemoryConfig config;
  FakeSha256Provider shaProvider;
  Supla::Suplet::DefinitionCache cache(&config, &shaProvider);
  const char shortJson[] = "{\"definitionId\":20,\"definitionVersion\":1}";
  const char longJson[] =
      "{\"definitionId\":21,\"definitionVersion\":1,\"channels\":[{"
      "\"channelId\":1,\"key\":\"relay\",\"kind\":\"virtualRelay\","
      "\"function\":\"powerSwitch\"}]}";
  uint8_t sha[32] = {};

  makeSha(&shaProvider, shortJson, sha);
  ASSERT_TRUE(cache.save(20, 1, shortJson, sha));
  makeSha(&shaProvider, longJson, sha);
  ASSERT_TRUE(cache.save(21, 1, longJson, sha));

  ASSERT_GT(config.blobs.count("spld0"), 0);
  ASSERT_GT(config.blobs.count("spld1"), 0);
  ASSERT_GT(config.blobs.count("spld0c0"), 0);
  ASSERT_GT(config.blobs.count("spld1c0"), 0);
  EXPECT_EQ(config.blobs["spld0"].size(), kDefinitionCacheHeaderSize);
  EXPECT_EQ(config.blobs["spld1"].size(), kDefinitionCacheHeaderSize);
  EXPECT_EQ(config.blobs["spld0c0"].size(), strlen(shortJson));
  EXPECT_EQ(config.blobs["spld1c0"].size(), strlen(longJson));
  EXPECT_LT(config.blobs["spld0"].size(), 256u);
}

TEST(SupletDefinitionCacheTests, StoresPayloadInTwoKilobyteChunks) {
  InMemoryConfig config;
  FakeSha256Provider shaProvider;
  Supla::Suplet::DefinitionCache cache(&config, &shaProvider);
  std::string json = "{\"definitionId\":22,\"definitionVersion\":1,\"data\":\"";
  json += std::string(SUPLA_SUPLET_DEFINITION_CACHE_CHUNK_SIZE + 17, 'x');
  json += "\"}";
  uint8_t sha[32] = {};
  makeSha(&shaProvider, json.c_str(), sha);

  ASSERT_TRUE(cache.save(22, 1, json.c_str(), sha));
  ASSERT_GT(config.blobs.count("spld0"), 0);
  ASSERT_GT(config.blobs.count("spld0c0"), 0);
  ASSERT_GT(config.blobs.count("spld0c1"), 0);
  EXPECT_EQ(config.blobs["spld0"].size(), kDefinitionCacheHeaderSize);
  EXPECT_EQ(config.blobs["spld0c0"].size(),
            static_cast<size_t>(SUPLA_SUPLET_DEFINITION_CACHE_CHUNK_SIZE));
  EXPECT_EQ(config.blobs["spld0c1"].size(),
            json.size() - SUPLA_SUPLET_DEFINITION_CACHE_CHUNK_SIZE);

  std::string output(json.size() + 1, '\0');
  ASSERT_TRUE(cache.load(22, 1, output.data(), output.size()));
  EXPECT_STREQ(output.c_str(), json.c_str());
}

TEST(SupletDefinitionCacheTests, UsesSlotsAboveThree) {
  InMemoryConfig config;
  FakeSha256Provider shaProvider;
  Supla::Suplet::DefinitionCache cache(&config, &shaProvider);
  uint8_t sha[32] = {};

  for (uint8_t i = 0; i < 5; i++) {
    std::string json = "{\"definitionId\":";
    json += std::to_string(30 + i);
    json += ",\"definitionVersion\":1}";
    makeSha(&shaProvider, json.c_str(), sha);
    ASSERT_TRUE(cache.save(30 + i, 1, json.c_str(), sha));
  }

  ASSERT_GT(config.blobs.count("spld4"), 0);
  ASSERT_GT(config.blobs.count("spld4c0"), 0);
  char output[128] = {};
  ASSERT_TRUE(cache.load(34, 1, output, sizeof(output)));
  EXPECT_STREQ(output, "{\"definitionId\":34,\"definitionVersion\":1}");
}

TEST(SupletDefinitionCacheTests, RejectsSaveWhenCacheIsFull) {
  InMemoryConfig config;
  FakeSha256Provider shaProvider;
  Supla::Suplet::DefinitionCache cache(&config, &shaProvider);
  uint8_t sha[32] = {};

  for (uint8_t i = 0; i < SUPLA_SUPLET_MAX_CACHED_DEFINITIONS; i++) {
    std::string json = "{\"definitionId\":";
    json += std::to_string(100 + i);
    json += ",\"definitionVersion\":1}";
    makeSha(&shaProvider, json.c_str(), sha);
    ASSERT_TRUE(cache.save(100 + i, 1, json.c_str(), sha));
  }

  const char overflowJson[] = "{\"definitionId\":999,\"definitionVersion\":1}";
  makeSha(&shaProvider, overflowJson, sha);
  EXPECT_FALSE(cache.save(999, 1, overflowJson, sha));
}

TEST(SupletDefinitionCacheTests, RejectsWrongShaAndSmallOutputBuffer) {
  InMemoryConfig config;
  FakeSha256Provider shaProvider;
  Supla::Suplet::DefinitionCache cache(&config, &shaProvider);
  const char json[] = "{\"definitionId\":11,\"definitionVersion\":1}";
  uint8_t sha[32] = {};
  makeSha(&shaProvider, json, sha);
  sha[0] ^= 0xFF;

  EXPECT_FALSE(cache.save(11, 1, json, sha));
  EXPECT_TRUE(config.blobs.empty());

  sha[0] ^= 0xFF;
  ASSERT_TRUE(cache.save(11, 1, json, sha));
  char small[4] = {};
  EXPECT_FALSE(cache.load(11, 1, small, sizeof(small)));
}

TEST(SupletDefinitionCacheTests, DetectsCorruptedStoredJson) {
  InMemoryConfig config;
  FakeSha256Provider shaProvider;
  Supla::Suplet::DefinitionCache cache(&config, &shaProvider);
  const char json[] = "{\"definitionId\":12,\"definitionVersion\":1}";
  uint8_t sha[32] = {};
  makeSha(&shaProvider, json, sha);
  ASSERT_TRUE(cache.save(12, 1, json, sha));

  ASSERT_GT(config.blobs.count("spld0c0"), 0);
  for (auto &byte : config.blobs["spld0c0"]) {
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
  FakeSha256Provider shaProvider;
  Supla::Suplet::DefinitionCache cache(&config, &shaProvider);
  const char json[] = "{\"definitionId\":14,\"definitionVersion\":1}";
  uint8_t sha[32] = {};
  makeSha(&shaProvider, json, sha);
  ASSERT_TRUE(cache.save(14, 1, json, sha));

  ASSERT_GT(config.blobs.count("spld0"), 0);
  auto original = config.blobs["spld0"];
  char output[128] = {};

  config.blobs["spld0"][4] = 1;  // version
  EXPECT_FALSE(cache.load(14, 1, output, sizeof(output)));

  config.blobs["spld0"] = original;
  config.blobs["spld0"][6] = 0x7F;  // jsonSize low byte
  EXPECT_FALSE(cache.load(14, 1, output, sizeof(output)));

  config.blobs["spld0"] = original;
  config.blobs["spld0"][14] = 0;  // chunkCount low byte
  EXPECT_FALSE(cache.load(14, 1, output, sizeof(output)));
}

TEST(SupletDefinitionCacheTests, UpdatesExistingSlotAndErasesIt) {
  InMemoryConfig config;
  FakeSha256Provider shaProvider;
  Supla::Suplet::DefinitionCache cache(&config, &shaProvider);
  const char jsonA[] = "{\"definitionId\":13,\"definitionVersion\":1}";
  const char jsonB[] = "{\"definitionId\":13,\"definitionVersion\":1,\"x\":2}";
  uint8_t sha[32] = {};

  makeSha(&shaProvider, jsonA, sha);
  ASSERT_TRUE(cache.save(13, 1, jsonA, sha));
  makeSha(&shaProvider, jsonB, sha);
  ASSERT_TRUE(cache.save(13, 1, jsonB, sha));

  char output[128] = {};
  EXPECT_TRUE(cache.contains(13, 1));
  ASSERT_TRUE(cache.load(13, 1, output, sizeof(output)));
  EXPECT_STREQ(output, jsonB);
  EXPECT_EQ(config.blobs.size(), 2u);

  EXPECT_TRUE(cache.erase(13, 1));
  EXPECT_FALSE(cache.contains(13, 1));
  EXPECT_FALSE(cache.load(13, 1, output, sizeof(output)));
  EXPECT_TRUE(config.blobs.empty());
}

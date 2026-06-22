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
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <supla/channels/channel.h>
#include <supla/element.h>
#include <supla/storage/config.h>
#include <supla/suplet/server_config.h>

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

void shaToHex(const uint8_t *sha, char *hex, size_t hexSize) {
  ASSERT_NE(sha, nullptr);
  ASSERT_NE(hex, nullptr);
  ASSERT_GE(hexSize, 65u);
  static const char chars[] = "0123456789abcdef";
  for (uint8_t i = 0; i < 32; i++) {
    hex[i * 2] = chars[(sha[i] >> 4) & 0x0F];
    hex[i * 2 + 1] = chars[sha[i] & 0x0F];
  }
  hex[64] = '\0';
}

void escapeJsonString(const char *input, char *output, size_t outputSize) {
  ASSERT_NE(input, nullptr);
  ASSERT_NE(output, nullptr);
  ASSERT_GT(outputSize, 0u);
  size_t index = 0;
  for (const char *ptr = input; *ptr != '\0'; ptr++) {
    if (*ptr == '"' || *ptr == '\\') {
      ASSERT_LT(index + 2, outputSize);
      output[index++] = '\\';
      output[index++] = *ptr;
    } else {
      ASSERT_LT(index + 1, outputSize);
      output[index++] = *ptr;
    }
  }
  ASSERT_LT(index, outputSize);
  output[index] = '\0';
}

Supla::Suplet::ChannelDefinition relayChannels[] = {
    {1,
     Supla::Suplet::ChannelKind::VirtualRelay,
     SUPLA_CHANNELFNC_POWERSWITCH,
     nullptr},
};

Supla::Suplet::ChannelDefinition binaryChannels[] = {
    {2,
     Supla::Suplet::ChannelKind::VirtualBinarySensor,
     SUPLA_CHANNELFNC_OPENINGSENSOR_DOOR,
     nullptr},
};

Supla::Suplet::ParameterDefinition relayParameters[] = {
    {"relay.count",
     Supla::Suplet::ParameterType::UInt8,
     Supla::Suplet::ParameterLifecycle::CreateOnly,
     1,
     16,
     4,
     nullptr,
     nullptr,
     1,
     1,
     1},
    {"mode",
     Supla::Suplet::ParameterType::Enum,
     Supla::Suplet::ParameterLifecycle::Editable,
     INT32_MIN,
     INT32_MAX,
     0,
     "avg",
     "avg,min,max",
     1,
     1,
     0},
    {"host",
     Supla::Suplet::ParameterType::String,
     Supla::Suplet::ParameterLifecycle::Editable,
     INT32_MIN,
     INT32_MAX,
     0,
     nullptr,
     nullptr,
     1,
     0,
     0},
};

Supla::Suplet::Definition makeRelayDefinition() {
  Supla::Suplet::Definition definition = {};
  definition.definitionId = 700;
  definition.definitionVersion = 2;
  definition.category = Supla::Suplet::Category::Virtual;
  definition.kind = Supla::Suplet::Kind::VirtualRelay;
  definition.channels = relayChannels;
  definition.channelCount = 1;
  return definition;
}

Supla::Suplet::Definition makeParameterizedRelayDefinition() {
  auto definition = makeRelayDefinition();
  definition.parameters = relayParameters;
  definition.parameterCount = 3;
  return definition;
}

Supla::Suplet::Definition makeBinaryDefinition() {
  Supla::Suplet::Definition definition = {};
  definition.definitionId = 702;
  definition.definitionVersion = 1;
  definition.category = Supla::Suplet::Category::Virtual;
  definition.kind = Supla::Suplet::Kind::VirtualBinarySensor;
  definition.channels = binaryChannels;
  definition.channelCount = 1;
  return definition;
}

const char relayAssignment[] =
"{"
"\"instanceId\":70,"
"\"definitionId\":700,"
"\"definitionVersion\":2"
"}";

const char downloadedDefinitionJson[] =
"{"
"\"schemaVersion\":1,"
"\"handlerVersion\":1,"
"\"definitionId\":701,"
"\"definitionVersion\":1,"
"\"maxInstances\":3,"
"\"category\":\"virtual\","
"\"kind\":\"virtualRelay\","
"\"channels\":[{"
"\"channelId\":1,"
"\"key\":\"relay\","
"\"kind\":\"virtualRelay\","
"\"function\":\"powerSwitch\""
"}]"
"}";

const char downloadedAssignmentJson[] =
"{"
"\"instanceId\":71,"
"\"definitionId\":701,"
"\"definitionVersion\":1"
"}";

}  // namespace

TEST(SupletServerConfigTests, AppliesBuiltInAssignmentAndRequestsRefresh) {
  InMemoryConfig config;
  Supla::Suplet::Manager manager(&config);
  Supla::Suplet::Registry registry;
  auto definition = makeRelayDefinition();
  ASSERT_TRUE(registry.add(&definition, 4));
  Supla::Suplet::ServerConfigHandler handler(&manager, &registry);

  EXPECT_EQ(handler.applyAssignmentJson(relayAssignment, 700, 2),
            Supla::Suplet::ServerConfigResult::Applied);
  EXPECT_TRUE(handler.isRuntimeRefreshRequired());

  auto record = manager.getInstanceTable()->findByInstanceId(70);
  ASSERT_NE(record, nullptr);
  EXPECT_EQ(record->channelMap.getChannelNumber(1),
            Supla::Suplet::kInvalidChannelNumber);
}

TEST(SupletServerConfigTests, RemovesAssignmentAndRequestsRefresh) {
  InMemoryConfig config;
  Supla::Suplet::Manager manager(&config);
  Supla::Suplet::Registry registry;
  auto definition = makeRelayDefinition();
  ASSERT_TRUE(registry.add(&definition, 4));
  Supla::Suplet::ServerConfigHandler handler(&manager, &registry);
  ASSERT_EQ(handler.applyAssignmentJson(relayAssignment, 700, 2),
            Supla::Suplet::ServerConfigResult::Applied);
  handler.clearRuntimeRefreshRequired();

  EXPECT_EQ(handler.removeAssignment(70),
            Supla::Suplet::ServerConfigResult::Removed);
  EXPECT_TRUE(handler.isRuntimeRefreshRequired());
  EXPECT_EQ(manager.getInstanceTable()->findByInstanceId(70), nullptr);
}

TEST(SupletServerConfigTests, AppliesPrivateInstanceParams) {
  InMemoryConfig config;
  Supla::Suplet::Manager manager(&config);
  Supla::Suplet::Registry registry;
  auto definition = makeParameterizedRelayDefinition();
  ASSERT_TRUE(registry.add(&definition, 4));
  Supla::Suplet::ServerConfigHandler handler(&manager, &registry);

  const char params[] =
      "{\"relay.count\":4,\"mode\":\"avg\",\"host\":\"192.168.1.50\"}";
  EXPECT_EQ(handler.applyInstanceParams(80,
                                        definition.definitionId,
                                        definition.definitionVersion,
                                        params,
                                        strlen(params)),
            Supla::Suplet::ServerConfigResult::Applied);
  EXPECT_TRUE(handler.isRuntimeRefreshRequired());

  auto record = manager.getInstanceTable()->findByInstanceId(80);
  ASSERT_NE(record, nullptr);
  EXPECT_EQ(record->configSize, strlen(params));
  EXPECT_EQ(memcmp(record->config, params, strlen(params)), 0);
}

TEST(SupletServerConfigTests, RejectsInvalidPrivateInstanceParams) {
  InMemoryConfig config;
  Supla::Suplet::Manager manager(&config);
  Supla::Suplet::Registry registry;
  auto definition = makeParameterizedRelayDefinition();
  ASSERT_TRUE(registry.add(&definition, 4));
  Supla::Suplet::ServerConfigHandler handler(&manager, &registry);

  const char badRange[] =
      "{\"relay.count\":40,\"mode\":\"avg\",\"host\":\"192.168.1.50\"}";
  EXPECT_EQ(handler.validateInstanceParams(80,
                                           definition.definitionId,
                                           definition.definitionVersion,
                                           badRange,
                                           strlen(badRange)),
            Supla::Suplet::ServerConfigResult::InvalidConfig);

  const char badEnum[] =
      "{\"relay.count\":4,\"mode\":\"median\",\"host\":\"192.168.1.50\"}";
  EXPECT_EQ(handler.validateInstanceParams(80,
                                           definition.definitionId,
                                           definition.definitionVersion,
                                           badEnum,
                                           strlen(badEnum)),
            Supla::Suplet::ServerConfigResult::InvalidConfig);
}

TEST(SupletServerConfigTests, RejectsCreateOnlyParamChangeOnUpdate) {
  InMemoryConfig config;
  Supla::Suplet::Manager manager(&config);
  Supla::Suplet::Registry registry;
  auto definition = makeParameterizedRelayDefinition();
  ASSERT_TRUE(registry.add(&definition, 4));
  Supla::Suplet::ServerConfigHandler handler(&manager, &registry);

  const char firstParams[] =
      "{\"relay.count\":4,\"mode\":\"avg\",\"host\":\"192.168.1.50\"}";
  ASSERT_EQ(handler.applyInstanceParams(80,
                                        definition.definitionId,
                                        definition.definitionVersion,
                                        firstParams,
                                        strlen(firstParams)),
            Supla::Suplet::ServerConfigResult::Applied);

  const char changedEditable[] =
      "{\"relay.count\":4,\"mode\":\"avg\",\"host\":\"192.168.1.5\"}";
  EXPECT_EQ(handler.applyInstanceParams(80,
                                        definition.definitionId,
                                        definition.definitionVersion,
                                        changedEditable,
                                        strlen(changedEditable)),
            Supla::Suplet::ServerConfigResult::Applied);
  auto record = manager.getInstanceTable()->findByInstanceId(80);
  ASSERT_NE(record, nullptr);
  ASSERT_EQ(record->configSize, strlen(changedEditable));
  ASSERT_EQ(memcmp(record->config, changedEditable, strlen(changedEditable)),
            0);

  const char changedCreateOnly[] =
      "{\"relay.count\":5,\"mode\":\"avg\",\"host\":\"192.168.1.50\"}";
  EXPECT_EQ(handler.validateInstanceParams(80,
                                           definition.definitionId,
                                           definition.definitionVersion,
                                           changedCreateOnly,
                                           strlen(changedCreateOnly)),
            Supla::Suplet::ServerConfigResult::CreateOnlyParamChanged);
}

TEST(SupletServerConfigTests, RejectsDefinitionChangeOnUpdate) {
  InMemoryConfig config;
  Supla::Suplet::Manager manager(&config);
  Supla::Suplet::Registry registry;
  auto relayDefinition = makeParameterizedRelayDefinition();
  auto binaryDefinition = makeBinaryDefinition();
  ASSERT_TRUE(registry.add(&relayDefinition, 4));
  ASSERT_TRUE(registry.add(&binaryDefinition, 4));
  Supla::Suplet::ServerConfigHandler handler(&manager, &registry);

  const char params[] =
      "{\"relay.count\":2,\"mode\":\"max\",\"host\":\"192.168.1.50\"}";
  ASSERT_EQ(handler.applyInstanceParams(80,
                                        relayDefinition.definitionId,
                                        relayDefinition.definitionVersion,
                                        params,
                                        strlen(params)),
            Supla::Suplet::ServerConfigResult::Applied);

  EXPECT_EQ(handler.validateInstanceParams(80,
                                           binaryDefinition.definitionId,
                                           binaryDefinition.definitionVersion,
                                           "{}",
                                           2),
            Supla::Suplet::ServerConfigResult::TopologyChangeNotAllowed);

  EXPECT_EQ(
      handler.validateInstanceParams(80,
                                     relayDefinition.definitionId,
                                     relayDefinition.definitionVersion + 1,
                                     params,
                                     strlen(params)),
      Supla::Suplet::ServerConfigResult::TopologyChangeNotAllowed);
}

TEST(SupletServerConfigTests, AppliesUpsertCommandJson) {
  InMemoryConfig config;
  Supla::Suplet::Manager manager(&config);
  Supla::Suplet::Registry registry;
  auto definition = makeRelayDefinition();
  ASSERT_TRUE(registry.add(&definition, 4));
  Supla::Suplet::ServerConfigHandler handler(&manager, &registry);

  const char commandJson[] =
      "{"
      "\"op\":\"upsert\","
      "\"instanceId\":70,"
      "\"definitionId\":700,"
      "\"definitionVersion\":2"
      "}";

  EXPECT_EQ(handler.applyCommandJson(commandJson),
            Supla::Suplet::ServerConfigResult::Applied);
  EXPECT_TRUE(handler.isRuntimeRefreshRequired());
  ASSERT_NE(manager.getInstanceTable()->findByInstanceId(70), nullptr);
}

TEST(SupletServerConfigTests, RejectsUpsertCommandWhenInstanceLimitExceeded) {
  InMemoryConfig config;
  Supla::Suplet::Manager manager(&config);
  Supla::Suplet::Registry registry;
  auto definition = makeRelayDefinition();
  ASSERT_TRUE(registry.add(&definition, 1));
  Supla::Suplet::ServerConfigHandler handler(&manager, &registry);

  const char commandJson[] =
      "{"
      "\"op\":\"upsert\","
      "\"instanceId\":70,"
      "\"definitionId\":700,"
      "\"definitionVersion\":2"
      "}";
  const char secondCommandJson[] =
      "{"
      "\"op\":\"upsert\","
      "\"instanceId\":71,"
      "\"definitionId\":700,"
      "\"definitionVersion\":2"
      "}";

  ASSERT_EQ(handler.applyCommandJson(commandJson),
            Supla::Suplet::ServerConfigResult::Applied);
  ASSERT_TRUE(handler.isRuntimeRefreshRequired());
  handler.clearRuntimeRefreshRequired();

  EXPECT_EQ(handler.validateCommandJson(secondCommandJson),
            Supla::Suplet::ServerConfigResult::InstanceLimitExceeded);
  EXPECT_FALSE(handler.isRuntimeRefreshRequired());
  EXPECT_EQ(handler.applyCommandJson(secondCommandJson),
            Supla::Suplet::ServerConfigResult::InstanceLimitExceeded);
  EXPECT_FALSE(handler.isRuntimeRefreshRequired());
  EXPECT_EQ(manager.getInstanceTable()->findByInstanceId(71), nullptr);
}

TEST(SupletServerConfigTests, ValidatesUpsertCommandWithoutPersisting) {
  InMemoryConfig config;
  Supla::Suplet::Manager manager(&config);
  Supla::Suplet::Registry registry;
  auto definition = makeRelayDefinition();
  ASSERT_TRUE(registry.add(&definition, 4));
  Supla::Suplet::ServerConfigHandler handler(&manager, &registry);

  const char commandJson[] =
      "{"
      "\"op\":\"upsert\","
      "\"instanceId\":70,"
      "\"definitionId\":700,"
      "\"definitionVersion\":2"
      "}";

  EXPECT_EQ(handler.validateCommandJson(commandJson),
            Supla::Suplet::ServerConfigResult::Applied);
  EXPECT_FALSE(handler.isRuntimeRefreshRequired());
  EXPECT_EQ(manager.getInstanceTable()->findByInstanceId(70), nullptr);
}

TEST(SupletServerConfigTests, ValidateUpsertCommandFailsWhenNoChannelIsFree) {
  Supla::Channel::resetToDefaults();
  InMemoryConfig config;
  Supla::Suplet::Manager manager(&config);
  Supla::Suplet::Registry registry;
  auto definition = makeRelayDefinition();
  ASSERT_TRUE(registry.add(&definition, 4));
  Supla::Suplet::ServerConfigHandler handler(&manager, &registry);
  std::vector<Supla::Channel> channels(SUPLA_CHANNELMAXCOUNT);

  const char commandJson[] =
      "{"
      "\"op\":\"upsert\","
      "\"instanceId\":70,"
      "\"definitionId\":700,"
      "\"definitionVersion\":2"
      "}";

  EXPECT_EQ(handler.validateCommandJson(commandJson),
            Supla::Suplet::ServerConfigResult::ChannelLimitExceeded);
  EXPECT_EQ(manager.getInstanceTable()->findByInstanceId(70), nullptr);
}

TEST(SupletServerConfigTests, AppliesRemoveCommandJson) {
  InMemoryConfig config;
  Supla::Suplet::Manager manager(&config);
  Supla::Suplet::Registry registry;
  auto definition = makeRelayDefinition();
  ASSERT_TRUE(registry.add(&definition, 4));
  Supla::Suplet::ServerConfigHandler handler(&manager, &registry);
  ASSERT_EQ(handler.applyAssignmentJson(relayAssignment, 700, 2),
            Supla::Suplet::ServerConfigResult::Applied);
  handler.clearRuntimeRefreshRequired();

  const char commandJson[] =
      "{"
      "\"operation\":\"remove\","
      "\"instanceId\":70"
      "}";

  EXPECT_EQ(handler.applyCommandJson(commandJson),
            Supla::Suplet::ServerConfigResult::Removed);
  EXPECT_TRUE(handler.isRuntimeRefreshRequired());
  EXPECT_EQ(manager.getInstanceTable()->findByInstanceId(70), nullptr);
}

TEST(SupletServerConfigTests, RejectsInvalidCommandJson) {
  InMemoryConfig config;
  Supla::Suplet::Manager manager(&config);
  Supla::Suplet::Registry registry;
  auto definition = makeRelayDefinition();
  ASSERT_TRUE(registry.add(&definition, 4));
  Supla::Suplet::ServerConfigHandler handler(&manager, &registry);

  EXPECT_EQ(handler.applyCommandJson("{\"op\":\"upsert\",\"instanceId\":70}"),
            Supla::Suplet::ServerConfigResult::InvalidArgument);
  EXPECT_EQ(handler.applyCommandJson("{\"op\":\"unknown\",\"instanceId\":70}"),
            Supla::Suplet::ServerConfigResult::InvalidArgument);
  EXPECT_EQ(handler.applyCommandJson("{}"),
            Supla::Suplet::ServerConfigResult::InvalidArgument);
  EXPECT_EQ(handler.applyCommandJson(
                "{\"op\":\"saveDefinition\",\"definitionId\":701,"
                "\"definitionVersion\":1,\"sha256\":\"xyz\"}"),
            Supla::Suplet::ServerConfigResult::InvalidArgument);
}

TEST(SupletServerConfigTests, SavesDownloadedDefinitionAndAppliesAssignment) {
  InMemoryConfig config;
  FakeSha256Provider shaProvider;
  Supla::Suplet::DefinitionCache cache(&config, &shaProvider);
  Supla::Suplet::DownloadedDefinitionStore downloadedDefinitions;
  Supla::Suplet::Manager manager(&config);
  Supla::Suplet::Registry registry;
  Supla::Suplet::ServerConfigHandler handler(
      &manager, &registry, &cache, &downloadedDefinitions);
  uint8_t sha[32] = {};
  makeSha(&shaProvider, downloadedDefinitionJson, sha);

  EXPECT_EQ(
      handler.saveDownloadedDefinition(701, 1, downloadedDefinitionJson, sha),
      Supla::Suplet::ServerConfigResult::Applied);
  EXPECT_EQ(
      handler.saveDownloadedDefinition(701, 1, downloadedDefinitionJson, sha),
      Supla::Suplet::ServerConfigResult::Applied);
  EXPECT_EQ(downloadedDefinitions.getCount(cache), 1);
  EXPECT_EQ(registry.findDefinition(701, 1), nullptr);
  Supla::Suplet::JsonDefinition loadedDefinition;
  ASSERT_TRUE(downloadedDefinitions.load(cache, 701, 1, &loadedDefinition));
  EXPECT_EQ(loadedDefinition.getDefinition()->maxInstances, 3);
  EXPECT_EQ(
      handler.applyAssignmentJson(downloadedAssignmentJson, 701, 1),
      Supla::Suplet::ServerConfigResult::Applied);

  auto record = manager.getInstanceTable()->findByInstanceId(71);
  ASSERT_NE(record, nullptr);
  EXPECT_EQ(record->definitionId, 701u);
}

TEST(SupletServerConfigTests, ReplacesUnusedConflictingDownloadedDefinition) {
  InMemoryConfig config;
  FakeSha256Provider shaProvider;
  Supla::Suplet::DefinitionCache cache(&config, &shaProvider);
  Supla::Suplet::DownloadedDefinitionStore downloadedDefinitions;
  Supla::Suplet::Manager manager(&config);
  Supla::Suplet::Registry registry;
  Supla::Suplet::ServerConfigHandler handler(
      &manager, &registry, &cache, &downloadedDefinitions);
  uint8_t sha[32] = {};
  makeSha(&shaProvider, downloadedDefinitionJson, sha);
  ASSERT_EQ(
      handler.saveDownloadedDefinition(701, 1, downloadedDefinitionJson, sha),
      Supla::Suplet::ServerConfigResult::Applied);

  const char conflictingJson[] =
      "{"
      "\"schemaVersion\":1,"
      "\"handlerVersion\":1,"
      "\"definitionId\":701,"
      "\"definitionVersion\":1,"
      "\"maxInstances\":3,"
      "\"category\":\"virtual\","
      "\"kind\":\"virtualRelay\","
      "\"channels\":[{"
      "\"channelId\":1,"
      "\"key\":\"relay\","
      "\"kind\":\"virtualRelay\","
      "\"function\":\"powerSwitch\","
      "\"caption\":\"Changed\""
      "}]"
      "}";
  makeSha(&shaProvider, conflictingJson, sha);
  EXPECT_EQ(handler.saveDownloadedDefinition(701, 1, conflictingJson, sha),
            Supla::Suplet::ServerConfigResult::Applied);

  char storedJson[1024] = {};
  ASSERT_TRUE(cache.load(701, 1, storedJson, sizeof(storedJson)));
  EXPECT_STREQ(storedJson, conflictingJson);
  Supla::Suplet::JsonDefinition loadedDefinition;
  ASSERT_TRUE(downloadedDefinitions.load(cache, 701, 1, &loadedDefinition));
  EXPECT_STREQ(loadedDefinition.getDefinition()->channels[0].caption,
               "Changed");
}

TEST(SupletServerConfigTests,
     RejectsChangingDownloadedDefinitionUsedByInstance) {
  InMemoryConfig config;
  FakeSha256Provider shaProvider;
  Supla::Suplet::DefinitionCache cache(&config, &shaProvider);
  Supla::Suplet::DownloadedDefinitionStore downloadedDefinitions;
  Supla::Suplet::Manager manager(&config);
  Supla::Suplet::Registry registry;
  Supla::Suplet::ServerConfigHandler handler(
      &manager, &registry, &cache, &downloadedDefinitions);
  uint8_t sha[32] = {};
  makeSha(&shaProvider, downloadedDefinitionJson, sha);
  ASSERT_EQ(
      handler.saveDownloadedDefinition(701, 1, downloadedDefinitionJson, sha),
      Supla::Suplet::ServerConfigResult::Applied);
  ASSERT_EQ(
      handler.applyAssignmentJson(downloadedAssignmentJson, 701, 1),
      Supla::Suplet::ServerConfigResult::Applied);

  const char conflictingJson[] =
      "{"
      "\"schemaVersion\":1,"
      "\"handlerVersion\":1,"
      "\"definitionId\":701,"
      "\"definitionVersion\":1,"
      "\"maxInstances\":3,"
      "\"category\":\"virtual\","
      "\"kind\":\"virtualRelay\","
      "\"channels\":[{"
      "\"channelId\":1,"
      "\"key\":\"relay\","
      "\"kind\":\"virtualRelay\","
      "\"function\":\"powerSwitch\","
      "\"caption\":\"Changed\""
      "}]"
      "}";
  makeSha(&shaProvider, conflictingJson, sha);
  EXPECT_EQ(handler.saveDownloadedDefinition(701, 1, conflictingJson, sha),
            Supla::Suplet::ServerConfigResult::DefinitionCannotBeChanged);

  char storedJson[1024] = {};
  ASSERT_TRUE(cache.load(701, 1, storedJson, sizeof(storedJson)));
  EXPECT_STREQ(storedJson, downloadedDefinitionJson);
}

TEST(SupletServerConfigTests, LoadsMultipleDownloadedDefinitionVersions) {
  InMemoryConfig config;
  FakeSha256Provider shaProvider;
  Supla::Suplet::DefinitionCache cache(&config, &shaProvider);
  Supla::Suplet::DownloadedDefinitionStore downloadedDefinitions;
  Supla::Suplet::Manager manager(&config);
  Supla::Suplet::Registry registry;
  Supla::Suplet::ServerConfigHandler handler(
      &manager, &registry, &cache, &downloadedDefinitions);

  const char v1[] =
      "{"
      "\"schemaVersion\":1,"
      "\"handlerVersion\":1,"
      "\"definitionId\":701,"
      "\"definitionVersion\":1,"
      "\"maxInstances\":3,"
      "\"category\":\"virtual\","
      "\"kind\":\"virtualRelay\","
      "\"channels\":[{"
      "\"channelId\":1,"
      "\"key\":\"relay\","
      "\"kind\":\"virtualRelay\","
      "\"function\":\"powerSwitch\","
      "\"caption\":\"Param relay v1\""
      "}]"
      "}";
  const char v2[] =
      "{"
      "\"schemaVersion\":1,"
      "\"handlerVersion\":1,"
      "\"definitionId\":701,"
      "\"definitionVersion\":2,"
      "\"maxInstances\":3,"
      "\"category\":\"virtual\","
      "\"kind\":\"virtualRelay\","
      "\"channels\":[{"
      "\"channelId\":1,"
      "\"key\":\"relay\","
      "\"kind\":\"virtualRelay\","
      "\"function\":\"powerSwitch\","
      "\"caption\":\"Param relay v2\""
      "}]"
      "}";

  uint8_t sha[32] = {};
  makeSha(&shaProvider, v1, sha);
  ASSERT_EQ(handler.saveDownloadedDefinition(701, 1, v1, sha),
            Supla::Suplet::ServerConfigResult::Applied);
  makeSha(&shaProvider, v2, sha);
  ASSERT_EQ(handler.saveDownloadedDefinition(701, 2, v2, sha),
            Supla::Suplet::ServerConfigResult::Applied);

  EXPECT_EQ(downloadedDefinitions.getCount(cache), 2);
  Supla::Suplet::JsonDefinition loadedV1;
  Supla::Suplet::JsonDefinition loadedV2;
  ASSERT_TRUE(downloadedDefinitions.load(cache, 701, 1, &loadedV1));
  ASSERT_TRUE(downloadedDefinitions.load(cache, 701, 2, &loadedV2));
  EXPECT_STREQ(loadedV1.getDefinition()->channels[0].caption,
               "Param relay v1");
  EXPECT_STREQ(loadedV2.getDefinition()->channels[0].caption,
               "Param relay v2");
}

TEST(SupletServerConfigTests, RemovesDownloadedDefinitionWhenUnused) {
  InMemoryConfig config;
  FakeSha256Provider shaProvider;
  Supla::Suplet::DefinitionCache cache(&config, &shaProvider);
  Supla::Suplet::DownloadedDefinitionStore downloadedDefinitions;
  Supla::Suplet::Manager manager(&config);
  Supla::Suplet::Registry registry;
  Supla::Suplet::ServerConfigHandler handler(
      &manager, &registry, &cache, &downloadedDefinitions);
  uint8_t sha[32] = {};
  makeSha(&shaProvider, downloadedDefinitionJson, sha);

  ASSERT_EQ(
      handler.saveDownloadedDefinition(701, 1, downloadedDefinitionJson, sha),
      Supla::Suplet::ServerConfigResult::Applied);
  ASSERT_TRUE(cache.contains(701, 1));
  EXPECT_EQ(registry.findDefinition(701, 1), nullptr);

  EXPECT_EQ(handler.removeDownloadedDefinition(701, 1),
            Supla::Suplet::ServerConfigResult::Removed);
  EXPECT_FALSE(cache.contains(701, 1));
  EXPECT_EQ(downloadedDefinitions.getCount(cache), 0);
  EXPECT_EQ(registry.findDefinition(701, 1), nullptr);
  EXPECT_TRUE(handler.isRuntimeRefreshRequired());

  EXPECT_EQ(handler.removeDownloadedDefinition(701, 1),
            Supla::Suplet::ServerConfigResult::DefinitionNotFound);
}

TEST(SupletServerConfigTests, RejectsRemovingDefinitionUsedByInstance) {
  InMemoryConfig config;
  FakeSha256Provider shaProvider;
  Supla::Suplet::DefinitionCache cache(&config, &shaProvider);
  Supla::Suplet::DownloadedDefinitionStore downloadedDefinitions;
  Supla::Suplet::Manager manager(&config);
  Supla::Suplet::Registry registry;
  Supla::Suplet::ServerConfigHandler handler(
      &manager, &registry, &cache, &downloadedDefinitions);
  uint8_t sha[32] = {};
  makeSha(&shaProvider, downloadedDefinitionJson, sha);
  ASSERT_EQ(
      handler.saveDownloadedDefinition(701, 1, downloadedDefinitionJson, sha),
      Supla::Suplet::ServerConfigResult::Applied);
  ASSERT_EQ(
      handler.applyAssignmentJson(downloadedAssignmentJson, 701, 1),
      Supla::Suplet::ServerConfigResult::Applied);

  EXPECT_EQ(handler.removeDownloadedDefinition(701, 1),
            Supla::Suplet::ServerConfigResult::TopologyChangeNotAllowed);
  EXPECT_TRUE(cache.contains(701, 1));
  EXPECT_EQ(registry.findDefinition(701, 1), nullptr);
}

TEST(SupletServerConfigTests, RemoveAssignmentGarbageCollectsUnusedDefinition) {
  InMemoryConfig config;
  FakeSha256Provider shaProvider;
  Supla::Suplet::DefinitionCache cache(&config, &shaProvider);
  Supla::Suplet::DownloadedDefinitionStore downloadedDefinitions;
  Supla::Suplet::Manager manager(&config);
  Supla::Suplet::Registry registry;
  Supla::Suplet::ServerConfigHandler handler(
      &manager, &registry, &cache, &downloadedDefinitions);
  uint8_t sha[32] = {};
  makeSha(&shaProvider, downloadedDefinitionJson, sha);
  ASSERT_EQ(
      handler.saveDownloadedDefinition(701, 1, downloadedDefinitionJson, sha),
      Supla::Suplet::ServerConfigResult::Applied);
  ASSERT_EQ(
      handler.applyAssignmentJson(downloadedAssignmentJson, 701, 1),
      Supla::Suplet::ServerConfigResult::Applied);
  ASSERT_TRUE(cache.contains(701, 1));

  EXPECT_EQ(handler.removeAssignment(71),
            Supla::Suplet::ServerConfigResult::Removed);
  EXPECT_FALSE(cache.contains(701, 1));
  EXPECT_EQ(downloadedDefinitions.getCount(cache), 0);
  EXPECT_EQ(registry.findDefinition(701, 1), nullptr);
}

TEST(SupletServerConfigTests,
     RemoveAssignmentKeepsDefinitionUsedByOtherInstance) {
  InMemoryConfig config;
  FakeSha256Provider shaProvider;
  Supla::Suplet::DefinitionCache cache(&config, &shaProvider);
  Supla::Suplet::DownloadedDefinitionStore downloadedDefinitions;
  Supla::Suplet::Manager manager(&config);
  Supla::Suplet::Registry registry;
  Supla::Suplet::ServerConfigHandler handler(
      &manager, &registry, &cache, &downloadedDefinitions);
  uint8_t sha[32] = {};
  makeSha(&shaProvider, downloadedDefinitionJson, sha);
  ASSERT_EQ(
      handler.saveDownloadedDefinition(701, 1, downloadedDefinitionJson, sha),
      Supla::Suplet::ServerConfigResult::Applied);
  ASSERT_EQ(
      handler.applyAssignmentJson("{\"instanceId\":71,\"definitionId\":701,"
                                  "\"definitionVersion\":1}",
                                  701,
                                  1),
      Supla::Suplet::ServerConfigResult::Applied);
  ASSERT_EQ(
      handler.applyAssignmentJson("{\"instanceId\":72,\"definitionId\":701,"
                                  "\"definitionVersion\":1}",
                                  701,
                                  1),
      Supla::Suplet::ServerConfigResult::Applied);

  EXPECT_EQ(handler.removeAssignment(71),
            Supla::Suplet::ServerConfigResult::Removed);
  EXPECT_TRUE(cache.contains(701, 1));
  EXPECT_EQ(downloadedDefinitions.getCount(cache), 1);
  EXPECT_EQ(registry.findDefinition(701, 1), nullptr);
}

TEST(SupletServerConfigTests,
     DownloadedDefinitionMaxInstancesSurvivesCacheReload) {
  InMemoryConfig config;
  FakeSha256Provider shaProvider;
  Supla::Suplet::DefinitionCache cache(&config, &shaProvider);
  Supla::Suplet::DownloadedDefinitionStore downloadedDefinitions;
  Supla::Suplet::Registry registry;
  uint8_t sha[32] = {};
  makeSha(&shaProvider, downloadedDefinitionJson, sha);
  ASSERT_TRUE(cache.save(701, 1, downloadedDefinitionJson, sha));

  Supla::Suplet::JsonDefinition loadedDefinition;
  ASSERT_TRUE(downloadedDefinitions.load(cache, 701, 1, &loadedDefinition));
  EXPECT_EQ(loadedDefinition.getDefinition()->maxInstances, 3);
}

TEST(SupletServerConfigTests, DownloadedDefinitionMaxInstancesIsEnforced) {
  InMemoryConfig config;
  FakeSha256Provider shaProvider;
  Supla::Suplet::DefinitionCache cache(&config, &shaProvider);
  Supla::Suplet::DownloadedDefinitionStore downloadedDefinitions;
  Supla::Suplet::Manager manager(&config);
  Supla::Suplet::Registry registry;
  Supla::Suplet::ServerConfigHandler handler(
      &manager, &registry, &cache, &downloadedDefinitions);
  uint8_t sha[32] = {};
  makeSha(&shaProvider, downloadedDefinitionJson, sha);
  ASSERT_EQ(
      handler.saveDownloadedDefinition(701, 1, downloadedDefinitionJson, sha),
      Supla::Suplet::ServerConfigResult::Applied);
  ASSERT_EQ(
      handler.applyAssignmentJson("{\"instanceId\":71,\"definitionId\":701,"
                                  "\"definitionVersion\":1}",
                                  701,
                                  1),
      Supla::Suplet::ServerConfigResult::Applied);
  ASSERT_EQ(
      handler.applyAssignmentJson("{\"instanceId\":72,\"definitionId\":701,"
                                  "\"definitionVersion\":1}",
                                  701,
                                  1),
      Supla::Suplet::ServerConfigResult::Applied);
  ASSERT_EQ(
      handler.applyAssignmentJson("{\"instanceId\":73,\"definitionId\":701,"
                                  "\"definitionVersion\":1}",
                                  701,
                                  1),
      Supla::Suplet::ServerConfigResult::Applied);

  EXPECT_EQ(
      handler.applyAssignmentJson("{\"instanceId\":74,\"definitionId\":701,"
                                  "\"definitionVersion\":1}",
                                  701,
                                  1),
      Supla::Suplet::ServerConfigResult::InstanceLimitExceeded);
  EXPECT_EQ(manager.getInstanceTable()->findByInstanceId(74), nullptr);
}

TEST(SupletServerConfigTests, RuntimeLoadsDownloadedDefinitionOnDemand) {
  Supla::Channel::resetToDefaults();
  InMemoryConfig config;
  FakeSha256Provider shaProvider;
  Supla::Suplet::DefinitionCache cache(&config, &shaProvider);
  Supla::Suplet::DownloadedDefinitionStore downloadedDefinitions;
  Supla::Suplet::Manager manager(&config);
  Supla::Suplet::Registry registry;
  Supla::Suplet::ServerConfigHandler handler(
      &manager, &registry, &cache, &downloadedDefinitions);
  manager.setRegistry(&registry);
  manager.setServerConfigHandler(&handler);

  uint8_t sha[32] = {};
  makeSha(&shaProvider, downloadedDefinitionJson, sha);
  ASSERT_EQ(
      handler.saveDownloadedDefinition(701, 1, downloadedDefinitionJson, sha),
      Supla::Suplet::ServerConfigResult::Applied);
  ASSERT_EQ(
      handler.applyAssignmentJson(downloadedAssignmentJson, 701, 1),
      Supla::Suplet::ServerConfigResult::Applied);
  ASSERT_EQ(registry.findDefinition(701, 1), nullptr);

  ASSERT_TRUE(manager.loadRuntimeElements());
  EXPECT_EQ(manager.getRuntimeElementCount(), 1);

  manager.deleteRuntimeElements();
  while (Supla::Element::begin() != nullptr) {
    delete Supla::Element::begin();
  }
  Supla::Channel::resetToDefaults();
}

TEST(SupletServerConfigTests, SavesDownloadedDefinitionFromCommandJson) {
  InMemoryConfig config;
  FakeSha256Provider shaProvider;
  Supla::Suplet::DefinitionCache cache(&config, &shaProvider);
  Supla::Suplet::DownloadedDefinitionStore downloadedDefinitions;
  Supla::Suplet::Manager manager(&config);
  Supla::Suplet::Registry registry;
  Supla::Suplet::ServerConfigHandler handler(
      &manager, &registry, &cache, &downloadedDefinitions);
  uint8_t sha[32] = {};
  char shaHex[65] = {};
  char escapedDefinitionJson[1024] = {};
  makeSha(&shaProvider, downloadedDefinitionJson, sha);
  shaToHex(sha, shaHex, sizeof(shaHex));
  escapeJsonString(downloadedDefinitionJson,
                   escapedDefinitionJson,
                   sizeof(escapedDefinitionJson));

  char command[1400] = {};
  snprintf(command,
           sizeof(command),
           "{"
           "\"op\":\"saveDefinition\","
           "\"definitionId\":701,"
           "\"definitionVersion\":1,"
           "\"sha256\":\"%s\","
           "\"definitionJson\":\"%s\""
           "}",
           shaHex,
           escapedDefinitionJson);

  EXPECT_EQ(
      handler.applyCommandJson(command),
      Supla::Suplet::ServerConfigResult::Applied);
  EXPECT_EQ(downloadedDefinitions.getCount(cache), 1);
  EXPECT_EQ(registry.findDefinition(701, 1), nullptr);

  EXPECT_EQ(handler.applyCommandJson(
                "{\"op\":\"removeDefinition\",\"definitionId\":701,"
                "\"definitionVersion\":1}"),
            Supla::Suplet::ServerConfigResult::Removed);
  EXPECT_EQ(downloadedDefinitions.getCount(cache), 0);
  EXPECT_EQ(registry.findDefinition(701, 1), nullptr);
}

TEST(SupletServerConfigTests, RejectsInvalidDownloadedDefinition) {
  InMemoryConfig config;
  FakeSha256Provider shaProvider;
  Supla::Suplet::DefinitionCache cache(&config, &shaProvider);
  Supla::Suplet::DownloadedDefinitionStore downloadedDefinitions;
  Supla::Suplet::Manager manager(&config);
  Supla::Suplet::Registry registry;
  Supla::Suplet::ServerConfigHandler handler(
      &manager, &registry, &cache, &downloadedDefinitions);
  uint8_t sha[32] = {};
  const char badJson[] = "{\"definitionId\":701,\"definitionVersion\":1}";
  makeSha(&shaProvider, badJson, sha);

  EXPECT_EQ(handler.saveDownloadedDefinition(701, 1, badJson, sha),
            Supla::Suplet::ServerConfigResult::InvalidDefinition);
  EXPECT_EQ(downloadedDefinitions.getCount(cache), 0);
}

TEST(SupletServerConfigTests,
     DownloadedDefinitionsLoadIsAtomicOnBadCacheEntry) {
  InMemoryConfig config;
  FakeSha256Provider shaProvider;
  Supla::Suplet::DefinitionCache cache(&config, &shaProvider);
  Supla::Suplet::DownloadedDefinitionStore downloadedDefinitions;
  Supla::Suplet::Registry registry;
  uint8_t sha[32] = {};

  makeSha(&shaProvider, downloadedDefinitionJson, sha);
  ASSERT_TRUE(cache.save(701, 1, downloadedDefinitionJson, sha));

  const char badJson[] =
      "{"
      "\"schemaVersion\":1,"
      "\"handlerVersion\":1,"
      "\"definitionId\":702,"
      "\"definitionVersion\":1,"
      "\"category\":\"virtual\","
      "\"kind\":\"virtualRelay\""
      "}";
  makeSha(&shaProvider, badJson, sha);
  ASSERT_TRUE(cache.save(702, 1, badJson, sha));

  Supla::Suplet::JsonDefinition loadedDefinition;
  EXPECT_FALSE(downloadedDefinitions.load(cache, 702, 1, &loadedDefinition));
  EXPECT_EQ(downloadedDefinitions.getCount(cache), 2);
  EXPECT_EQ(registry.findDefinition(701, 1), nullptr);
  EXPECT_EQ(registry.findDefinition(702, 1), nullptr);
}

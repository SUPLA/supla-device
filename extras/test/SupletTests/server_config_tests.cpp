// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

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

class DownloadedZeroChannelRuntimeHandler
    : public Supla::Suplet::RuntimeHandler {
 public:
  uint8_t getRequiredElementCount(
      const Supla::Suplet::Definition &,
      const Supla::Suplet::InstanceRecord &) const override {
    return 1;
  }

  bool createElements(const Supla::Suplet::Definition &,
                      const Supla::Suplet::InstanceRecord &,
                      Supla::Element **created,
                      uint8_t createdSize,
                      Supla::Suplet::ChannelMap *) override {
    if (created == nullptr || createdSize != 1) {
      return false;
    }
    created[0] = new Supla::Element;
    return created[0] != nullptr;
  }
};

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

uint32_t calculateTestCrc32(const char *data) {
  uint32_t crc = 0xFFFFFFFFUL;
  for (const char *ptr = data; *ptr != '\0'; ptr++) {
    crc ^= static_cast<uint8_t>(*ptr);
    for (uint8_t bit = 0; bit < 8; bit++) {
      crc = (crc >> 1) ^ (0xEDB88320UL & (0U - (crc & 1U)));
    }
  }
  return crc ^ 0xFFFFFFFFUL;
}

Supla::Suplet::ChannelDefinition relayChannels[] = {
    {1,
     Supla::Suplet::ChannelKind::VirtualRelay,
     SUPLA_CHANNELFNC_POWERSWITCH,
     nullptr},
};

Supla::Suplet::ChannelDefinition relayChannelsWithExtra[] = {
    {1,
     Supla::Suplet::ChannelKind::VirtualRelay,
     SUPLA_CHANNELFNC_POWERSWITCH,
     nullptr},
    {3,
     Supla::Suplet::ChannelKind::VirtualRelay,
     SUPLA_CHANNELFNC_POWERSWITCH,
     nullptr},
};

Supla::Suplet::ChannelDefinition relayChannelsChangedFunction[] = {
    {1,
     Supla::Suplet::ChannelKind::VirtualRelay,
     SUPLA_CHANNELFNC_LIGHTSWITCH,
     nullptr},
};

Supla::Suplet::ChannelDefinition relayChannelsMissingOld[] = {
    {3,
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

Supla::Suplet::Definition makeRelayDefinitionVersion(uint16_t version) {
  auto definition = makeRelayDefinition();
  definition.definitionVersion = version;
  return definition;
}

Supla::Suplet::Definition makeRelayDefinitionVersionWithExtra(
    uint16_t version) {
  auto definition = makeRelayDefinitionVersion(version);
  definition.channels = relayChannelsWithExtra;
  definition.channelCount = 2;
  return definition;
}

Supla::Suplet::Definition makeRelayDefinitionVersionWithChangedFunction(
    uint16_t version) {
  auto definition = makeRelayDefinitionVersion(version);
  definition.channels = relayChannelsChangedFunction;
  definition.channelCount = 1;
  return definition;
}

Supla::Suplet::Definition makeRelayDefinitionVersionMissingOldChannel(
    uint16_t version) {
  auto definition = makeRelayDefinitionVersion(version);
  definition.channels = relayChannelsMissingOld;
  definition.channelCount = 1;
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
"\"definitionId\":1701,"
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
"\"definitionId\":1701,"
"\"definitionVersion\":1"
"}";

const char downloadedZeroChannelDefinitionJson[] =
"{"
"\"schemaVersion\":1,"
"\"handlerVersion\":1,"
"\"definitionId\":1702,"
"\"definitionVersion\":1,"
"\"maxInstances\":4,"
"\"maxArtifactSize\":4,"
"\"category\":\"virtual\","
"\"kind\":\"virtualRelay\","
"\"channels\":[]"
"}";

const char conflictingDownloadedDefinitionJson[] =
"{"
"\"schemaVersion\":1,"
"\"handlerVersion\":1,"
"\"definitionId\":1701,"
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

const char collidingDownloadedDefinitionA[] =
"{\"schemaVersion\":1,\"handlerVersion\":1,\"definitionId\":1706,"
"\"definitionVersion\":1,\"maxInstances\":2,\"category\":\"virtual\","
"\"kind\":\"virtualRelay\",\"name\":\"uvwJ6UqzzqaU\",\"channels\":[{"
"\"channelId\":1,\"kind\":\"virtualRelay\","
"\"function\":\"powerSwitch\"}]}";

const char collidingDownloadedDefinitionB[] =
"{\"schemaVersion\":1,\"handlerVersion\":1,\"definitionId\":1706,"
"\"definitionVersion\":1,\"maxInstances\":2,\"category\":\"virtual\","
"\"kind\":\"virtualRelay\",\"name\":\"uzK0KBdUO2CN\",\"channels\":[{"
"\"channelId\":1,\"kind\":\"virtualRelay\","
"\"function\":\"powerSwitch\"}]}";

const char collidingDownloadedAssignmentJson[] =
"{\"instanceId\":76,\"definitionId\":1706,"
"\"definitionVersion\":1}";

void forceDownloadedDefinitionActiveVariantToMissingB(InMemoryConfig *config) {
  ASSERT_NE(config, nullptr);
  config->uint8Values["spld0_act"] = 2;
  config->blobs.erase("spld0_2");
  config->blobs.erase("spld0_2c0");
}

}  // namespace

TEST(SupletServerConfigTests, AppliesBuiltInAssignmentAndRequestsRefresh) {
  InMemoryConfig config;
  Supla::Suplet::Manager manager(&config);
  Supla::Suplet::Registry registry;
  auto definition = makeRelayDefinition();
  definition.maxArtifactSize = 4;
  ASSERT_TRUE(registry.add(&definition, 4));
  Supla::Suplet::ServerConfigHandler handler(&manager, &registry);

  EXPECT_EQ(handler.applyAssignmentJson(relayAssignment, 700, 2),
            Supla::Suplet::ServerConfigResult::Applied);
  EXPECT_TRUE(handler.isRuntimeRefreshRequired());

  auto record = manager.getInstanceTable()->findByInstanceId(70);
  ASSERT_NE(record, nullptr);
  EXPECT_EQ(record->revision, 1u);
  EXPECT_EQ(record->channelMap.getChannelNumber(1),
            Supla::Suplet::kInvalidChannelNumber);

  const uint8_t artifact[] = {1, 2, 3, 4};
  Supla::Suplet::ArtifactStorageHandle stagedArtifact;
  ASSERT_TRUE(manager.beginStagedArtifact(
      record->instanceId, sizeof(artifact), &stagedArtifact));
  ASSERT_TRUE(manager.writeStagedArtifactChunk(
      &stagedArtifact, artifact, sizeof(artifact)));
  record->artifactSize = sizeof(artifact);
  record->artifactCrc32 =
      Supla::Suplet::Storage::stagedArtifactCrc32(stagedArtifact);
  ASSERT_TRUE(manager.save(&stagedArtifact));

  EXPECT_EQ(handler.applyAssignmentJson(relayAssignment, 700, 2),
            Supla::Suplet::ServerConfigResult::Applied);
  record = manager.getInstanceTable()->findByInstanceId(70);
  ASSERT_NE(record, nullptr);
  EXPECT_EQ(record->revision, 2u);
  EXPECT_EQ(record->artifactSize, sizeof(artifact));
  uint8_t loadedArtifact[sizeof(artifact)] = {};
  ASSERT_TRUE(manager.readArtifact(
      record->instanceId, 0, loadedArtifact, sizeof(loadedArtifact)));
  EXPECT_EQ(memcmp(loadedArtifact, artifact, sizeof(artifact)), 0);
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

TEST(SupletServerConfigTests, UpgradesInstanceToNewerAddOnlyDefinition) {
  InMemoryConfig config;
  Supla::Suplet::Manager manager(&config);
  Supla::Suplet::Registry registry;
  auto v1 = makeRelayDefinitionVersion(1);
  auto v2 = makeRelayDefinitionVersionWithExtra(2);
  ASSERT_TRUE(registry.add(&v1, 4));
  ASSERT_TRUE(registry.add(&v2, 4));
  Supla::Suplet::ServerConfigHandler handler(&manager, &registry);

  ASSERT_EQ(handler.applyInstanceParams(80,
                                        v1.definitionId,
                                        v1.definitionVersion,
                                        nullptr,
                                        0),
            Supla::Suplet::ServerConfigResult::Applied);
  auto record = manager.getInstanceTable()->findByInstanceId(80);
  ASSERT_NE(record, nullptr);
  ASSERT_TRUE(record->channelMap.add(1, 12));
  handler.clearRuntimeRefreshRequired();

  EXPECT_EQ(handler.applyInstanceUpgrade(80,
                                         v1.definitionId,
                                         v1.definitionVersion,
                                         v2.definitionVersion,
                                         nullptr,
                                         0),
            Supla::Suplet::ServerConfigResult::Applied);
  EXPECT_TRUE(handler.isRuntimeRefreshRequired());

  record = manager.getInstanceTable()->findByInstanceId(80);
  ASSERT_NE(record, nullptr);
  EXPECT_EQ(record->definitionVersion, 2);
  EXPECT_EQ(record->channelMap.getChannelNumber(1), 12);
  EXPECT_EQ(record->channelMap.getChannelNumber(3),
            Supla::Suplet::kInvalidChannelNumber);
}

TEST(SupletServerConfigTests, RejectsUpgradeWhenInstanceIsMissing) {
  InMemoryConfig config;
  Supla::Suplet::Manager manager(&config);
  Supla::Suplet::Registry registry;
  auto v1 = makeRelayDefinitionVersion(1);
  auto v2 = makeRelayDefinitionVersionWithExtra(2);
  ASSERT_TRUE(registry.add(&v1, 4));
  ASSERT_TRUE(registry.add(&v2, 4));
  Supla::Suplet::ServerConfigHandler handler(&manager, &registry);

  EXPECT_EQ(handler.applyInstanceUpgrade(80,
                                         v1.definitionId,
                                         v1.definitionVersion,
                                         v2.definitionVersion,
                                         nullptr,
                                         0),
            Supla::Suplet::ServerConfigResult::InstanceNotFound);
}

TEST(SupletServerConfigTests, RejectsUpgradeFromMismatchedVersion) {
  InMemoryConfig config;
  Supla::Suplet::Manager manager(&config);
  Supla::Suplet::Registry registry;
  auto v1 = makeRelayDefinitionVersion(1);
  auto v2 = makeRelayDefinitionVersionWithExtra(2);
  ASSERT_TRUE(registry.add(&v1, 4));
  ASSERT_TRUE(registry.add(&v2, 4));
  Supla::Suplet::ServerConfigHandler handler(&manager, &registry);
  ASSERT_EQ(handler.applyInstanceParams(80,
                                        v1.definitionId,
                                        v1.definitionVersion,
                                        nullptr,
                                        0),
            Supla::Suplet::ServerConfigResult::Applied);

  EXPECT_EQ(handler.applyInstanceUpgrade(80,
                                         v1.definitionId,
                                         2,
                                         3,
                                         nullptr,
                                         0),
            Supla::Suplet::ServerConfigResult::VersionMismatch);
}

TEST(SupletServerConfigTests, RejectsUpgradeToSameOrLowerVersion) {
  InMemoryConfig config;
  Supla::Suplet::Manager manager(&config);
  Supla::Suplet::Registry registry;
  auto v1 = makeRelayDefinitionVersion(1);
  ASSERT_TRUE(registry.add(&v1, 4));
  Supla::Suplet::ServerConfigHandler handler(&manager, &registry);
  ASSERT_EQ(handler.applyInstanceParams(80,
                                        v1.definitionId,
                                        v1.definitionVersion,
                                        nullptr,
                                        0),
            Supla::Suplet::ServerConfigResult::Applied);

  EXPECT_EQ(handler.applyInstanceUpgrade(80,
                                         v1.definitionId,
                                         v1.definitionVersion,
                                         v1.definitionVersion,
                                         nullptr,
                                         0),
            Supla::Suplet::ServerConfigResult::InvalidArgument);
}

TEST(SupletServerConfigTests, RejectsUpgradeWhenTargetDefinitionIsMissing) {
  InMemoryConfig config;
  Supla::Suplet::Manager manager(&config);
  Supla::Suplet::Registry registry;
  auto v1 = makeRelayDefinitionVersion(1);
  ASSERT_TRUE(registry.add(&v1, 4));
  Supla::Suplet::ServerConfigHandler handler(&manager, &registry);
  ASSERT_EQ(handler.applyInstanceParams(80,
                                        v1.definitionId,
                                        v1.definitionVersion,
                                        nullptr,
                                        0),
            Supla::Suplet::ServerConfigResult::Applied);

  EXPECT_EQ(handler.applyInstanceUpgrade(80,
                                         v1.definitionId,
                                         v1.definitionVersion,
                                         2,
                                         nullptr,
                                         0),
            Supla::Suplet::ServerConfigResult::DefinitionNotFound);
}

TEST(SupletServerConfigTests, RejectsUpgradeRemovingOldChannelId) {
  InMemoryConfig config;
  Supla::Suplet::Manager manager(&config);
  Supla::Suplet::Registry registry;
  auto v1 = makeRelayDefinitionVersion(1);
  auto v2 = makeRelayDefinitionVersionMissingOldChannel(2);
  ASSERT_TRUE(registry.add(&v1, 4));
  ASSERT_TRUE(registry.add(&v2, 4));
  Supla::Suplet::ServerConfigHandler handler(&manager, &registry);
  ASSERT_EQ(handler.applyInstanceParams(80,
                                        v1.definitionId,
                                        v1.definitionVersion,
                                        nullptr,
                                        0),
            Supla::Suplet::ServerConfigResult::Applied);

  EXPECT_EQ(handler.applyInstanceUpgrade(80,
                                         v1.definitionId,
                                         v1.definitionVersion,
                                         v2.definitionVersion,
                                         nullptr,
                                         0),
            Supla::Suplet::ServerConfigResult::TopologyChangeNotAllowed);
}

TEST(SupletServerConfigTests, RejectsUpgradeChangingOldChannelFunction) {
  InMemoryConfig config;
  Supla::Suplet::Manager manager(&config);
  Supla::Suplet::Registry registry;
  auto v1 = makeRelayDefinitionVersion(1);
  auto v2 = makeRelayDefinitionVersionWithChangedFunction(2);
  ASSERT_TRUE(registry.add(&v1, 4));
  ASSERT_TRUE(registry.add(&v2, 4));
  Supla::Suplet::ServerConfigHandler handler(&manager, &registry);
  ASSERT_EQ(handler.applyInstanceParams(80,
                                        v1.definitionId,
                                        v1.definitionVersion,
                                        nullptr,
                                        0),
            Supla::Suplet::ServerConfigResult::Applied);

  EXPECT_EQ(handler.applyInstanceUpgrade(80,
                                         v1.definitionId,
                                         v1.definitionVersion,
                                         v2.definitionVersion,
                                         nullptr,
                                         0),
            Supla::Suplet::ServerConfigResult::TopologyChangeNotAllowed);
}

TEST(SupletServerConfigTests, UpgradeKeepsCreateOnlyParamsAndUpdatesEditable) {
  InMemoryConfig config;
  Supla::Suplet::Manager manager(&config);
  Supla::Suplet::Registry registry;
  auto v1 = makeParameterizedRelayDefinition();
  v1.definitionVersion = 1;
  auto v2 = makeParameterizedRelayDefinition();
  v2.definitionVersion = 2;
  ASSERT_TRUE(registry.add(&v1, 4));
  ASSERT_TRUE(registry.add(&v2, 4));
  Supla::Suplet::ServerConfigHandler handler(&manager, &registry);

  const char paramsV1[] =
      "{\"relay.count\":2,\"mode\":\"avg\",\"host\":\"192.168.1.50\"}";
  const char paramsV2[] =
      "{\"relay.count\":2,\"mode\":\"max\",\"host\":\"192.168.1.51\"}";
  ASSERT_EQ(handler.applyInstanceParams(80,
                                        v1.definitionId,
                                        v1.definitionVersion,
                                        paramsV1,
                                        strlen(paramsV1)),
            Supla::Suplet::ServerConfigResult::Applied);

  EXPECT_EQ(handler.applyInstanceUpgrade(80,
                                         v1.definitionId,
                                         v1.definitionVersion,
                                         v2.definitionVersion,
                                         paramsV2,
                                         strlen(paramsV2)),
            Supla::Suplet::ServerConfigResult::Applied);

  auto record = manager.getInstanceTable()->findByInstanceId(80);
  ASSERT_NE(record, nullptr);
  EXPECT_EQ(record->definitionVersion, 2);
  EXPECT_EQ(record->configSize, strlen(paramsV2));
  EXPECT_EQ(memcmp(record->config, paramsV2, strlen(paramsV2)), 0);
}

TEST(SupletServerConfigTests, RejectsCreateOnlyParamChangeOnUpgrade) {
  InMemoryConfig config;
  Supla::Suplet::Manager manager(&config);
  Supla::Suplet::Registry registry;
  auto v1 = makeParameterizedRelayDefinition();
  v1.definitionVersion = 1;
  auto v2 = makeParameterizedRelayDefinition();
  v2.definitionVersion = 2;
  ASSERT_TRUE(registry.add(&v1, 4));
  ASSERT_TRUE(registry.add(&v2, 4));
  Supla::Suplet::ServerConfigHandler handler(&manager, &registry);

  const char paramsV1[] =
      "{\"relay.count\":2,\"mode\":\"avg\",\"host\":\"192.168.1.50\"}";
  const char paramsV2[] =
      "{\"relay.count\":3,\"mode\":\"max\",\"host\":\"192.168.1.51\"}";
  ASSERT_EQ(handler.applyInstanceParams(80,
                                        v1.definitionId,
                                        v1.definitionVersion,
                                        paramsV1,
                                        strlen(paramsV1)),
            Supla::Suplet::ServerConfigResult::Applied);

  EXPECT_EQ(handler.applyInstanceUpgrade(80,
                                         v1.definitionId,
                                         v1.definitionVersion,
                                         v2.definitionVersion,
                                         paramsV2,
                                         strlen(paramsV2)),
            Supla::Suplet::ServerConfigResult::CreateOnlyParamChanged);
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
                "{\"op\":\"saveDefinition\",\"definitionId\":1701,"
                "\"definitionVersion\":1}"),
            Supla::Suplet::ServerConfigResult::InvalidArgument);
}

TEST(SupletServerConfigTests, SavesDownloadedDefinitionAndAppliesAssignment) {
  InMemoryConfig config;
  Supla::Suplet::DefinitionCache cache(&config);
  Supla::Suplet::DownloadedDefinitionStore downloadedDefinitions;
  Supla::Suplet::Manager manager(&config);
  Supla::Suplet::Registry registry;
  Supla::Suplet::ServerConfigHandler handler(
      &manager, &registry, &cache, &downloadedDefinitions);

  EXPECT_EQ(
      handler.saveDownloadedDefinition(1701, 1, downloadedDefinitionJson),
      Supla::Suplet::ServerConfigResult::Applied);
  EXPECT_EQ(
      handler.saveDownloadedDefinition(1701, 1, downloadedDefinitionJson),
      Supla::Suplet::ServerConfigResult::Applied);
  EXPECT_EQ(downloadedDefinitions.getCount(cache), 1);
  EXPECT_EQ(registry.findDefinition(1701, 1), nullptr);
  Supla::Suplet::JsonDefinition loadedDefinition;
  ASSERT_TRUE(downloadedDefinitions.load(cache, 1701, 1, &loadedDefinition));
  EXPECT_EQ(loadedDefinition.getDefinition()->maxInstances, 3);
  EXPECT_EQ(
      handler.applyAssignmentJson(downloadedAssignmentJson, 1701, 1),
      Supla::Suplet::ServerConfigResult::Applied);

  auto record = manager.getInstanceTable()->findByInstanceId(71);
  ASSERT_NE(record, nullptr);
  EXPECT_EQ(record->definitionId, 1701u);
}

TEST(SupletServerConfigTests,
     ResolvesRuntimeHandlerForDownloadedZeroChannelDefinition) {
  Supla::Channel::resetToDefaults();
  InMemoryConfig config;
  Supla::Suplet::DefinitionCache cache(&config);
  Supla::Suplet::DownloadedDefinitionStore downloadedDefinitions;
  Supla::Suplet::Manager manager(&config);
  Supla::Suplet::Registry registry;
  Supla::Suplet::CapabilityRegistry capabilities;
  DownloadedZeroChannelRuntimeHandler runtimeHandler;
  Supla::Suplet::Capability capability = {};
  capability.category = Supla::Suplet::Category::Virtual;
  capability.kind = Supla::Suplet::Kind::VirtualRelay;
  capability.minSchemaVersion = 1;
  capability.maxSchemaVersion = 1;
  capability.handlerVersion = 1;
  capability.maxInstances = 4;
  capability.supportsDownloadedDefinition = 1;
  capability.maxArtifactSize = 4;
  capability.runtimeHandler = &runtimeHandler;
  ASSERT_TRUE(capabilities.add(capability));
  manager.setCapabilityRegistry(&capabilities);
  manager.setRegistry(&registry);
  Supla::Suplet::ServerConfigHandler handler(
      &manager, &registry, &cache, &downloadedDefinitions);
  manager.setServerConfigHandler(&handler);

  ASSERT_EQ(handler.saveDownloadedDefinition(
                1702, 1, downloadedZeroChannelDefinitionJson),
            Supla::Suplet::ServerConfigResult::Applied);
  Supla::Suplet::JsonDefinition loaded;
  ASSERT_TRUE(handler.loadDownloadedDefinition(1702, 1, &loaded));
  EXPECT_EQ(loaded.getDefinition()->runtimeHandler, &runtimeHandler);
  ASSERT_EQ(handler.applyAssignmentJson(
                "{\"instanceId\":75,\"definitionId\":1702,"
                "\"definitionVersion\":1}",
                1702,
                1),
            Supla::Suplet::ServerConfigResult::Applied);

  ASSERT_TRUE(manager.loadRuntimeElements());
  EXPECT_EQ(manager.getRuntimeElementCount(), 1);
  manager.deleteRuntimeElements();
  while (Supla::Element::begin() != nullptr) {
    delete Supla::Element::begin();
  }
  Supla::Channel::resetToDefaults();
}

TEST(SupletServerConfigTests,
     RejectsDownloadedDefinitionOutsideRuntimeCapability) {
  InMemoryConfig config;
  Supla::Suplet::DefinitionCache cache(&config);
  Supla::Suplet::DownloadedDefinitionStore downloadedDefinitions;
  Supla::Suplet::Manager manager(&config);
  Supla::Suplet::Registry registry;
  Supla::Suplet::CapabilityRegistry capabilities;
  Supla::Suplet::Capability capability = {};
  capability.category = Supla::Suplet::Category::Virtual;
  capability.kind = Supla::Suplet::Kind::VirtualRelay;
  capability.minSchemaVersion = 1;
  capability.maxSchemaVersion = 1;
  capability.handlerVersion = 1;
  capability.maxInstances = 4;
  capability.supportsDownloadedDefinition = 1;
  capability.maxArtifactSize = 4;
  ASSERT_TRUE(capabilities.add(capability));
  manager.setCapabilityRegistry(&capabilities);
  Supla::Suplet::ServerConfigHandler handler(
      &manager, &registry, &cache, &downloadedDefinitions);

  const char tooLargeArtifact[] =
      "{\"schemaVersion\":1,\"handlerVersion\":1,"
      "\"definitionId\":1703,\"definitionVersion\":1,"
      "\"maxInstances\":4,\"maxArtifactSize\":8,"
      "\"category\":\"virtual\",\"kind\":\"virtualRelay\","
      "\"channels\":[]}";
  EXPECT_EQ(handler.saveDownloadedDefinition(1703, 1, tooLargeArtifact),
            Supla::Suplet::ServerConfigResult::InvalidDefinition);

  const char unsupportedSchema[] =
      "{\"schemaVersion\":2,\"handlerVersion\":1,"
      "\"definitionId\":1704,\"definitionVersion\":1,"
      "\"maxInstances\":4,\"category\":\"virtual\","
      "\"kind\":\"virtualRelay\",\"channels\":[]}";
  EXPECT_EQ(handler.saveDownloadedDefinition(1704, 1, unsupportedSchema),
            Supla::Suplet::ServerConfigResult::InvalidDefinition);

  const char noMatchingHandler[] =
      "{\"schemaVersion\":1,\"handlerVersion\":2,"
      "\"definitionId\":1705,\"definitionVersion\":1,"
      "\"maxInstances\":4,\"category\":\"virtual\","
      "\"kind\":\"virtualRelay\",\"channels\":[]}";
  EXPECT_EQ(handler.saveDownloadedDefinition(1705, 1, noMatchingHandler),
            Supla::Suplet::ServerConfigResult::InvalidDefinition);
}

TEST(SupletServerConfigTests, ReplacesUnusedConflictingDownloadedDefinition) {
  InMemoryConfig config;
  Supla::Suplet::DefinitionCache cache(&config);
  Supla::Suplet::DownloadedDefinitionStore downloadedDefinitions;
  Supla::Suplet::Manager manager(&config);
  Supla::Suplet::Registry registry;
  Supla::Suplet::ServerConfigHandler handler(
      &manager, &registry, &cache, &downloadedDefinitions);
  ASSERT_EQ(
      handler.saveDownloadedDefinition(1701, 1, downloadedDefinitionJson),
      Supla::Suplet::ServerConfigResult::Applied);

  EXPECT_EQ(handler.saveDownloadedDefinition(
                1701, 1, conflictingDownloadedDefinitionJson),
            Supla::Suplet::ServerConfigResult::Applied);

  char storedJson[1024] = {};
  ASSERT_TRUE(cache.load(1701, 1, storedJson, sizeof(storedJson)));
  EXPECT_STREQ(storedJson, conflictingDownloadedDefinitionJson);
  Supla::Suplet::JsonDefinition loadedDefinition;
  ASSERT_TRUE(downloadedDefinitions.load(cache, 1701, 1, &loadedDefinition));
  EXPECT_STREQ(loadedDefinition.getDefinition()->channels[0].caption,
               "Changed");
}

TEST(SupletServerConfigTests,
     RejectsChangingDownloadedDefinitionUsedByInstance) {
  InMemoryConfig config;
  Supla::Suplet::DefinitionCache cache(&config);
  Supla::Suplet::DownloadedDefinitionStore downloadedDefinitions;
  Supla::Suplet::Manager manager(&config);
  Supla::Suplet::Registry registry;
  Supla::Suplet::ServerConfigHandler handler(
      &manager, &registry, &cache, &downloadedDefinitions);
  ASSERT_EQ(
      handler.saveDownloadedDefinition(1701, 1, downloadedDefinitionJson),
      Supla::Suplet::ServerConfigResult::Applied);
  ASSERT_EQ(
      handler.applyAssignmentJson(downloadedAssignmentJson, 1701, 1),
      Supla::Suplet::ServerConfigResult::Applied);

  EXPECT_EQ(handler.saveDownloadedDefinition(
                1701, 1, conflictingDownloadedDefinitionJson),
            Supla::Suplet::ServerConfigResult::DefinitionCannotBeChanged);

  char storedJson[1024] = {};
  ASSERT_TRUE(cache.load(1701, 1, storedJson, sizeof(storedJson)));
  EXPECT_STREQ(storedJson, downloadedDefinitionJson);
}

TEST(SupletServerConfigTests,
     RejectsCrc32CollisionForUsedDownloadedDefinition) {
  ASSERT_STRNE(collidingDownloadedDefinitionA, collidingDownloadedDefinitionB);
  ASSERT_EQ(strlen(collidingDownloadedDefinitionA),
            strlen(collidingDownloadedDefinitionB));
  ASSERT_EQ(calculateTestCrc32(collidingDownloadedDefinitionA),
            calculateTestCrc32(collidingDownloadedDefinitionB));

  InMemoryConfig config;
  Supla::Suplet::DefinitionCache cache(&config);
  Supla::Suplet::DownloadedDefinitionStore downloadedDefinitions;
  Supla::Suplet::Manager manager(&config);
  Supla::Suplet::Registry registry;
  Supla::Suplet::ServerConfigHandler handler(
      &manager, &registry, &cache, &downloadedDefinitions);
  ASSERT_EQ(handler.saveDownloadedDefinition(
                1706, 1, collidingDownloadedDefinitionA),
            Supla::Suplet::ServerConfigResult::Applied);
  ASSERT_EQ(handler.applyAssignmentJson(
                collidingDownloadedAssignmentJson, 1706, 1),
            Supla::Suplet::ServerConfigResult::Applied);
  handler.clearRuntimeRefreshRequired();

  EXPECT_EQ(handler.saveDownloadedDefinition(
                1706, 1, collidingDownloadedDefinitionB),
            Supla::Suplet::ServerConfigResult::DefinitionCannotBeChanged);
  EXPECT_FALSE(handler.isRuntimeRefreshRequired());

  char storedJson[512] = {};
  ASSERT_TRUE(cache.load(1706, 1, storedJson, sizeof(storedJson)));
  EXPECT_STREQ(storedJson, collidingDownloadedDefinitionA);
}

TEST(SupletServerConfigTests,
     RejectsStagedCrc32CollisionForUsedDownloadedDefinition) {
  ASSERT_EQ(calculateTestCrc32(collidingDownloadedDefinitionA),
            calculateTestCrc32(collidingDownloadedDefinitionB));

  InMemoryConfig config;
  Supla::Suplet::DefinitionCache cache(&config);
  Supla::Suplet::DownloadedDefinitionStore downloadedDefinitions;
  Supla::Suplet::Manager manager(&config);
  Supla::Suplet::Registry registry;
  Supla::Suplet::ServerConfigHandler handler(
      &manager, &registry, &cache, &downloadedDefinitions);
  ASSERT_EQ(handler.saveDownloadedDefinition(
                1706, 1, collidingDownloadedDefinitionA),
            Supla::Suplet::ServerConfigResult::Applied);
  ASSERT_EQ(handler.applyAssignmentJson(
                collidingDownloadedAssignmentJson, 1706, 1),
            Supla::Suplet::ServerConfigResult::Applied);
  handler.clearRuntimeRefreshRequired();

  Supla::Suplet::DefinitionCacheHandle handle = {};
  ASSERT_EQ(handler.beginStagedDownloadedDefinition(
                1706,
                1,
                strlen(collidingDownloadedDefinitionB),
                &handle),
            Supla::Suplet::ServerConfigResult::Applied);
  ASSERT_EQ(handler.writeStagedDownloadedDefinitionChunk(
                handle,
                0,
                reinterpret_cast<const uint8_t *>(
                    collidingDownloadedDefinitionB),
                strlen(collidingDownloadedDefinitionB)),
            Supla::Suplet::ServerConfigResult::Applied);

  EXPECT_EQ(handler.commitStagedDownloadedDefinition(
                handle,
                1706,
                1,
                strlen(collidingDownloadedDefinitionB)),
            Supla::Suplet::ServerConfigResult::DefinitionCannotBeChanged);
  EXPECT_FALSE(handler.isRuntimeRefreshRequired());

  char storedJson[512] = {};
  ASSERT_TRUE(cache.load(1706, 1, storedJson, sizeof(storedJson)));
  EXPECT_STREQ(storedJson, collidingDownloadedDefinitionA);
  handler.abortStagedDownloadedDefinition(handle);
}

TEST(SupletServerConfigTests,
     IdenticalDownloadedDefinitionSaveAndCommitAreNoOps) {
  InMemoryConfig config;
  Supla::Suplet::DefinitionCache cache(&config);
  Supla::Suplet::DownloadedDefinitionStore downloadedDefinitions;
  Supla::Suplet::Manager manager(&config);
  Supla::Suplet::Registry registry;
  Supla::Suplet::ServerConfigHandler handler(
      &manager, &registry, &cache, &downloadedDefinitions);
  ASSERT_EQ(handler.saveDownloadedDefinition(
                1706, 1, collidingDownloadedDefinitionA),
            Supla::Suplet::ServerConfigResult::Applied);
  ASSERT_EQ(handler.applyAssignmentJson(
                collidingDownloadedAssignmentJson, 1706, 1),
            Supla::Suplet::ServerConfigResult::Applied);
  ASSERT_EQ(config.uint8Values["spld0_act"], 1);
  handler.clearRuntimeRefreshRequired();
  const int commitCountAfterFirstSave = config.commitCount;

  EXPECT_EQ(handler.saveDownloadedDefinition(
                1706, 1, collidingDownloadedDefinitionA),
            Supla::Suplet::ServerConfigResult::Applied);
  EXPECT_FALSE(handler.isRuntimeRefreshRequired());
  EXPECT_EQ(config.uint8Values["spld0_act"], 1);
  EXPECT_EQ(config.commitCount, commitCountAfterFirstSave);

  Supla::Suplet::DefinitionCacheHandle handle = {};
  ASSERT_EQ(handler.beginStagedDownloadedDefinition(
                1706,
                1,
                strlen(collidingDownloadedDefinitionA),
                &handle),
            Supla::Suplet::ServerConfigResult::Applied);
  ASSERT_EQ(handler.writeStagedDownloadedDefinitionChunk(
                handle,
                0,
                reinterpret_cast<const uint8_t *>(
                    collidingDownloadedDefinitionA),
                strlen(collidingDownloadedDefinitionA)),
            Supla::Suplet::ServerConfigResult::Applied);
  ASSERT_GT(config.blobs.count("spld0_2c0"), 0);

  EXPECT_EQ(handler.commitStagedDownloadedDefinition(
                handle,
                1706,
                1,
                strlen(collidingDownloadedDefinitionA)),
            Supla::Suplet::ServerConfigResult::Applied);
  EXPECT_FALSE(handler.isRuntimeRefreshRequired());
  EXPECT_EQ(config.uint8Values["spld0_act"], 1);
  EXPECT_EQ(config.blobs.count("spld0_2"), 0);
  EXPECT_EQ(config.blobs.count("spld0_2c0"), 0);
}

TEST(SupletServerConfigTests,
     RejectsChangingUsedDownloadedDefinitionWhenActiveCacheVariantIsStale) {
  InMemoryConfig config;
  Supla::Suplet::DefinitionCache cache(&config);
  Supla::Suplet::DownloadedDefinitionStore downloadedDefinitions;
  Supla::Suplet::Manager manager(&config);
  Supla::Suplet::Registry registry;
  Supla::Suplet::ServerConfigHandler handler(
      &manager, &registry, &cache, &downloadedDefinitions);
  ASSERT_EQ(
      handler.saveDownloadedDefinition(1701, 1, downloadedDefinitionJson),
      Supla::Suplet::ServerConfigResult::Applied);
  ASSERT_EQ(
      handler.applyAssignmentJson(downloadedAssignmentJson, 1701, 1),
      Supla::Suplet::ServerConfigResult::Applied);

  forceDownloadedDefinitionActiveVariantToMissingB(&config);

  EXPECT_EQ(handler.saveDownloadedDefinition(
                1701, 1, conflictingDownloadedDefinitionJson),
            Supla::Suplet::ServerConfigResult::DefinitionCannotBeChanged);

  char storedJson[1024] = {};
  ASSERT_TRUE(cache.load(1701, 1, storedJson, sizeof(storedJson)));
  EXPECT_STREQ(storedJson, downloadedDefinitionJson);
}

TEST(SupletServerConfigTests,
     RejectsStagedChangingUsedDownloadedDefWhenActiveCacheVariantIsStale) {
  InMemoryConfig config;
  Supla::Suplet::DefinitionCache cache(&config);
  Supla::Suplet::DownloadedDefinitionStore downloadedDefinitions;
  Supla::Suplet::Manager manager(&config);
  Supla::Suplet::Registry registry;
  Supla::Suplet::ServerConfigHandler handler(
      &manager, &registry, &cache, &downloadedDefinitions);
  ASSERT_EQ(
      handler.saveDownloadedDefinition(1701, 1, downloadedDefinitionJson),
      Supla::Suplet::ServerConfigResult::Applied);
  ASSERT_EQ(
      handler.applyAssignmentJson(downloadedAssignmentJson, 1701, 1),
      Supla::Suplet::ServerConfigResult::Applied);

  forceDownloadedDefinitionActiveVariantToMissingB(&config);

  Supla::Suplet::DefinitionCacheHandle handle = {};
  ASSERT_EQ(handler.beginStagedDownloadedDefinition(
                1701,
                1,
                strlen(conflictingDownloadedDefinitionJson),
                &handle),
            Supla::Suplet::ServerConfigResult::Applied);
  ASSERT_TRUE(cache.writeStagedChunk(
      handle,
      0,
      reinterpret_cast<const uint8_t *>(conflictingDownloadedDefinitionJson),
      strlen(conflictingDownloadedDefinitionJson)));

  EXPECT_EQ(handler.commitStagedDownloadedDefinition(
                handle,
                1701,
                1,
                strlen(conflictingDownloadedDefinitionJson)),
            Supla::Suplet::ServerConfigResult::DefinitionCannotBeChanged);

  char storedJson[1024] = {};
  ASSERT_TRUE(cache.load(1701, 1, storedJson, sizeof(storedJson)));
  EXPECT_STREQ(storedJson, downloadedDefinitionJson);
}

TEST(SupletServerConfigTests, LoadsMultipleDownloadedDefinitionVersions) {
  InMemoryConfig config;
  Supla::Suplet::DefinitionCache cache(&config);
  Supla::Suplet::DownloadedDefinitionStore downloadedDefinitions;
  Supla::Suplet::Manager manager(&config);
  Supla::Suplet::Registry registry;
  Supla::Suplet::ServerConfigHandler handler(
      &manager, &registry, &cache, &downloadedDefinitions);

  const char v1[] =
      "{"
      "\"schemaVersion\":1,"
      "\"handlerVersion\":1,"
      "\"definitionId\":1701,"
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
      "\"definitionId\":1701,"
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

  ASSERT_EQ(handler.saveDownloadedDefinition(1701, 1, v1),
            Supla::Suplet::ServerConfigResult::Applied);
  ASSERT_EQ(handler.saveDownloadedDefinition(1701, 2, v2),
            Supla::Suplet::ServerConfigResult::Applied);

  EXPECT_EQ(downloadedDefinitions.getCount(cache), 2);
  Supla::Suplet::JsonDefinition loadedV1;
  Supla::Suplet::JsonDefinition loadedV2;
  ASSERT_TRUE(downloadedDefinitions.load(cache, 1701, 1, &loadedV1));
  ASSERT_TRUE(downloadedDefinitions.load(cache, 1701, 2, &loadedV2));
  EXPECT_STREQ(loadedV1.getDefinition()->channels[0].caption,
               "Param relay v1");
  EXPECT_STREQ(loadedV2.getDefinition()->channels[0].caption,
               "Param relay v2");
}

TEST(SupletServerConfigTests, RemovesDownloadedDefinitionWhenUnused) {
  InMemoryConfig config;
  Supla::Suplet::DefinitionCache cache(&config);
  Supla::Suplet::DownloadedDefinitionStore downloadedDefinitions;
  Supla::Suplet::Manager manager(&config);
  Supla::Suplet::Registry registry;
  Supla::Suplet::ServerConfigHandler handler(
      &manager, &registry, &cache, &downloadedDefinitions);

  ASSERT_EQ(
      handler.saveDownloadedDefinition(1701, 1, downloadedDefinitionJson),
      Supla::Suplet::ServerConfigResult::Applied);
  ASSERT_TRUE(cache.contains(1701, 1));
  EXPECT_EQ(registry.findDefinition(1701, 1), nullptr);

  EXPECT_EQ(handler.removeDownloadedDefinition(1701, 1),
            Supla::Suplet::ServerConfigResult::Removed);
  EXPECT_FALSE(cache.contains(1701, 1));
  EXPECT_EQ(downloadedDefinitions.getCount(cache), 0);
  EXPECT_EQ(registry.findDefinition(1701, 1), nullptr);
  EXPECT_TRUE(handler.isRuntimeRefreshRequired());

  EXPECT_EQ(handler.removeDownloadedDefinition(1701, 1),
            Supla::Suplet::ServerConfigResult::DefinitionNotFound);
}

TEST(SupletServerConfigTests, RejectsRemovingDefinitionUsedByInstance) {
  InMemoryConfig config;
  Supla::Suplet::DefinitionCache cache(&config);
  Supla::Suplet::DownloadedDefinitionStore downloadedDefinitions;
  Supla::Suplet::Manager manager(&config);
  Supla::Suplet::Registry registry;
  Supla::Suplet::ServerConfigHandler handler(
      &manager, &registry, &cache, &downloadedDefinitions);
  ASSERT_EQ(
      handler.saveDownloadedDefinition(1701, 1, downloadedDefinitionJson),
      Supla::Suplet::ServerConfigResult::Applied);
  ASSERT_EQ(
      handler.applyAssignmentJson(downloadedAssignmentJson, 1701, 1),
      Supla::Suplet::ServerConfigResult::Applied);

  EXPECT_EQ(handler.removeDownloadedDefinition(1701, 1),
            Supla::Suplet::ServerConfigResult::TopologyChangeNotAllowed);
  EXPECT_TRUE(cache.contains(1701, 1));
  EXPECT_EQ(registry.findDefinition(1701, 1), nullptr);
}

TEST(SupletServerConfigTests, RemoveAssignmentGarbageCollectsUnusedDefinition) {
  InMemoryConfig config;
  Supla::Suplet::DefinitionCache cache(&config);
  Supla::Suplet::DownloadedDefinitionStore downloadedDefinitions;
  Supla::Suplet::Manager manager(&config);
  Supla::Suplet::Registry registry;
  Supla::Suplet::ServerConfigHandler handler(
      &manager, &registry, &cache, &downloadedDefinitions);
  ASSERT_EQ(
      handler.saveDownloadedDefinition(1701, 1, downloadedDefinitionJson),
      Supla::Suplet::ServerConfigResult::Applied);
  ASSERT_EQ(
      handler.applyAssignmentJson(downloadedAssignmentJson, 1701, 1),
      Supla::Suplet::ServerConfigResult::Applied);
  ASSERT_TRUE(cache.contains(1701, 1));

  EXPECT_EQ(handler.removeAssignment(71),
            Supla::Suplet::ServerConfigResult::Removed);
  EXPECT_FALSE(cache.contains(1701, 1));
  EXPECT_EQ(downloadedDefinitions.getCount(cache), 0);
  EXPECT_EQ(registry.findDefinition(1701, 1), nullptr);
}

TEST(SupletServerConfigTests,
     RemoveAssignmentKeepsDefinitionUsedByOtherInstance) {
  InMemoryConfig config;
  Supla::Suplet::DefinitionCache cache(&config);
  Supla::Suplet::DownloadedDefinitionStore downloadedDefinitions;
  Supla::Suplet::Manager manager(&config);
  Supla::Suplet::Registry registry;
  Supla::Suplet::ServerConfigHandler handler(
      &manager, &registry, &cache, &downloadedDefinitions);
  ASSERT_EQ(
      handler.saveDownloadedDefinition(1701, 1, downloadedDefinitionJson),
      Supla::Suplet::ServerConfigResult::Applied);
  ASSERT_EQ(
      handler.applyAssignmentJson("{\"instanceId\":71,\"definitionId\":1701,"
                                  "\"definitionVersion\":1}",
                                  1701,
                                  1),
      Supla::Suplet::ServerConfigResult::Applied);
  ASSERT_EQ(
      handler.applyAssignmentJson("{\"instanceId\":72,\"definitionId\":1701,"
                                  "\"definitionVersion\":1}",
                                  1701,
                                  1),
      Supla::Suplet::ServerConfigResult::Applied);

  EXPECT_EQ(handler.removeAssignment(71),
            Supla::Suplet::ServerConfigResult::Removed);
  EXPECT_TRUE(cache.contains(1701, 1));
  EXPECT_EQ(downloadedDefinitions.getCount(cache), 1);
  EXPECT_EQ(registry.findDefinition(1701, 1), nullptr);
}

TEST(SupletServerConfigTests,
     DownloadedDefinitionMaxInstancesSurvivesCacheReload) {
  InMemoryConfig config;
  Supla::Suplet::DefinitionCache cache(&config);
  Supla::Suplet::DownloadedDefinitionStore downloadedDefinitions;
  Supla::Suplet::Registry registry;
  ASSERT_TRUE(cache.save(1701, 1, downloadedDefinitionJson));

  Supla::Suplet::JsonDefinition loadedDefinition;
  ASSERT_TRUE(downloadedDefinitions.load(cache, 1701, 1, &loadedDefinition));
  EXPECT_EQ(loadedDefinition.getDefinition()->maxInstances, 3);
}

TEST(SupletServerConfigTests, DownloadedDefinitionMaxInstancesIsEnforced) {
  InMemoryConfig config;
  Supla::Suplet::DefinitionCache cache(&config);
  Supla::Suplet::DownloadedDefinitionStore downloadedDefinitions;
  Supla::Suplet::Manager manager(&config);
  Supla::Suplet::Registry registry;
  Supla::Suplet::ServerConfigHandler handler(
      &manager, &registry, &cache, &downloadedDefinitions);
  ASSERT_EQ(
      handler.saveDownloadedDefinition(1701, 1, downloadedDefinitionJson),
      Supla::Suplet::ServerConfigResult::Applied);
  ASSERT_EQ(
      handler.applyAssignmentJson("{\"instanceId\":71,\"definitionId\":1701,"
                                  "\"definitionVersion\":1}",
                                  1701,
                                  1),
      Supla::Suplet::ServerConfigResult::Applied);
  ASSERT_EQ(
      handler.applyAssignmentJson("{\"instanceId\":72,\"definitionId\":1701,"
                                  "\"definitionVersion\":1}",
                                  1701,
                                  1),
      Supla::Suplet::ServerConfigResult::Applied);
  ASSERT_EQ(
      handler.applyAssignmentJson("{\"instanceId\":73,\"definitionId\":1701,"
                                  "\"definitionVersion\":1}",
                                  1701,
                                  1),
      Supla::Suplet::ServerConfigResult::Applied);

  EXPECT_EQ(
      handler.applyAssignmentJson("{\"instanceId\":74,\"definitionId\":1701,"
                                  "\"definitionVersion\":1}",
                                  1701,
                                  1),
      Supla::Suplet::ServerConfigResult::InstanceLimitExceeded);
  EXPECT_EQ(manager.getInstanceTable()->findByInstanceId(74), nullptr);
}

TEST(SupletServerConfigTests, RuntimeLoadsDownloadedDefinitionOnDemand) {
  Supla::Channel::resetToDefaults();
  InMemoryConfig config;
  Supla::Suplet::DefinitionCache cache(&config);
  Supla::Suplet::DownloadedDefinitionStore downloadedDefinitions;
  Supla::Suplet::Manager manager(&config);
  Supla::Suplet::Registry registry;
  Supla::Suplet::ServerConfigHandler handler(
      &manager, &registry, &cache, &downloadedDefinitions);
  manager.setRegistry(&registry);
  manager.setServerConfigHandler(&handler);

  ASSERT_EQ(
      handler.saveDownloadedDefinition(1701, 1, downloadedDefinitionJson),
      Supla::Suplet::ServerConfigResult::Applied);
  ASSERT_EQ(
      handler.applyAssignmentJson(downloadedAssignmentJson, 1701, 1),
      Supla::Suplet::ServerConfigResult::Applied);
  ASSERT_EQ(registry.findDefinition(1701, 1), nullptr);

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
  Supla::Suplet::DefinitionCache cache(&config);
  Supla::Suplet::DownloadedDefinitionStore downloadedDefinitions;
  Supla::Suplet::Manager manager(&config);
  Supla::Suplet::Registry registry;
  Supla::Suplet::ServerConfigHandler handler(
      &manager, &registry, &cache, &downloadedDefinitions);
  char escapedDefinitionJson[1024] = {};
  escapeJsonString(downloadedDefinitionJson,
                   escapedDefinitionJson,
                   sizeof(escapedDefinitionJson));

  char command[1400] = {};
  snprintf(command,
           sizeof(command),
           "{"
           "\"op\":\"saveDefinition\","
           "\"definitionId\":1701,"
           "\"definitionVersion\":1,"
           "\"definitionJson\":\"%s\""
           "}",
           escapedDefinitionJson);

  EXPECT_EQ(
      handler.applyCommandJson(command),
      Supla::Suplet::ServerConfigResult::Applied);
  EXPECT_EQ(downloadedDefinitions.getCount(cache), 1);
  EXPECT_EQ(registry.findDefinition(1701, 1), nullptr);

  EXPECT_EQ(handler.applyCommandJson(
                "{\"op\":\"removeDefinition\",\"definitionId\":1701,"
                "\"definitionVersion\":1}"),
            Supla::Suplet::ServerConfigResult::Removed);
  EXPECT_EQ(downloadedDefinitions.getCount(cache), 0);
  EXPECT_EQ(registry.findDefinition(1701, 1), nullptr);
}

TEST(SupletServerConfigTests, RejectsInvalidDownloadedDefinition) {
  InMemoryConfig config;
  Supla::Suplet::DefinitionCache cache(&config);
  Supla::Suplet::DownloadedDefinitionStore downloadedDefinitions;
  Supla::Suplet::Manager manager(&config);
  Supla::Suplet::Registry registry;
  Supla::Suplet::ServerConfigHandler handler(
      &manager, &registry, &cache, &downloadedDefinitions);
  const char badJson[] = "{\"definitionId\":1701,\"definitionVersion\":1}";

  EXPECT_EQ(handler.saveDownloadedDefinition(1701, 1, badJson),
            Supla::Suplet::ServerConfigResult::InvalidDefinition);
  EXPECT_EQ(downloadedDefinitions.getCount(cache), 0);
}

TEST(SupletServerConfigTests,
     DownloadedDefinitionsLoadIsAtomicOnBadCacheEntry) {
  InMemoryConfig config;
  Supla::Suplet::DefinitionCache cache(&config);
  Supla::Suplet::DownloadedDefinitionStore downloadedDefinitions;
  Supla::Suplet::Registry registry;

  ASSERT_TRUE(cache.save(1701, 1, downloadedDefinitionJson));

  const char badJson[] =
      "{"
      "\"schemaVersion\":1,"
      "\"handlerVersion\":1,"
      "\"definitionId\":702,"
      "\"definitionVersion\":1,"
      "\"category\":\"virtual\","
      "\"kind\":\"virtualRelay\""
      "}";
  ASSERT_TRUE(cache.save(702, 1, badJson));

  Supla::Suplet::JsonDefinition loadedDefinition;
  EXPECT_FALSE(downloadedDefinitions.load(cache, 702, 1, &loadedDefinition));
  EXPECT_EQ(downloadedDefinitions.getCount(cache), 2);
  EXPECT_EQ(registry.findDefinition(1701, 1), nullptr);
  EXPECT_EQ(registry.findDefinition(702, 1), nullptr);
}

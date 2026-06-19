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

#include <SuplaDevice.h>
#include <config_simulator.h>
#include <gtest/gtest.h>
#include <simple_time.h>
#include <supla/channel_element.h>
#include <supla/channels/channel.h>
#include <supla/clock/clock.h>
#include <supla/element.h>
#include <supla/suplet/capability_registry.h>
#include <supla/suplet/manager.h>
#include <supla/suplet/registry.h>
#include <supla/suplet/server_config.h>
#include <storage_mock.h>
#include <timer_mock.h>
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <string.h>

namespace {

class SuplaDeviceSupletStartupTests : public ::testing::Test {
 protected:
  SimpleTime time;

  void SetUp() override {
    if (SuplaDevice.getClock()) {
      delete SuplaDevice.getClock();
    }
    Supla::Channel::resetToDefaults();
  }

  void TearDown() override {
    Supla::Channel::resetToDefaults();
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
    for (size_t i = 0; i < dataSize; i++) {
      sum = static_cast<uint8_t>(sum + data[i] + i);
    }
    for (uint8_t i = 0; i < 32; i++) {
      output[i] = static_cast<uint8_t>(sum + i);
    }
    return true;
  }
};

void makeSha(FakeSha256Provider *provider, const char *json, uint8_t *sha) {
  ASSERT_TRUE(provider->calculate(
      reinterpret_cast<const uint8_t *>(json), strlen(json), sha, 32));
}

void makeDeviceTestSha(const char *data, uint16_t dataSize, uint8_t *sha) {
  ASSERT_NE(sha, nullptr);
  memset(sha, 0, 32);
  for (uint16_t i = 0; i < dataSize; i++) {
    sha[i % 32] = static_cast<uint8_t>(sha[i % 32] + data[i] + i);
  }
}

class DeviceTestShaProvider : public Supla::Suplet::Sha256Provider {
 public:
  bool calculate(const uint8_t *data,
                 size_t dataSize,
                 uint8_t *output,
                 size_t outputSize) override {
    if (data == nullptr || output == nullptr || outputSize < 32 ||
        dataSize > UINT16_MAX) {
      return false;
    }
    memset(output, 0, 32);
    for (uint16_t i = 0; i < dataSize; i++) {
      output[i % 32] = static_cast<uint8_t>(output[i % 32] + data[i] + i);
    }
    return true;
  }
};

int sendSupletDefinitionCalcfg(SuplaDeviceClass *sd,
                               uint32_t definitionId,
                               uint16_t definitionVersion,
                               const char *definitionJson,
                               TDS_DeviceCalCfgResult *result) {
  if (sd == nullptr || definitionJson == nullptr || result == nullptr) {
    return SUPLA_CALCFG_RESULT_FALSE;
  }

  uint8_t sha[32] = {};
  makeDeviceTestSha(definitionJson, strlen(definitionJson), sha);

  TSD_DeviceCalCfgRequest request = {};
  request.ChannelNumber = -1;
  request.SuperUserAuthorized = 1;
  request.Command = SUPLA_CALCFG_CMD_SUPLET_DEFINITION_BEGIN;
  TCalCfg_SupletDefinitionBegin begin = {};
  begin.SessionId = 2345;
  begin.DefinitionId = definitionId;
  begin.DefinitionVersion = definitionVersion;
  begin.JsonSize = strlen(definitionJson);
  memcpy(begin.JsonSha256, sha, sizeof(sha));
  request.DataSize = sizeof(begin);
  memcpy(request.Data, &begin, sizeof(begin));
  int calcfgResult = sd->handleCalcfgFromServer(&request, result);
  if (calcfgResult != SUPLA_CALCFG_RESULT_DONE) {
    return calcfgResult;
  }

  const uint16_t jsonSize = strlen(definitionJson);
  uint16_t offset = 0;
  while (offset < jsonSize) {
    const uint8_t size =
        jsonSize - offset > 53 ? 53 : static_cast<uint8_t>(jsonSize - offset);
    request = {};
    *result = {};
    request.ChannelNumber = -1;
    request.SuperUserAuthorized = 1;
    request.Command = SUPLA_CALCFG_CMD_SUPLET_DEFINITION_CHUNK;
    TCalCfg_SupletDefinitionChunk chunk = {};
    chunk.SessionId = begin.SessionId;
    chunk.Offset = offset;
    chunk.Size = size;
    memcpy(chunk.Data, definitionJson + offset, size);
    request.DataSize = offsetof(TCalCfg_SupletDefinitionChunk, Data) + size;
    memcpy(request.Data, &chunk, request.DataSize);
    calcfgResult = sd->handleCalcfgFromServer(&request, result);
    if (calcfgResult != SUPLA_CALCFG_RESULT_DONE) {
      return calcfgResult;
    }
    offset += size;
  }

  request = {};
  *result = {};
  request.ChannelNumber = -1;
  request.SuperUserAuthorized = 1;
  request.Command = SUPLA_CALCFG_CMD_SUPLET_DEFINITION_COMMIT;
  TCalCfg_SupletSessionRequest commit = {};
  commit.SessionId = begin.SessionId;
  request.DataSize = sizeof(commit);
  memcpy(request.Data, &commit, sizeof(commit));
  return sd->handleCalcfgFromServer(&request, result);
}

int sendSupletInstanceParamsCalcfg(SuplaDeviceClass *sd,
                                   uint8_t instanceId,
                                   uint32_t definitionId,
                                   uint16_t definitionVersion,
                                   const char *paramsJson,
                                   uint16_t paramsSize,
                                   TDS_DeviceCalCfgResult *result) {
  if (sd == nullptr || paramsJson == nullptr || result == nullptr) {
    return SUPLA_CALCFG_RESULT_FALSE;
  }

  uint8_t sha[32] = {};
  makeDeviceTestSha(paramsJson, paramsSize, sha);

  TSD_DeviceCalCfgRequest request = {};
  request.ChannelNumber = -1;
  request.SuperUserAuthorized = 1;
  request.Command = SUPLA_CALCFG_CMD_SUPLET_INSTANCE_BEGIN;
  TCalCfg_SupletInstanceBegin begin = {};
  begin.SessionId = 4321;
  begin.InstanceId = instanceId;
  begin.DefinitionId = definitionId;
  begin.DefinitionVersion = definitionVersion;
  begin.ParamsSize = paramsSize;
  begin.State = SUPLA_CALCFG_SUPLET_INSTANCE_STATE_ACTIVE;
  memcpy(begin.ParamsSha256, sha, sizeof(sha));
  request.DataSize = sizeof(begin);
  memcpy(request.Data, &begin, sizeof(begin));
  int calcfgResult = sd->handleCalcfgFromServer(&request, result);
  if (calcfgResult != SUPLA_CALCFG_RESULT_DONE) {
    return calcfgResult;
  }

  uint16_t offset = 0;
  while (offset < paramsSize) {
    const uint8_t size =
        paramsSize - offset > SUPLA_CALCFG_SUPLET_INSTANCE_CHUNK_MAXSIZE
            ? SUPLA_CALCFG_SUPLET_INSTANCE_CHUNK_MAXSIZE
            : static_cast<uint8_t>(paramsSize - offset);
    request = {};
    *result = {};
    request.ChannelNumber = -1;
    request.SuperUserAuthorized = 1;
    request.Command = SUPLA_CALCFG_CMD_SUPLET_INSTANCE_CHUNK;
    TCalCfg_SupletInstanceChunk chunk = {};
    chunk.SessionId = begin.SessionId;
    chunk.Offset = offset;
    chunk.Size = size;
    memcpy(chunk.Data, paramsJson + offset, size);
    request.DataSize = offsetof(TCalCfg_SupletInstanceChunk, Data) + size;
    memcpy(request.Data, &chunk, request.DataSize);
    calcfgResult = sd->handleCalcfgFromServer(&request, result);
    if (calcfgResult != SUPLA_CALCFG_RESULT_DONE) {
      return calcfgResult;
    }
    offset += size;
  }

  request = {};
  *result = {};
  request.ChannelNumber = -1;
  request.SuperUserAuthorized = 1;
  request.Command = SUPLA_CALCFG_CMD_SUPLET_INSTANCE_COMMIT;
  TCalCfg_SupletSessionRequest commit = {};
  commit.SessionId = begin.SessionId;
  request.DataSize = sizeof(commit);
  memcpy(request.Data, &commit, sizeof(commit));
  return sd->handleCalcfgFromServer(&request, result);
}

const uint8_t relayId = 1;

Supla::Suplet::Definition makeRelayDefinition(
    uint32_t definitionId,
    const char *name = "Runtime virtual relay",
    const char *caption = "Virtual relay") {
  static Supla::Suplet::ChannelDefinition channels[1] = {};
  channels[0].channelId = relayId;
  channels[0].kind = Supla::Suplet::ChannelKind::VirtualRelay;
  channels[0].defaultFunction = SUPLA_CHANNELFNC_POWERSWITCH;
  channels[0].caption = caption;

  Supla::Suplet::Definition definition = {};
  definition.category = Supla::Suplet::Category::Virtual;
  definition.kind = Supla::Suplet::Kind::VirtualRelay;
  definition.schemaVersion = 1;
  definition.handlerVersion = 1;
  definition.definitionId = definitionId;
  definition.definitionVersion = 1;
  definition.name = name;
  definition.channels = channels;
  definition.channelCount = 1;
  return definition;
}

Supla::Suplet::Definition makeParameterizedRelayDefinition(
    uint32_t definitionId) {
  static Supla::Suplet::ParameterDefinition parameters[2] = {};
  parameters[0].key = "relay.count";
  parameters[0].type = Supla::Suplet::ParameterType::UInt8;
  parameters[0].lifecycle = Supla::Suplet::ParameterLifecycle::CreateOnly;
  parameters[0].min = 1;
  parameters[0].max = 16;
  parameters[0].defaultNumber = 1;
  parameters[0].required = 1;
  parameters[0].hasDefault = 1;
  parameters[0].affectsTopology = 1;
  parameters[1].key = "host";
  parameters[1].type = Supla::Suplet::ParameterType::String;
  parameters[1].lifecycle = Supla::Suplet::ParameterLifecycle::Editable;
  parameters[1].required = 1;

  auto definition = makeRelayDefinition(definitionId);
  definition.parameters = parameters;
  definition.parameterCount = 2;
  return definition;
}

void setRequiredSuplaConfig(ConfigSimulator *config) {
  const char guid[SUPLA_GUID_SIZE] = {1};
  const char authkey[SUPLA_AUTHKEY_SIZE] = {2};
  ASSERT_TRUE(config->setGUID(guid));
  ASSERT_TRUE(config->setAuthKey(authkey));
  ASSERT_TRUE(config->setSuplaServer("supla.example.org"));
  ASSERT_TRUE(config->setEmail("user@supla.org"));
}

}  // namespace

TEST_F(SuplaDeviceSupletStartupTests,
       BeginRestoresStoredSupletElementsBeforeInit) {
  ConfigSimulator config;
  TimerMock timer;
  SuplaDeviceClass sd;

  setRequiredSuplaConfig(&config);

  auto definition = makeRelayDefinition(1001, "Test virtual relay");
  Supla::Suplet::Registry registry;
  ASSERT_TRUE(registry.add(&definition));

  Supla::Suplet::Manager manager(&config);
  Supla::Suplet::InstanceRecord record = {};
  record.instanceId = 123;
  record.definitionId = definition.definitionId;
  record.definitionVersion = definition.definitionVersion;
  record.subDeviceId = 7;
  record.state = Supla::Suplet::InstanceState::Active;
  ASSERT_TRUE(record.channelMap.add(relayId, 4));
  ASSERT_TRUE(manager.addInstance(record));

  sd.setSupletRuntime(&manager, &registry);

  EXPECT_CALL(timer, initTimers());

  EXPECT_FALSE(sd.begin());
  EXPECT_EQ(sd.getCurrentStatus(), STATUS_MISSING_NETWORK_INTERFACE);

  auto element = Supla::Element::getElementByChannelNumber(4);
  ASSERT_NE(element, nullptr);
  ASSERT_NE(element->getChannel(), nullptr);
  EXPECT_EQ(element->getChannel()->getDefaultFunction(),
            SUPLA_CHANNELFNC_POWERSWITCH);
  EXPECT_EQ(element->getChannel()->getSubDeviceId(), 123);
}

TEST_F(SuplaDeviceSupletStartupTests,
       IterateRefreshesSupletRuntimeAfterServerAssignment) {
  ConfigSimulator config;
  TimerMock timer;
  SuplaDeviceClass sd;

  setRequiredSuplaConfig(&config);

  auto definition = makeRelayDefinition(1002);
  Supla::Suplet::Registry registry;
  ASSERT_TRUE(registry.add(&definition));
  Supla::Suplet::Manager manager(&config);
  Supla::Suplet::ServerConfigHandler handler(&manager, &registry);
  sd.setSupletRuntime(&manager, &registry);
  sd.setSupletServerConfigHandler(&handler);

  EXPECT_CALL(timer, initTimers());
  EXPECT_FALSE(sd.begin());
  EXPECT_EQ(Supla::Element::getElementByChannelNumber(0), nullptr);

  const char assignment[] =
      "{"
      "\"instanceId\":124,"
      "\"definitionId\":1002,"
      "\"definitionVersion\":1"
      "}";
  Supla::Suplet::ChannelAllocator occupied;
  ASSERT_EQ(handler.applyAssignmentJson(assignment, 1002, 1, occupied),
            Supla::Suplet::ServerConfigResult::Applied);

  sd.iterate();

  auto element = Supla::Element::getElementByChannelNumber(0);
  ASSERT_NE(element, nullptr);
  ASSERT_NE(element->getChannel(), nullptr);
  EXPECT_EQ(element->getChannel()->getDefaultFunction(),
            SUPLA_CHANNELFNC_POWERSWITCH);
  EXPECT_FALSE(handler.isRuntimeRefreshRequired());
}

TEST_F(SuplaDeviceSupletStartupTests,
       AppliesSupletCommandThroughDeviceUsingOccupiedChannels) {
  ConfigSimulator config;
  TimerMock timer;
  SuplaDeviceClass sd;
  Supla::ChannelElement regularElement(0);

  setRequiredSuplaConfig(&config);

  auto definition = makeRelayDefinition(1003);
  Supla::Suplet::Registry registry;
  ASSERT_TRUE(registry.add(&definition));
  Supla::Suplet::Manager manager(&config);
  Supla::Suplet::ServerConfigHandler handler(&manager, &registry);
  sd.setSupletRuntime(&manager, &registry);
  sd.setSupletServerConfigHandler(&handler);

  EXPECT_CALL(timer, initTimers());
  EXPECT_FALSE(sd.begin());

  const char command[] =
      "{"
      "\"op\":\"upsert\","
      "\"instanceId\":125,"
      "\"definitionId\":1003,"
      "\"definitionVersion\":1"
      "}";
  ASSERT_EQ(sd.validateSupletCommandJson(command),
            Supla::Suplet::ServerConfigResult::Applied);
  EXPECT_EQ(manager.getInstanceTable()->findByInstanceId(125), nullptr);

  ASSERT_EQ(sd.applySupletCommandJson(command),
            Supla::Suplet::ServerConfigResult::Applied);

  auto record = manager.getInstanceTable()->findByInstanceId(125);
  ASSERT_NE(record, nullptr);
  EXPECT_EQ(record->channelMap.getChannelNumber(relayId),
            Supla::Suplet::kInvalidChannelNumber);

  sd.iterate();

  EXPECT_EQ(Supla::Element::getElementByChannelNumber(0), &regularElement);
  auto supletElement = Supla::Element::getElementByChannelNumber(1);
  ASSERT_NE(supletElement, nullptr);
  EXPECT_NE(supletElement, &regularElement);
  ASSERT_NE(supletElement->getChannel(), nullptr);
  EXPECT_EQ(supletElement->getChannel()->getDefaultFunction(),
            SUPLA_CHANNELFNC_POWERSWITCH);

  record = manager.getInstanceTable()->findByInstanceId(125);
  ASSERT_NE(record, nullptr);
  EXPECT_EQ(record->channelMap.getChannelNumber(relayId), 1);
}

TEST_F(SuplaDeviceSupletStartupTests,
       RuntimeRefreshRewritesInvalidStateStorageLayout) {
  ConfigSimulator config;
  StorageMockSimulator storage;
  EXPECT_CALL(storage, commit()).Times(::testing::AnyNumber());
  storage.enableChannelNumbers();
  TimerMock timer;
  SuplaDeviceClass sd;
  Supla::ChannelElement regularElement(0);

  setRequiredSuplaConfig(&config);

  auto definition = makeRelayDefinition(1005);
  Supla::Suplet::Registry registry;
  ASSERT_TRUE(registry.add(&definition));
  Supla::Suplet::Manager manager(&config);
  Supla::Suplet::ServerConfigHandler handler(&manager, &registry);
  sd.setSupletRuntime(&manager, &registry);
  sd.setSupletServerConfigHandler(&handler);

  EXPECT_CALL(timer, initTimers());
  EXPECT_FALSE(sd.begin());

  ASSERT_FALSE(Supla::Storage::IsStateStorageValid());
  Supla::Storage::WriteStateStorage();
  ASSERT_TRUE(Supla::Storage::IsStateStorageValid());

  const char command[] =
      "{"
      "\"op\":\"upsert\","
      "\"instanceId\":127,"
      "\"definitionId\":1005,"
      "\"definitionVersion\":1"
      "}";
  ASSERT_EQ(sd.applySupletCommandJson(command),
            Supla::Suplet::ServerConfigResult::Applied);
  ASSERT_TRUE(Supla::Storage::IsStateStorageValid());

  sd.iterate();

  EXPECT_EQ(Supla::Element::getElementByChannelNumber(0), &regularElement);
  auto supletElement = Supla::Element::getElementByChannelNumber(1);
  ASSERT_NE(supletElement, nullptr);
  ASSERT_NE(supletElement->getChannel(), nullptr);
  EXPECT_EQ(supletElement->getChannel()->getDefaultFunction(),
            SUPLA_CHANNELFNC_POWERSWITCH);
  EXPECT_TRUE(Supla::Storage::IsStateStorageValid());
}

TEST_F(SuplaDeviceSupletStartupTests,
       RestoresSupletChannelMappingAfterDeviceRestart) {
  ConfigSimulator config;
  TimerMock timer;
  setRequiredSuplaConfig(&config);

  auto definition = makeRelayDefinition(1004);

  {
    SuplaDeviceClass sd;
    Supla::ChannelElement regularElement(0);
    Supla::Suplet::Registry registry;
    ASSERT_TRUE(registry.add(&definition));
    Supla::Suplet::Manager manager(&config);
    Supla::Suplet::ServerConfigHandler handler(&manager, &registry);
    sd.setSupletRuntime(&manager, &registry);
    sd.setSupletServerConfigHandler(&handler);

    EXPECT_CALL(timer, initTimers());
    EXPECT_FALSE(sd.begin());

    const char command[] =
        "{"
        "\"op\":\"upsert\","
        "\"instanceId\":126,"
        "\"definitionId\":1004,"
        "\"definitionVersion\":1"
        "}";
    ASSERT_EQ(sd.applySupletCommandJson(command),
              Supla::Suplet::ServerConfigResult::Applied);
    sd.iterate();

    EXPECT_EQ(Supla::Element::getElementByChannelNumber(0), &regularElement);
    auto supletElement = Supla::Element::getElementByChannelNumber(1);
    ASSERT_NE(supletElement, nullptr);
    auto record = manager.getInstanceTable()->findByInstanceId(126);
    ASSERT_NE(record, nullptr);
    EXPECT_EQ(record->channelMap.getChannelNumber(relayId), 1);
  }

  {
    SuplaDeviceClass restartedSd;
    Supla::ChannelElement regularElement(0);
    Supla::Suplet::Registry restartedRegistry;
    ASSERT_TRUE(restartedRegistry.add(&definition));
    Supla::Suplet::Manager restartedManager(&config);
    restartedSd.setSupletRuntime(&restartedManager, &restartedRegistry);

    EXPECT_CALL(timer, initTimers());
    EXPECT_FALSE(restartedSd.begin());

    EXPECT_EQ(Supla::Element::getElementByChannelNumber(0), &regularElement);
    auto restoredSuplet = Supla::Element::getElementByChannelNumber(1);
    ASSERT_NE(restoredSuplet, nullptr);
    ASSERT_NE(restoredSuplet->getChannel(), nullptr);
    EXPECT_EQ(restoredSuplet->getChannel()->getDefaultFunction(),
              SUPLA_CHANNELFNC_POWERSWITCH);

    auto record = restartedManager.getInstanceTable()->findByInstanceId(126);
    ASSERT_NE(record, nullptr);
    EXPECT_EQ(record->channelMap.getChannelNumber(relayId), 1);
  }
}

TEST_F(SuplaDeviceSupletStartupTests,
       SaveDefinitionRefreshesRuntimeForStoredInstance) {
  ConfigSimulator config;
  TimerMock timer;
  SuplaDeviceClass sd;

  setRequiredSuplaConfig(&config);

  Supla::Suplet::Manager manager(&config);
  Supla::Suplet::InstanceRecord record = {};
  record.instanceId = 130;
  record.definitionId = 5001;
  record.definitionVersion = 1;
  record.subDeviceId = 9;
  record.state = Supla::Suplet::InstanceState::Active;
  ASSERT_TRUE(record.channelMap.add(relayId, 3));
  ASSERT_TRUE(manager.addInstance(record));

  Supla::Suplet::Registry registry;
  FakeSha256Provider shaProvider;
  Supla::Suplet::DefinitionCache cache(&config, &shaProvider);
  Supla::Suplet::DownloadedDefinitionStore downloadedDefinitions;
  Supla::Suplet::ServerConfigHandler handler(
      &manager, &registry, &cache, &downloadedDefinitions);
  sd.setSupletRuntime(&manager, &registry);
  sd.setSupletServerConfigHandler(&handler);

  EXPECT_CALL(timer, initTimers());
  EXPECT_FALSE(sd.begin());
  EXPECT_EQ(Supla::Element::getElementByChannelNumber(3), nullptr);

  const char definitionJson[] =
      "{"
      "\"schemaVersion\":1,"
      "\"handlerVersion\":1,"
      "\"definitionId\":5001,"
      "\"definitionVersion\":1,"
      "\"category\":\"virtual\","
      "\"kind\":\"virtualRelay\","
      "\"channels\":[{"
      "\"channelId\":1,"
      "\"key\":\"relay\","
      "\"kind\":\"virtualRelay\","
      "\"function\":\"powerSwitch\","
      "\"caption\":\"Late relay\""
      "}]"
      "}";
  uint8_t sha[32] = {};
  makeSha(&shaProvider, definitionJson, sha);

  EXPECT_EQ(handler.saveDownloadedDefinition(5001, 1, definitionJson, sha),
            Supla::Suplet::ServerConfigResult::Applied);
  EXPECT_TRUE(handler.isRuntimeRefreshRequired());

  sd.iterate();

  auto element = Supla::Element::getElementByChannelNumber(3);
  ASSERT_NE(element, nullptr);
  ASSERT_NE(element->getChannel(), nullptr);
  EXPECT_EQ(element->getChannel()->getDefaultFunction(),
            SUPLA_CHANNELFNC_POWERSWITCH);
  EXPECT_STREQ(element->getChannel()->getInitialCaption(), "Late relay");
  EXPECT_FALSE(handler.isRuntimeRefreshRequired());
}

TEST_F(SuplaDeviceSupletStartupTests,
       CalcfgTransfersDownloadedDefinitionInChunks) {
  ConfigSimulator config;
  TimerMock timer;
  SuplaDeviceClass sd;

  setRequiredSuplaConfig(&config);

  Supla::Suplet::Manager manager(&config);
  Supla::Suplet::Registry registry;
  DeviceTestShaProvider shaProvider;
  Supla::Suplet::DefinitionCache cache(&config, &shaProvider);
  Supla::Suplet::DownloadedDefinitionStore downloadedDefinitions;
  Supla::Suplet::ServerConfigHandler handler(
      &manager, &registry, &cache, &downloadedDefinitions);
  sd.setSupletRuntime(&manager, &registry);
  sd.setSupletServerConfigHandler(&handler);

  EXPECT_CALL(timer, initTimers());
  EXPECT_FALSE(sd.begin());

  const char definitionJson[] =
      "{"
      "\"schemaVersion\":1,"
      "\"handlerVersion\":1,"
      "\"definitionId\":5002,"
      "\"definitionVersion\":1,"
      "\"category\":\"virtual\","
      "\"kind\":\"virtualRelay\","
      "\"maxInstances\":2,"
      "\"channels\":[{"
      "\"channelId\":1,"
      "\"key\":\"relay\","
      "\"kind\":\"virtualRelay\","
      "\"function\":\"powerSwitch\","
      "\"caption\":\"CALCFG relay\""
      "}]"
      "}";
  uint8_t sha[32] = {};
  makeDeviceTestSha(definitionJson, strlen(definitionJson), sha);

  TSD_DeviceCalCfgRequest request = {};
  TDS_DeviceCalCfgResult result = {};
  request.ChannelNumber = -1;
  request.SuperUserAuthorized = 1;
  request.Command = SUPLA_CALCFG_CMD_SUPLET_DEFINITION_BEGIN;
  TCalCfg_SupletDefinitionBegin begin = {};
  begin.SessionId = 2345;
  begin.DefinitionId = 5002;
  begin.DefinitionVersion = 1;
  begin.JsonSize = strlen(definitionJson);
  memcpy(begin.JsonSha256, sha, sizeof(sha));
  request.DataSize = sizeof(begin);
  memcpy(request.Data, &begin, sizeof(begin));
  EXPECT_EQ(sd.handleCalcfgFromServer(&request, &result),
            SUPLA_CALCFG_RESULT_DONE);

  const uint16_t jsonSize = strlen(definitionJson);
  uint16_t offset = 0;
  while (offset < jsonSize) {
    const uint8_t size =
        jsonSize - offset > 53 ? 53 : static_cast<uint8_t>(jsonSize - offset);
    request = {};
    result = {};
    request.ChannelNumber = -1;
    request.SuperUserAuthorized = 1;
    request.Command = SUPLA_CALCFG_CMD_SUPLET_DEFINITION_CHUNK;
    TCalCfg_SupletDefinitionChunk chunk = {};
    chunk.SessionId = begin.SessionId;
    chunk.Offset = offset;
    chunk.Size = size;
    memcpy(chunk.Data, definitionJson + offset, size);
    request.DataSize = offsetof(TCalCfg_SupletDefinitionChunk, Data) + size;
    memcpy(request.Data, &chunk, request.DataSize);
    EXPECT_EQ(sd.handleCalcfgFromServer(&request, &result),
              SUPLA_CALCFG_RESULT_DONE);
    offset += size;
  }

  request = {};
  result = {};
  request.ChannelNumber = -1;
  request.SuperUserAuthorized = 1;
  request.Command = SUPLA_CALCFG_CMD_SUPLET_DEFINITION_COMMIT;
  TCalCfg_SupletSessionRequest commit = {};
  commit.SessionId = begin.SessionId;
  request.DataSize = sizeof(commit);
  memcpy(request.Data, &commit, sizeof(commit));
  EXPECT_EQ(sd.handleCalcfgFromServer(&request, &result),
            SUPLA_CALCFG_RESULT_DONE);
  ASSERT_EQ(result.DataSize, sizeof(TCalCfg_SupletResult));
  TCalCfg_SupletResult supletResult = {};
  memcpy(&supletResult, result.Data, sizeof(supletResult));
  EXPECT_EQ(supletResult.DetailCode, SUPLA_CALCFG_SUPLET_RESULT_OK);
  EXPECT_EQ(registry.findDefinition(5002, 1), nullptr);
  Supla::Suplet::JsonDefinition loadedDefinition;
  ASSERT_TRUE(downloadedDefinitions.load(cache, 5002, 1, &loadedDefinition));
  ASSERT_NE(loadedDefinition.getDefinition(), nullptr);
  EXPECT_EQ(loadedDefinition.getDefinition()->definitionId, 5002u);
  EXPECT_EQ(loadedDefinition.getDefinition()->definitionVersion, 1);
  EXPECT_TRUE(handler.isRuntimeRefreshRequired());

  const char command[] =
      "{"
      "\"op\":\"upsert\","
      "\"instanceId\":131,"
      "\"definitionId\":5002,"
      "\"definitionVersion\":1"
      "}";
  ASSERT_EQ(sd.applySupletCommandJson(command),
            Supla::Suplet::ServerConfigResult::Applied);

  sd.iterate();

  auto element = Supla::Element::getElementByChannelNumber(0);
  ASSERT_NE(element, nullptr);
  ASSERT_NE(element->getChannel(), nullptr);
  EXPECT_EQ(element->getChannel()->getDefaultFunction(),
            SUPLA_CHANNELFNC_POWERSWITCH);
  EXPECT_STREQ(element->getChannel()->getInitialCaption(), "CALCFG relay");
}

TEST_F(SuplaDeviceSupletStartupTests,
       CalcfgCapabilitiesUseSupportedHandlerRegistry) {
  ConfigSimulator config;
  SuplaDeviceClass sd;

  auto definition = makeRelayDefinition(8001);
  Supla::Suplet::Registry registry;
  ASSERT_TRUE(registry.add(&definition, 4));
  Supla::Suplet::CapabilityRegistry capabilities;
  Supla::Suplet::Capability capability = {};
  capability.category = Supla::Suplet::Category::Aggregate;
  capability.kind = Supla::Suplet::Kind::ThermometerGroup;
  capability.minSchemaVersion = 1;
  capability.maxSchemaVersion = 1;
  capability.handlerVersion = 1;
  capability.maxInstances = 8;
  capability.supportsDownloadedDefinition = 1;
  ASSERT_TRUE(capabilities.add(capability));
  Supla::Suplet::Manager manager(&config);
  Supla::Suplet::ServerConfigHandler handler(&manager, &registry);
  sd.setSupletRuntime(&manager, &registry);
  sd.setSupletCapabilityRegistry(&capabilities);
  sd.setSupletServerConfigHandler(&handler);

  TSD_DeviceCalCfgRequest request = {};
  TDS_DeviceCalCfgResult result = {};
  request.ChannelNumber = -1;
  request.SuperUserAuthorized = 1;
  request.Command = SUPLA_CALCFG_CMD_SUPLET_GET_CAPABILITIES;

  EXPECT_EQ(sd.handleCalcfgFromServer(&request, &result),
            SUPLA_CALCFG_RESULT_TRUE);
  ASSERT_EQ(result.DataSize, sizeof(TCalCfg_SupletCapabilityList));
  TCalCfg_SupletCapabilityList output = {};
  memcpy(&output, result.Data, sizeof(output));
  ASSERT_EQ(output.Count, 1);
  EXPECT_EQ(output.Total, 1);
  EXPECT_EQ(output.Items[0].Category,
            static_cast<uint8_t>(Supla::Suplet::Category::Aggregate));
  EXPECT_EQ(output.Items[0].Kind,
            static_cast<uint8_t>(Supla::Suplet::Kind::ThermometerGroup));
  EXPECT_EQ(output.Items[0].DefinitionId, 0u);
  EXPECT_EQ(output.Items[0].MinDefinitionVersion, 0u);
  EXPECT_EQ(output.Items[0].MaxDefinitionVersion, 0u);
}

TEST_F(SuplaDeviceSupletStartupTests,
       CalcfgDefinitionListReportsCachedDefinitions) {
  ConfigSimulator config;
  SuplaDeviceClass sd;

  auto runtimeDefinition = makeRelayDefinition(1000);
  Supla::Suplet::Registry registry;
  ASSERT_TRUE(registry.add(&runtimeDefinition));
  Supla::Suplet::CapabilityRegistry capabilities;
  Supla::Suplet::Capability capability = {};
  capability.category = Supla::Suplet::Category::Virtual;
  capability.kind = Supla::Suplet::Kind::VirtualRelay;
  capability.minSchemaVersion = 1;
  capability.maxSchemaVersion = 1;
  capability.handlerVersion = 1;
  capability.maxInstances = 4;
  capability.supportsDownloadedDefinition = 1;
  ASSERT_TRUE(capabilities.add(capability));
  Supla::Suplet::Manager manager(&config);
  DeviceTestShaProvider shaProvider;
  Supla::Suplet::DefinitionCache cache(&config, &shaProvider);
  Supla::Suplet::DownloadedDefinitionStore downloadedDefinitions;
  Supla::Suplet::ServerConfigHandler handler(
      &manager, &registry, &cache, &downloadedDefinitions);
  sd.setSupletRuntime(&manager, &registry);
  sd.setSupletCapabilityRegistry(&capabilities);
  sd.setSupletServerConfigHandler(&handler);

  const char definitionJson[] =
      "{"
      "\"schemaVersion\":1,"
      "\"handlerVersion\":1,"
      "\"definitionId\":8002,"
      "\"definitionVersion\":1,"
      "\"maxInstances\":3,"
      "\"category\":\"virtual\","
      "\"kind\":\"virtualRelay\","
      "\"channels\":[{"
      "\"channelId\":1,"
      "\"key\":\"relay\","
      "\"kind\":\"virtualRelay\","
      "\"function\":\"powerSwitch\","
      "\"caption\":\"Cached relay\""
      "}]"
      "}";

  TDS_DeviceCalCfgResult result = {};
  ASSERT_EQ(sendSupletDefinitionCalcfg(&sd, 8002, 1, definitionJson, &result),
            SUPLA_CALCFG_RESULT_DONE);

  TSD_DeviceCalCfgRequest request = {};
  result = {};
  request.ChannelNumber = -1;
  request.SuperUserAuthorized = 1;
  request.Command = SUPLA_CALCFG_CMD_SUPLET_GET_DEFINITION_LIST;

  EXPECT_EQ(sd.handleCalcfgFromServer(&request, &result),
            SUPLA_CALCFG_RESULT_TRUE);
  ASSERT_EQ(result.DataSize, sizeof(TCalCfg_SupletDefinitionList));
  TCalCfg_SupletDefinitionList output = {};
  memcpy(&output, result.Data, sizeof(output));
  ASSERT_EQ(output.Count, 1);
  EXPECT_EQ(output.Total, 1);
  EXPECT_EQ(output.Items[0].DefinitionId, 8002u);
  EXPECT_EQ(output.Items[0].DefinitionVersion, 1);
  EXPECT_EQ(output.Items[0].JsonSize, strlen(definitionJson));
  EXPECT_EQ(output.Items[0].Category,
            static_cast<uint8_t>(Supla::Suplet::Category::Virtual));
  EXPECT_EQ(output.Items[0].Kind,
            static_cast<uint8_t>(Supla::Suplet::Kind::VirtualRelay));
  EXPECT_EQ(output.Items[0].SchemaVersion, 1);
  EXPECT_EQ(output.Items[0].HandlerVersion, 1);
  EXPECT_EQ(output.Items[0].MaxInstances, 3);
}

TEST_F(SuplaDeviceSupletStartupTests,
       CalcfgDefinitionCommitReportsInvalidDefinitionOnEnvelopeMismatch) {
  ConfigSimulator config;
  TimerMock timer;
  SuplaDeviceClass sd;

  setRequiredSuplaConfig(&config);

  Supla::Suplet::Manager manager(&config);
  Supla::Suplet::Registry registry;
  DeviceTestShaProvider shaProvider;
  Supla::Suplet::DefinitionCache cache(&config, &shaProvider);
  Supla::Suplet::DownloadedDefinitionStore downloadedDefinitions;
  Supla::Suplet::ServerConfigHandler handler(
      &manager, &registry, &cache, &downloadedDefinitions);
  sd.setSupletRuntime(&manager, &registry);
  sd.setSupletServerConfigHandler(&handler);

  EXPECT_CALL(timer, initTimers());
  EXPECT_FALSE(sd.begin());

  const char definitionJson[] =
      "{"
      "\"schemaVersion\":1,"
      "\"handlerVersion\":1,"
      "\"definitionId\":5003,"
      "\"definitionVersion\":1,"
      "\"category\":\"virtual\","
      "\"kind\":\"virtualRelay\","
      "\"channels\":[{"
      "\"channelId\":1,"
      "\"key\":\"relay\","
      "\"kind\":\"virtualRelay\","
      "\"function\":\"powerSwitch\","
      "\"caption\":\"Wrong version\""
      "}]"
      "}";

  TDS_DeviceCalCfgResult result = {};
  EXPECT_EQ(sendSupletDefinitionCalcfg(&sd, 5003, 2, definitionJson, &result),
            SUPLA_CALCFG_RESULT_FALSE);
  ASSERT_EQ(result.DataSize, sizeof(TCalCfg_SupletResult));
  TCalCfg_SupletResult supletResult = {};
  memcpy(&supletResult, result.Data, sizeof(supletResult));
  EXPECT_EQ(supletResult.DetailCode,
            SUPLA_CALCFG_SUPLET_RESULT_INVALID_DEFINITION);
  EXPECT_EQ(registry.findDefinition(5003, 1), nullptr);
  EXPECT_EQ(registry.findDefinition(5003, 2), nullptr);
}

TEST_F(SuplaDeviceSupletStartupTests,
       CalcfgWritesAndReadsPrivateInstanceParams) {
  ConfigSimulator config;
  SuplaDeviceClass sd;

  auto definition = makeParameterizedRelayDefinition(6001);
  Supla::Suplet::Registry registry;
  ASSERT_TRUE(registry.add(&definition, 4));
  Supla::Suplet::Manager manager(&config);
  Supla::Suplet::ServerConfigHandler handler(&manager, &registry);
  sd.setSupletRuntime(&manager, &registry);
  sd.setSupletServerConfigHandler(&handler);

  const char params[] = "{\"relay.count\":1,\"host\":\"192.168.1.50\"}";
  uint8_t sha[32] = {};
  makeDeviceTestSha(params, strlen(params), sha);

  TSD_DeviceCalCfgRequest request = {};
  TDS_DeviceCalCfgResult result = {};
  request.ChannelNumber = -1;
  request.SuperUserAuthorized = 1;
  request.Command = SUPLA_CALCFG_CMD_SUPLET_INSTANCE_BEGIN;
  TCalCfg_SupletInstanceBegin begin = {};
  begin.SessionId = 1234;
  begin.InstanceId = 91;
  begin.DefinitionId = definition.definitionId;
  begin.DefinitionVersion = definition.definitionVersion;
  begin.ParamsSize = strlen(params);
  begin.State = SUPLA_CALCFG_SUPLET_INSTANCE_STATE_ACTIVE;
  memcpy(begin.ParamsSha256, sha, sizeof(sha));
  request.DataSize = sizeof(begin);
  memcpy(request.Data, &begin, sizeof(begin));
  EXPECT_EQ(sd.handleCalcfgFromServer(&request, &result),
            SUPLA_CALCFG_RESULT_DONE);

  request = {};
  result = {};
  request.ChannelNumber = -1;
  request.SuperUserAuthorized = 1;
  request.Command = SUPLA_CALCFG_CMD_SUPLET_INSTANCE_CHUNK;
  TCalCfg_SupletInstanceChunk chunk = {};
  chunk.SessionId = begin.SessionId;
  chunk.Offset = 0;
  chunk.Size = strlen(params);
  memcpy(chunk.Data, params, chunk.Size);
  request.DataSize = offsetof(TCalCfg_SupletInstanceChunk, Data) + chunk.Size;
  memcpy(request.Data, &chunk, request.DataSize);
  EXPECT_EQ(sd.handleCalcfgFromServer(&request, &result),
            SUPLA_CALCFG_RESULT_DONE);

  request = {};
  result = {};
  request.ChannelNumber = -1;
  request.SuperUserAuthorized = 1;
  request.Command = SUPLA_CALCFG_CMD_SUPLET_INSTANCE_COMMIT;
  TCalCfg_SupletSessionRequest commit = {};
  commit.SessionId = begin.SessionId;
  request.DataSize = sizeof(commit);
  memcpy(request.Data, &commit, sizeof(commit));
  EXPECT_EQ(sd.handleCalcfgFromServer(&request, &result),
            SUPLA_CALCFG_RESULT_DONE);
  EXPECT_EQ(result.DataSize, sizeof(TCalCfg_SupletResult));

  auto record = manager.getInstanceTable()->findByInstanceId(begin.InstanceId);
  ASSERT_NE(record, nullptr);
  EXPECT_EQ(record->configSize, strlen(params));
  EXPECT_EQ(memcmp(record->config, params, strlen(params)), 0);

  request = {};
  result = {};
  request.ChannelNumber = -1;
  request.SuperUserAuthorized = 1;
  request.Command = SUPLA_CALCFG_CMD_SUPLET_GET_INSTANCE_COUNT;
  EXPECT_EQ(sd.handleCalcfgFromServer(&request, &result),
            SUPLA_CALCFG_RESULT_TRUE);
  ASSERT_EQ(result.DataSize, sizeof(TCalCfg_SupletInstanceCount));
  TCalCfg_SupletInstanceCount count = {};
  memcpy(&count, result.Data, sizeof(count));
  EXPECT_EQ(count.Count, 1);

  request = {};
  result = {};
  request.ChannelNumber = -1;
  request.SuperUserAuthorized = 1;
  request.Command = SUPLA_CALCFG_CMD_SUPLET_GET_INSTANCE_LIST;
  TCalCfg_SupletListRequest listRequest = {};
  listRequest.Limit = 5;
  request.DataSize = sizeof(listRequest);
  memcpy(request.Data, &listRequest, sizeof(listRequest));
  EXPECT_EQ(sd.handleCalcfgFromServer(&request, &result),
            SUPLA_CALCFG_RESULT_TRUE);
  ASSERT_EQ(result.DataSize, sizeof(TCalCfg_SupletInstanceList));
  TCalCfg_SupletInstanceList list = {};
  memcpy(&list, result.Data, sizeof(list));
  ASSERT_EQ(list.Count, 1);
  EXPECT_EQ(list.Items[0].InstanceId, begin.InstanceId);
  EXPECT_EQ(list.Items[0].DefinitionId, definition.definitionId);

  request = {};
  result = {};
  request.ChannelNumber = -1;
  request.SuperUserAuthorized = 1;
  request.Command = SUPLA_CALCFG_CMD_SUPLET_GET_INSTANCE_INFO;
  TCalCfg_SupletInstanceRequest infoRequest = {};
  infoRequest.InstanceId = begin.InstanceId;
  request.DataSize = sizeof(infoRequest);
  memcpy(request.Data, &infoRequest, sizeof(infoRequest));
  EXPECT_EQ(sd.handleCalcfgFromServer(&request, &result),
            SUPLA_CALCFG_RESULT_TRUE);
  ASSERT_EQ(result.DataSize, sizeof(TCalCfg_SupletInstanceInfo));
  TCalCfg_SupletInstanceInfo info = {};
  memcpy(&info, result.Data, sizeof(info));
  EXPECT_EQ(info.InstanceId, begin.InstanceId);
  EXPECT_EQ(info.DefinitionId, definition.definitionId);
  EXPECT_EQ(info.ParamsSize, strlen(params));
  EXPECT_EQ(memcmp(info.ParamsSha256, sha, sizeof(sha)), 0);

  request = {};
  result = {};
  request.ChannelNumber = -1;
  request.SuperUserAuthorized = 1;
  request.Command = SUPLA_CALCFG_CMD_SUPLET_GET_INSTANCE_CONFIG;
  TCalCfg_SupletInstanceConfigRequest configRequest = {};
  configRequest.InstanceId = begin.InstanceId;
  configRequest.MaxSize = 20;
  request.DataSize = sizeof(configRequest);
  memcpy(request.Data, &configRequest, sizeof(configRequest));
  EXPECT_EQ(sd.handleCalcfgFromServer(&request, &result),
            SUPLA_CALCFG_RESULT_TRUE);
  TCalCfg_SupletInstanceConfigChunk configChunk = {};
  memcpy(&configChunk, result.Data, result.DataSize);
  EXPECT_EQ(configChunk.InstanceId, begin.InstanceId);
  EXPECT_EQ(configChunk.TotalSize, strlen(params));
  EXPECT_EQ(configChunk.Size, 20);
  EXPECT_EQ(memcmp(configChunk.Data, params, configChunk.Size), 0);
}

TEST_F(SuplaDeviceSupletStartupTests,
       CalcfgAcceptsMaxSizePrivateInstanceParams) {
  ConfigSimulator config;
  SuplaDeviceClass sd;

  auto definition = makeParameterizedRelayDefinition(6002);
  Supla::Suplet::Registry registry;
  ASSERT_TRUE(registry.add(&definition, 4));
  Supla::Suplet::Manager manager(&config);
  Supla::Suplet::ServerConfigHandler handler(&manager, &registry);
  sd.setSupletRuntime(&manager, &registry);
  sd.setSupletServerConfigHandler(&handler);

  const std::string prefix = "{\"relay.count\":1,\"host\":\"";
  const std::string suffix = "\"}";
  ASSERT_LT(prefix.size() + suffix.size(),
            static_cast<size_t>(SUPLA_SUPLET_MAX_CONFIG_SIZE));
  std::string params = prefix +
      std::string(SUPLA_SUPLET_MAX_CONFIG_SIZE -
                      prefix.size() -
                      suffix.size(),
                  'x') +
      suffix;
  ASSERT_EQ(params.size(),
            static_cast<size_t>(SUPLA_SUPLET_MAX_CONFIG_SIZE));

  TDS_DeviceCalCfgResult result = {};
  EXPECT_EQ(sendSupletInstanceParamsCalcfg(
                &sd,
                92,
                definition.definitionId,
                definition.definitionVersion,
                params.c_str(),
                params.size(),
                &result),
            SUPLA_CALCFG_RESULT_DONE);

  auto record = manager.getInstanceTable()->findByInstanceId(92);
  ASSERT_NE(record, nullptr);
  EXPECT_EQ(record->configSize, SUPLA_SUPLET_MAX_CONFIG_SIZE);
  EXPECT_EQ(memcmp(record->config, params.data(), params.size()), 0);
}

TEST_F(SuplaDeviceSupletStartupTests,
       CalcfgInstanceIdZeroAllocatesFirstFreeSlot) {
  ConfigSimulator config;
  SuplaDeviceClass sd;

  auto definition = makeParameterizedRelayDefinition(6007);
  Supla::Suplet::Registry registry;
  ASSERT_TRUE(registry.add(&definition, 4));
  Supla::Suplet::Manager manager(&config);
  Supla::Suplet::ServerConfigHandler handler(&manager, &registry);
  sd.setSupletRuntime(&manager, &registry);
  sd.setSupletServerConfigHandler(&handler);

  Supla::Channel existingChannel;
  existingChannel.setSubDeviceId(1);

  const char params[] = "{\"relay.count\":1,\"host\":\"192.168.1.50\"}";
  TDS_DeviceCalCfgResult result = {};
  EXPECT_EQ(sendSupletInstanceParamsCalcfg(&sd,
                                           0,
                                           definition.definitionId,
                                           definition.definitionVersion,
                                           params,
                                           strlen(params),
                                           &result),
            SUPLA_CALCFG_RESULT_DONE);
  ASSERT_EQ(result.DataSize, sizeof(TCalCfg_SupletResult));
  TCalCfg_SupletResult supletResult = {};
  memcpy(&supletResult, result.Data, sizeof(supletResult));
  EXPECT_EQ(supletResult.InstanceId, 2);

  EXPECT_EQ(manager.getInstanceTable()->findByInstanceId(0), nullptr);
  auto record = manager.getInstanceTable()->findByInstanceId(2);
  ASSERT_NE(record, nullptr);
  EXPECT_EQ(record->subDeviceId, 2);
  EXPECT_EQ(record->configSize, strlen(params));
}

TEST_F(SuplaDeviceSupletStartupTests,
       CalcfgReportsInstanceLimitWhenMaxInstancesIsExceeded) {
  ConfigSimulator config;
  SuplaDeviceClass sd;

  auto definition = makeParameterizedRelayDefinition(6003);
  Supla::Suplet::Registry registry;
  ASSERT_TRUE(registry.add(&definition, 1));
  Supla::Suplet::Manager manager(&config);
  Supla::Suplet::ServerConfigHandler handler(&manager, &registry);
  sd.setSupletRuntime(&manager, &registry);
  sd.setSupletServerConfigHandler(&handler);

  const char params[] = "{\"relay.count\":1,\"host\":\"192.168.1.50\"}";
  TDS_DeviceCalCfgResult result = {};
  ASSERT_EQ(sendSupletInstanceParamsCalcfg(&sd,
                                           93,
                                           definition.definitionId,
                                           definition.definitionVersion,
                                           params,
                                           strlen(params),
                                           &result),
            SUPLA_CALCFG_RESULT_DONE);

  result = {};
  EXPECT_EQ(sendSupletInstanceParamsCalcfg(&sd,
                                           94,
                                           definition.definitionId,
                                           definition.definitionVersion,
                                           params,
                                           strlen(params),
                                           &result),
            SUPLA_CALCFG_RESULT_FALSE);
  ASSERT_EQ(result.DataSize, sizeof(TCalCfg_SupletResult));
  TCalCfg_SupletResult supletResult = {};
  memcpy(&supletResult, result.Data, sizeof(supletResult));
  EXPECT_EQ(supletResult.DetailCode,
            SUPLA_CALCFG_SUPLET_RESULT_INSTANCE_LIMIT_EXCEEDED);
  EXPECT_EQ(manager.getInstanceTable()->findByInstanceId(94), nullptr);
}

TEST_F(SuplaDeviceSupletStartupTests,
       CalcfgReportsChannelLimitWhenNoChannelIsFree) {
  ConfigSimulator config;
  SuplaDeviceClass sd;

  auto definition = makeParameterizedRelayDefinition(6004);
  Supla::Suplet::Registry registry;
  ASSERT_TRUE(registry.add(&definition, 4));
  Supla::Suplet::Manager manager(&config);
  Supla::Suplet::ServerConfigHandler handler(&manager, &registry);
  sd.setSupletRuntime(&manager, &registry);
  sd.setSupletServerConfigHandler(&handler);

  Supla::ChannelElement *occupied[SUPLA_CHANNELMAXCOUNT] = {};
  for (int i = 0; i < SUPLA_CHANNELMAXCOUNT; i++) {
    occupied[i] = new Supla::ChannelElement(i);
  }

  const char params[] = "{\"relay.count\":1,\"host\":\"192.168.1.50\"}";
  TDS_DeviceCalCfgResult result = {};
  EXPECT_EQ(sendSupletInstanceParamsCalcfg(&sd,
                                           95,
                                           definition.definitionId,
                                           definition.definitionVersion,
                                           params,
                                           strlen(params),
                                           &result),
            SUPLA_CALCFG_RESULT_FALSE);
  ASSERT_EQ(result.DataSize, sizeof(TCalCfg_SupletResult));
  TCalCfg_SupletResult supletResult = {};
  memcpy(&supletResult, result.Data, sizeof(supletResult));
  EXPECT_EQ(supletResult.DetailCode,
            SUPLA_CALCFG_SUPLET_RESULT_CHANNEL_LIMIT_EXCEEDED);
  EXPECT_EQ(manager.getInstanceTable()->findByInstanceId(95), nullptr);

  for (int i = 0; i < SUPLA_CHANNELMAXCOUNT; i++) {
    delete occupied[i];
  }
}

/*
 * Copyright (C) AC SOFTWARE SP. Z O.O
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#include <SuplaDevice.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <utility>

#include <nlohmann/json.hpp>

#include "../doubles/mqtt_documentation.h"
#include "../doubles/mqtt_mock.h"

using ::testing::StrEq;
using ::testing::StrictMock;

namespace {

MqttDocumentationScenarioMetadata metadata(
    std::string id = "relay.power_switch.state", int channel = 7) {
  return {std::move(id),
          "Relay state",
          "SUPLA_CHANNELTYPE_RELAY",
          "SUPLA_CHANNELFNC_POWERSWITCH",
          "public",
          channel,
          "custom/supla/devices/test-device"};
}

}  // namespace

TEST(MqttDocumentationTests, CapturesPublishAndSubscribeInsideScenario) {
  MqttDocumentationRecorder recorder;
  {
    MqttDocumentationScenario scenario(recorder, metadata());
    recorder.recordPublish(
        "custom/supla/devices/test-device/channels/7/state/on",
        "true",
        0,
        true);
    recorder.recordSubscribe(
        "custom/supla/devices/test-device/channels/7/set/on", 1);
  }

  const auto &operations =
      recorder.scenarios().at("relay.power_switch.state").operations;
  ASSERT_EQ(2, operations.size());
  EXPECT_EQ(MqttCapturedOperation::Type::Publish, operations[0].type);
  EXPECT_EQ("{prefix}/channels/{channel}/state/on", operations[0].topic);
  EXPECT_EQ("true", operations[0].payload);
  EXPECT_EQ(0, operations[0].qos);
  EXPECT_TRUE(operations[0].retain);
  EXPECT_EQ(MqttCapturedOperation::Type::Subscribe, operations[1].type);
  EXPECT_EQ("{prefix}/channels/{channel}/set/on", operations[1].topic);
  EXPECT_EQ(1, operations[1].qos);
  EXPECT_FALSE(operations[1].retain);
}

TEST(MqttDocumentationTests, IgnoresTrafficOutsideScenario) {
  MqttDocumentationRecorder recorder;
  recorder.recordPublish("unscoped", "payload", 0, false);
  recorder.recordSubscribe("unscoped", 0);
  EXPECT_TRUE(recorder.scenarios().empty());
}

TEST(MqttDocumentationTests, ClassifiesEmptyRetainedPublishAsCleanup) {
  MqttDocumentationRecorder recorder;
  {
    MqttDocumentationScenario scenario(recorder, metadata());
    recorder.recordPublish(
        "custom/supla/devices/test-device/channels/7/state/obsolete",
        "",
        0,
        true);
  }

  const auto &operation =
      recorder.scenarios().at("relay.power_switch.state").operations.at(0);
  EXPECT_EQ("cleanup", operation.category);
  EXPECT_EQ("cleanup",
            recorder.toJson()
                .at("scenarios")
                .at(0)
                .at("operations")
                .at(0)
                .at("category"));
}

TEST(MqttDocumentationTests, AssignsOperationsToTheirScenarios) {
  MqttDocumentationRecorder recorder;
  {
    MqttDocumentationScenario scenario(recorder, metadata("first"));
    recorder.recordPublish(
        "custom/supla/devices/test-device/channels/7/state/on",
        "true",
        0,
        true);
  }
  {
    MqttDocumentationScenario scenario(recorder, metadata("second"));
    recorder.recordSubscribe(
        "custom/supla/devices/test-device/channels/7/set/on", 0);
  }

  ASSERT_EQ(1, recorder.scenarios().at("first").operations.size());
  ASSERT_EQ(1, recorder.scenarios().at("second").operations.size());
  EXPECT_EQ(MqttCapturedOperation::Type::Publish,
            recorder.scenarios().at("first").operations[0].type);
  EXPECT_EQ(MqttCapturedOperation::Type::Subscribe,
            recorder.scenarios().at("second").operations[0].type);
}

TEST(MqttDocumentationTests, RejectsNestedScenariosWithoutEndingOuterOne) {
  MqttDocumentationRecorder recorder;
  MqttDocumentationScenario outer(recorder, metadata("outer"));
  EXPECT_THROW(MqttDocumentationScenario(recorder, metadata("inner")),
               std::logic_error);
  EXPECT_TRUE(recorder.hasActiveScenario());
  recorder.recordSubscribe("custom/supla/devices/test-device/channels/7/set/on",
                           0);
  EXPECT_EQ(1, recorder.scenarios().at("outer").operations.size());
}

TEST(MqttDocumentationTests, NormalizesOnlyKnownPrefixChannelAndPhase) {
  MqttDocumentationRecorder recorder;
  {
    MqttDocumentationScenario scenario(recorder, metadata());
    recorder.recordPublish(
        "custom/supla/devices/test-device/channels/7/state/phases/2/voltage",
        "230.00",
        0,
        true);
    recorder.recordPublish(
        "custom/supla/devices/test-device/channels/17/state/number/123",
        "123",
        0,
        true);
  }

  const auto &operations =
      recorder.scenarios().at("relay.power_switch.state").operations;
  EXPECT_EQ("{prefix}/channels/{channel}/state/phases/{phase}/voltage",
            operations[0].topic);
  EXPECT_EQ(2, operations[0].variables.at("phase"));
  EXPECT_EQ("{prefix}/channels/17/state/number/123", operations[1].topic);
  EXPECT_TRUE(operations[1].variables.empty());
}

TEST(MqttDocumentationTests, NormalizesHomeAssistantDiscoveryChannel) {
  MqttDocumentationRecorder recorder;
  auto scenarioMetadata = metadata("home_assistant.hvac", 7);
  scenarioMetadata.category = "home_assistant";
  {
    MqttDocumentationScenario scenario(recorder, scenarioMetadata);
    recorder.recordPublish(
        "homeassistant/climate/supla/device_7_0/config",
        "{}",
        0,
        true);
  }

  const auto &operation =
      recorder.scenarios().at("home_assistant.hvac").operations.at(0);
  EXPECT_EQ("homeassistant/climate/supla/device_{channel}_{sub_id}/config",
            operation.topic);
  EXPECT_EQ(0, operation.variables.at("sub_id"));
}

TEST(MqttDocumentationTests, MixedScenarioKeepsPublicAndCleanupOperations) {
  MqttDocumentationRecorder recorder;
  auto scenarioMetadata = metadata("mixed");
  scenarioMetadata.category = "mixed";
  {
    MqttDocumentationScenario scenario(recorder, scenarioMetadata);
    recorder.recordPublish(
        "custom/supla/devices/test-device/channels/7/state/on",
        "true",
        0,
        true);
    recorder.recordPublish(
        "custom/supla/devices/test-device/channels/7/state/obsolete",
        "",
        0,
        true);
  }

  const auto &operations = recorder.scenarios().at("mixed").operations;
  ASSERT_EQ(2, operations.size());
  EXPECT_EQ("public", operations[0].category);
  EXPECT_EQ("cleanup", operations[1].category);
}

TEST(MqttDocumentationTests,
     JsonOrderingIsDeterministicAndDuplicatesAreRemoved) {
  MqttDocumentationRecorder recorder;
  {
    MqttDocumentationScenario scenario(recorder, metadata("z-scenario"));
    recorder.recordPublish("z-topic", "second", 0, true);
    recorder.recordPublish("a-topic", "first", 0, true);
    recorder.recordPublish("a-topic", "first", 0, true);
  }
  { MqttDocumentationScenario scenario(recorder, metadata("a-scenario")); }

  const auto json = recorder.toJson();
  ASSERT_EQ(2, json.at("scenarios").size());
  EXPECT_EQ("a-scenario", json.at("scenarios").at(0).at("id"));
  EXPECT_EQ("z-scenario", json.at("scenarios").at(1).at("id"));
  const auto &operations = json.at("scenarios").at(1).at("operations");
  ASSERT_EQ(2, operations.size());
  EXPECT_EQ("a-topic", operations.at(0).at("topic"));
  EXPECT_EQ("z-topic", operations.at(1).at("topic"));
  EXPECT_EQ("device_to_broker", operations.at(0).at("direction"));
  EXPECT_EQ(1, json.at("schema_version"));
}

TEST(MqttDocumentationTests, DisabledRecorderDoesNothing) {
  MqttDocumentationRecorder recorder(false);
  {
    MqttDocumentationScenario scenario(recorder, metadata());
    recorder.recordPublish("topic", "payload", 0, true);
  }
  EXPECT_TRUE(recorder.scenarios().empty());
}

TEST(MqttDocumentationTests, AdapterKeepsExistingMockAssertionsActive) {
  SuplaDeviceClass device;
  StrictMock<MqttMock> mqtt(&device);
  MqttDocumentationRecorder recorder;
  mqtt.setDocumentationRecorder(&recorder);

  EXPECT_CALL(
      mqtt,
      publishTest(StrEq("prefix/channels/7/state/on"), StrEq("true"), 0, true));
  EXPECT_CALL(mqtt, subscribeTest(StrEq("prefix/channels/7/set/on"), 1));
  {
    MqttDocumentationScenario scenario(recorder,
                                       {"adapter",
                                        "Adapter",
                                        "SUPLA_CHANNELTYPE_RELAY",
                                        "SUPLA_CHANNELFNC_POWERSWITCH",
                                        "public",
                                        7,
                                        "prefix"});
    mqtt.publishImp("prefix/channels/7/state/on", "true", 0, true);
    mqtt.subscribeImp("prefix/channels/7/set/on", 1);
  }

  EXPECT_EQ(2, recorder.scenarios().at("adapter").operations.size());
}

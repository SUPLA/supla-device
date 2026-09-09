// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>
#include <stddef.h>
#include <string.h>
#include <supla-common/proto.h>
#include <supla-common/proto_suplet.h>
#include <supla/control/weekly_schedule_common.h>
#include <supla/debug/command_processor.h>

#include <string>
#include <vector>
#include <utility>

namespace {

class CapturingWriter : public Supla::Debug::ResponseWriter {
 public:
  void write(const char *text) override {
    response += text == nullptr ? "" : text;
  }

  std::string response;
};

struct ConfigChunk {
  uint16_t totalSize;
  std::string data;
};

class CalcfgChunkResponder {
 public:
  explicit CalcfgChunkResponder(std::vector<ConfigChunk> chunks)
      : chunks(std::move(chunks)) {
  }

  static int handle(void *context,
                    TSD_DeviceCalCfgRequest *request,
                    TDS_DeviceCalCfgResult *result) {
    return static_cast<CalcfgChunkResponder *>(context)->handle(request,
                                                                result);
  }

  int handle(TSD_DeviceCalCfgRequest *request, TDS_DeviceCalCfgResult *result) {
    if (request == nullptr || result == nullptr ||
        currentChunk >= chunks.size()) {
      return SUPLA_CALCFG_RESULT_FALSE;
    }

    const auto &chunk = chunks[currentChunk++];
    if (request->Command == SUPLA_CALCFG_CMD_SUPLET_GET_DEFINITION_CONFIG) {
      TCalCfg_SupletDefinitionConfigRequest configRequest = {};
      memcpy(&configRequest, request->Data, sizeof(configRequest));
      if (configRequest.Offset != expectedOffset) {
        return SUPLA_CALCFG_RESULT_FALSE;
      }
      TCalCfg_SupletDefinitionConfigChunk output = {};
      output.DefinitionId = definitionId;
      output.DefinitionVersion = definitionVersion;
      output.Offset = expectedOffset;
      output.TotalSize = chunk.totalSize;
      output.Source = SUPLA_CALCFG_SUPLET_DEFINITION_SOURCE_BUILTIN;
      output.Size = chunk.data.size();
      memcpy(output.Data, chunk.data.data(), output.Size);
      result->DataSize =
          offsetof(TCalCfg_SupletDefinitionConfigChunk, Data) + output.Size;
      memcpy(result->Data, &output, result->DataSize);
    } else if (request->Command ==
               SUPLA_CALCFG_CMD_SUPLET_GET_INSTANCE_DATA) {
      TCalCfg_SupletInstanceDataRequest configRequest = {};
      memcpy(&configRequest, request->Data, sizeof(configRequest));
      if (configRequest.Offset != expectedOffset) {
        return SUPLA_CALCFG_RESULT_FALSE;
      }
      TCalCfg_SupletInstanceDataChunk output = {};
      output.InstanceId = instanceId;
      output.Part = SUPLA_CALCFG_SUPLET_TRANSFER_PART_CONFIG;
      output.Offset = expectedOffset;
      output.TotalSize = chunk.totalSize;
      output.Size = chunk.data.size();
      memcpy(output.Data, chunk.data.data(), output.Size);
      result->DataSize =
          offsetof(TCalCfg_SupletInstanceDataChunk, Data) + output.Size;
      memcpy(result->Data, &output, result->DataSize);
    } else {
      return SUPLA_CALCFG_RESULT_FALSE;
    }

    expectedOffset += chunk.data.size();
    return SUPLA_CALCFG_RESULT_TRUE;
  }

  static constexpr uint32_t definitionId = 1234;
  static constexpr uint16_t definitionVersion = 2;
  static constexpr uint8_t instanceId = 7;

 private:
  std::vector<ConfigChunk> chunks;
  size_t currentChunk = 0;
  uint16_t expectedOffset = 0;
};

class InstanceBeginResponder {
 public:
  static int handle(void *context,
                    TSD_DeviceCalCfgRequest *request,
                    TDS_DeviceCalCfgResult *) {
    return static_cast<InstanceBeginResponder *>(context)->handle(request);
  }

  int handle(TSD_DeviceCalCfgRequest *request) {
    if (request == nullptr) {
      return SUPLA_CALCFG_RESULT_FALSE;
    }
    if (request->Command == SUPLA_CALCFG_CMD_SUPLET_INSTANCE_BEGIN) {
      memcpy(&begin, request->Data, sizeof(begin));
    }
    return SUPLA_CALCFG_RESULT_DONE;
  }

  TCalCfg_SupletInstanceBegin begin = {};
};

class ChannelValueResponder {
 public:
  static int32_t handle(void *context, TSD_SuplaChannelNewValue *newValue) {
    auto *responder = static_cast<ChannelValueResponder *>(context);
    responder->value = *newValue;
    responder->called = true;
    return responder->result;
  }

  TSD_SuplaChannelNewValue value = {};
  int32_t result = 1;
  bool called = false;
};

class ChannelConfigResponder {
 public:
  static uint8_t handle(void *context,
                        TSD_ChannelConfig *config,
                        bool local) {
    auto *responder = static_cast<ChannelConfigResponder *>(context);
    responder->config = *config;
    responder->local = local;
    responder->called = true;
    return responder->result;
  }

  TSD_ChannelConfig config = {};
  uint8_t result = SUPLA_CONFIG_RESULT_TRUE;
  bool local = false;
  bool called = false;
};

int programAt(const TChannelConfig_WeeklySchedule *schedule,
              Supla::DayOfWeek day,
              int hour,
              int minute) {
  int index = Supla::Control::calculateWeeklyScheduleIndex(
      day, hour, minute / 15);
  return Supla::Control::getWeeklyScheduleProgramId(schedule, index);
}

TEST(CommandProcessorTests, InjectsRawChannelValue) {
  ChannelValueResponder responder;
  Supla::Debug::CommandProcessor processor(
      nullptr, &ChannelValueResponder::handle, &responder);
  CapturingWriter writer;

  EXPECT_TRUE(processor.processLine(
      "{\"op\":\"channelValue\",\"channelNumber\":17,"
      "\"senderId\":1234,\"durationMs\":5678,"
      "\"valueHex\":\"0123456789aBcDeF\"}",
      &writer));

  EXPECT_TRUE(responder.called);
  EXPECT_EQ(responder.value.ChannelNumber, 17);
  EXPECT_EQ(responder.value.SenderID, 1234);
  EXPECT_EQ(responder.value.DurationMS, 5678U);
  const unsigned char expected[] = {
      0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef};
  EXPECT_EQ(memcmp(responder.value.value, expected, sizeof(expected)), 0);
  EXPECT_EQ(writer.response,
            "{\"op\":\"channelValue.result\",\"channelNumber\":17,"
            "\"result\":1,\"ok\":true}\n");
}

TEST(CommandProcessorTests, BuildsRelayModeChannelValue) {
  ChannelValueResponder responder;
  Supla::Debug::CommandProcessor processor(
      nullptr, &ChannelValueResponder::handle, &responder);
  CapturingWriter writer;

  EXPECT_TRUE(processor.processLine(
      "{\"op\":\"channelValue\",\"channelNumber\":3,"
      "\"relayMode\":6}",
      &writer));

  TRelayChannel_Value relayValue = {};
  memcpy(&relayValue, responder.value.value, sizeof(relayValue));
  EXPECT_EQ(relayValue.RelayMode, SUPLA_RELAY_MODE_CMD_WEEKLY_SCHEDULE);
}

TEST(CommandProcessorTests, BuildsRelayStartOnChannelValueWithOnState) {
  ChannelValueResponder responder;
  Supla::Debug::CommandProcessor processor(
      nullptr, &ChannelValueResponder::handle, &responder);
  CapturingWriter writer;

  EXPECT_TRUE(processor.processLine(
      "{\"op\":\"channelValue\",\"channelNumber\":3,"
      "\"relayMode\":1}",
      &writer));

  TRelayChannel_Value relayValue = {};
  memcpy(&relayValue, responder.value.value, sizeof(relayValue));
  EXPECT_EQ(relayValue.hi, 1);
  EXPECT_EQ(relayValue.RelayMode, SUPLA_RELAY_MODE_START_ON);
}

TEST(CommandProcessorTests, BuildsRelayForcedOffChannelValueWithOffState) {
  ChannelValueResponder responder;
  Supla::Debug::CommandProcessor processor(
      nullptr, &ChannelValueResponder::handle, &responder);
  CapturingWriter writer;

  EXPECT_TRUE(processor.processLine(
      "{\"op\":\"channelValue\",\"channelNumber\":3,"
      "\"relayMode\":4}",
      &writer));

  TRelayChannel_Value relayValue = {};
  memcpy(&relayValue, responder.value.value, sizeof(relayValue));
  EXPECT_EQ(relayValue.hi, 0);
  EXPECT_EQ(relayValue.RelayMode, SUPLA_RELAY_MODE_FORCED_OFF);
}

TEST(CommandProcessorTests, BuildsActionTriggerModeChannelValue) {
  ChannelValueResponder responder;
  Supla::Debug::CommandProcessor processor(
      nullptr, &ChannelValueResponder::handle, &responder);
  CapturingWriter writer;

  EXPECT_TRUE(processor.processLine(
      "{\"op\":\"channelValue\",\"channelNumber\":4,"
      "\"buttonMode\":4}",
      &writer));

  TActionTriggerProperties properties = {};
  memcpy(&properties, responder.value.value, sizeof(properties));
  EXPECT_EQ(properties.ButtonMode, SUPLA_BUTTON_MODE_CMD_WEEKLY_SCHEDULE);
}

TEST(CommandProcessorTests, RejectsAmbiguousChannelValuePayload) {
  ChannelValueResponder responder;
  Supla::Debug::CommandProcessor processor(
      nullptr, &ChannelValueResponder::handle, &responder);
  CapturingWriter writer;

  EXPECT_TRUE(processor.processLine(
      "{\"op\":\"channelValue\",\"channelNumber\":4,"
      "\"relayMode\":6,\"buttonMode\":4}",
      &writer));

  EXPECT_FALSE(responder.called);
  EXPECT_EQ(writer.response,
            "{\"ok\":false,\"error\":\"invalid_arguments\"}\n");
}

TEST(CommandProcessorTests, RejectsMalformedRawChannelValue) {
  ChannelValueResponder responder;
  Supla::Debug::CommandProcessor processor(
      nullptr, &ChannelValueResponder::handle, &responder);
  CapturingWriter writer;

  EXPECT_TRUE(processor.processLine(
      "{\"op\":\"channelValue\",\"channelNumber\":4,"
      "\"valueHex\":\"0123456789abcdeg\"}",
      &writer));

  EXPECT_FALSE(responder.called);
  EXPECT_EQ(writer.response,
            "{\"ok\":false,\"error\":\"invalid_value_hex\"}\n");
}

TEST(CommandProcessorTests, InjectsRelayWeeklySchedule) {
  ChannelConfigResponder responder;
  Supla::Debug::CommandProcessor processor(
      nullptr, &ChannelConfigResponder::handle, &responder);
  CapturingWriter writer;

  EXPECT_TRUE(processor.processLine(
      "{\"op\":\"weeklySchedule\",\"channelNumber\":3,"
      "\"programs\":[{\"mode\":\"forced_on\"},"
      "{\"mode\":\"forced_off\",\"value1\":-123,\"value2\":456},"
      "{\"mode\":\"automatic\"}],"
      "\"entries\":[{\"days\":[\"mon\",\"tue\",\"wed\",\"thu\","
      "\"fri\"],\"from\":\"08:00\",\"to\":\"19:00\",\"program\":2},"
      "{\"days\":[\"mon\",\"tue\",\"wed\",\"thu\",\"fri\"],"
      "\"from\":\"19:00\",\"to\":\"21:00\",\"program\":1},"
      "{\"days\":[\"fri\"],\"from\":\"21:00\",\"to\":\"08:00\","
      "\"program\":3}]}",
      &writer));

  ASSERT_TRUE(responder.called);
  EXPECT_TRUE(responder.local);
  EXPECT_EQ(responder.config.ChannelNumber, 3);
  EXPECT_EQ(responder.config.ConfigType, SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE);
  EXPECT_EQ(responder.config.ConfigSize,
            sizeof(TChannelConfig_WeeklySchedule));
  TChannelConfig_WeeklySchedule schedule = {};
  memcpy(&schedule, responder.config.Config, sizeof(schedule));
  EXPECT_EQ(schedule.Program[0].Mode, SUPLA_RELAY_MODE_FORCED_ON);
  EXPECT_EQ(schedule.Program[1].Mode, SUPLA_RELAY_MODE_FORCED_OFF);
  EXPECT_EQ(schedule.Program[1].Value1, -123);
  EXPECT_EQ(schedule.Program[1].Value2, 456);
  EXPECT_EQ(schedule.Program[2].Mode, SUPLA_RELAY_MODE_AUTOMATIC);
  EXPECT_EQ(programAt(&schedule, Supla::DayOfWeek_Monday, 7, 45), 0);
  EXPECT_EQ(programAt(&schedule, Supla::DayOfWeek_Monday, 8, 0), 2);
  EXPECT_EQ(programAt(&schedule, Supla::DayOfWeek_Monday, 19, 0), 1);
  EXPECT_EQ(programAt(&schedule, Supla::DayOfWeek_Monday, 21, 0), 0);
  EXPECT_EQ(programAt(&schedule, Supla::DayOfWeek_Friday, 21, 0), 3);
  EXPECT_EQ(programAt(&schedule, Supla::DayOfWeek_Saturday, 7, 45), 3);
  EXPECT_EQ(programAt(&schedule, Supla::DayOfWeek_Saturday, 8, 0), 0);
  EXPECT_EQ(writer.response,
            "{\"op\":\"weeklySchedule.result\",\"channelNumber\":3,"
            "\"result\":1,\"ok\":true}\n");
}

TEST(CommandProcessorTests,
     WeeklyDurationAcceptsUnsignedRangeAndRejectsAliases) {
  ChannelConfigResponder responder;
  Supla::Debug::CommandProcessor processor(
      nullptr, &ChannelConfigResponder::handle, &responder);
  CapturingWriter writer;
  ASSERT_TRUE(processor.processLine(
      "{\"op\":\"weeklySchedule\",\"channelNumber\":0,"
      "\"programs\":[{\"mode\":\"start_on\",\"relayModeDurationS\":65535,"
      "\"relayOppositeModeDurationS\":32768}],\"entries\":[]}",
      &writer));
  ASSERT_TRUE(responder.called);
  TChannelConfig_WeeklySchedule schedule = {};
  memcpy(&schedule, responder.config.Config, sizeof(schedule));
  EXPECT_EQ(schedule.Program[0].RelayModeDurationS, 65535);
  EXPECT_EQ(schedule.Program[0].RelayOppositeModeDurationS, 32768);
  for (const auto *fields :
       {"\"relayModeDurationS\":-1", "\"relayModeDurationS\":65536",
        "\"relayOppositeModeDurationS\":-1",
        "\"relayModeDurationS\":1,\"value1\":1",
        "\"value2\":1,\"relayOppositeModeDurationS\":1",
        "\"value1\":1,\"value1\":1"}) {
    responder.called = false;
    const std::string command =
        std::string("{\"op\":\"weeklySchedule\",\"channelNumber\":0,") +
        "\"programs\":[{\"mode\":\"start_on\"," + fields + "}],\"entries\":[]}";
    processor.processLine(command.c_str(), &writer);
    EXPECT_FALSE(responder.called) << fields;
  }
}

TEST(CommandProcessorTests, InjectsActionTriggerWeeklySchedule) {
  ChannelConfigResponder responder;
  Supla::Debug::CommandProcessor processor(
      nullptr, &ChannelConfigResponder::handle, &responder);
  CapturingWriter writer;

  EXPECT_TRUE(processor.processLine(
      "{\"op\":\"weeklySchedule\",\"channelNumber\":4,"
      "\"programs\":[{\"mode\":\"locked\"},{\"mode\":\"unlocked\"}],"
      "\"entries\":[{\"days\":[\"sun\"],\"from\":\"00:00\","
      "\"to\":\"24:00\",\"program\":1}]}",
      &writer));

  ASSERT_TRUE(responder.called);
  TChannelConfig_WeeklySchedule schedule = {};
  memcpy(&schedule, responder.config.Config, sizeof(schedule));
  EXPECT_EQ(schedule.Program[0].Mode, SUPLA_BUTTON_MODE_LOCKED);
  EXPECT_EQ(schedule.Program[1].Mode, SUPLA_BUTTON_MODE_NOT_SET);
  EXPECT_EQ(programAt(&schedule, Supla::DayOfWeek_Sunday, 0, 0), 1);
  EXPECT_EQ(programAt(&schedule, Supla::DayOfWeek_Sunday, 23, 45), 1);
  EXPECT_EQ(programAt(&schedule, Supla::DayOfWeek_Monday, 0, 0), 0);
}

TEST(CommandProcessorTests, RejectsUndefinedWeeklyScheduleProgram) {
  ChannelConfigResponder responder;
  Supla::Debug::CommandProcessor processor(
      nullptr, &ChannelConfigResponder::handle, &responder);
  CapturingWriter writer;

  EXPECT_TRUE(processor.processLine(
      "{\"op\":\"weeklySchedule\",\"channelNumber\":4,"
      "\"programs\":[{\"mode\":\"locked\"}],"
      "\"entries\":[{\"days\":[\"mon\"],\"from\":\"08:00\","
      "\"to\":\"09:00\",\"program\":2}]}",
      &writer));

  EXPECT_FALSE(responder.called);
  EXPECT_EQ(writer.response,
            "{\"ok\":false,\"error\":\"invalid_arguments\"}\n");
}

TEST(CommandProcessorTests, ReportsWeeklyScheduleHandlerFailure) {
  ChannelConfigResponder responder;
  responder.result = SUPLA_CONFIG_RESULT_TYPE_NOT_SUPPORTED;
  Supla::Debug::CommandProcessor processor(
      nullptr, &ChannelConfigResponder::handle, &responder);
  CapturingWriter writer;

  EXPECT_TRUE(processor.processLine(
      "{\"op\":\"weeklySchedule\",\"channelNumber\":8,"
      "\"programs\":[],\"entries\":[]}",
      &writer));

  EXPECT_TRUE(responder.called);
  EXPECT_EQ(writer.response,
            "{\"op\":\"weeklySchedule.result\",\"channelNumber\":8,"
            "\"result\":3,\"ok\":false}\n");
}

TEST(CommandProcessorTests, GetsMultichunkDefinitionConfig) {
  CalcfgChunkResponder responder(
      {{22, "abcdefghij"}, {22, "klmnopqrst"}, {22, "uv"}});
  Supla::Debug::CommandProcessor processor(
      nullptr, &CalcfgChunkResponder::handle, &responder);
  CapturingWriter writer;

  EXPECT_TRUE(processor.processLine(
      "{\"calcfg\":\"getDefinitionConfig\",\"definitionId\":1234,"
      "\"definitionVersion\":2}",
      &writer));

  EXPECT_NE(std::string::npos,
            writer.response.find(
                "{\"op\":\"definition.config.data\",\"definitionId\":1234,"
                "\"definitionVersion\":2,\"source\":1,\"json\":"
                "\"abcdefghijklmnopqrstuv\"}\n"));
}

TEST(CommandProcessorTests, GetsMultichunkInstanceConfig) {
  CalcfgChunkResponder responder(
      {{22, "abcdefghij"}, {22, "klmnopqrst"}, {22, "uv"}});
  Supla::Debug::CommandProcessor processor(
      nullptr, &CalcfgChunkResponder::handle, &responder);
  CapturingWriter writer;

  EXPECT_TRUE(processor.processLine(
      "{\"calcfg\":\"getInstanceConfig\",\"instanceId\":7}", &writer));

  EXPECT_NE(
      std::string::npos,
      writer.response.find("{\"op\":\"instance.config.data\",\"instanceId\":7,"
                           "\"json\":\"abcdefghijklmnopqrstuv\"}\n"));
}

TEST(CommandProcessorTests, GetsEmptyDefinitionConfig) {
  CalcfgChunkResponder responder({{0, ""}});
  Supla::Debug::CommandProcessor processor(
      nullptr, &CalcfgChunkResponder::handle, &responder);
  CapturingWriter writer;

  EXPECT_TRUE(processor.processLine(
      "{\"calcfg\":\"getDefinitionConfig\",\"definitionId\":1234,"
      "\"definitionVersion\":2}",
      &writer));

  EXPECT_NE(std::string::npos,
            writer.response.find(
                "{\"op\":\"definition.config.data\",\"definitionId\":1234,"
                "\"definitionVersion\":2,\"source\":1,\"json\":\"\"}\n"));
}

TEST(CommandProcessorTests, GetsEmptyInstanceConfig) {
  CalcfgChunkResponder responder({{0, ""}});
  Supla::Debug::CommandProcessor processor(
      nullptr, &CalcfgChunkResponder::handle, &responder);
  CapturingWriter writer;

  EXPECT_TRUE(processor.processLine(
      "{\"calcfg\":\"getInstanceConfig\",\"instanceId\":7}", &writer));

  EXPECT_NE(std::string::npos,
            writer.response.find("{\"op\":\"instance.config.data\","
                                 "\"instanceId\":7,\"json\":\"\"}\n"));
}

TEST(CommandProcessorTests, RejectsDefinitionConfigChunkExceedingTotalSize) {
  CalcfgChunkResponder responder({{2, "abc"}});
  Supla::Debug::CommandProcessor processor(
      nullptr, &CalcfgChunkResponder::handle, &responder);
  CapturingWriter writer;

  EXPECT_TRUE(processor.processLine(
      "{\"calcfg\":\"getDefinitionConfig\",\"definitionId\":1234,"
      "\"definitionVersion\":2}",
      &writer));

  EXPECT_NE(std::string::npos,
            writer.response.find(
                "{\"ok\":false,\"error\":\"malformed_definition_chunk\"}\n"));
}

TEST(CommandProcessorTests, RejectsInstanceConfigChunkExceedingTotalSize) {
  CalcfgChunkResponder responder({{2, "abc"}});
  Supla::Debug::CommandProcessor processor(
      nullptr, &CalcfgChunkResponder::handle, &responder);
  CapturingWriter writer;

  EXPECT_TRUE(processor.processLine(
      "{\"calcfg\":\"getInstanceConfig\",\"instanceId\":7}", &writer));

  EXPECT_NE(std::string::npos,
            writer.response.find(
                "{\"ok\":false,\"error\":\"malformed_config_chunk\"}\n"));
}

TEST(CommandProcessorTests, RejectsDefinitionConfigWithChangingTotalSize) {
  CalcfgChunkResponder responder({{6, "abc"}, {7, "def"}});
  Supla::Debug::CommandProcessor processor(
      nullptr, &CalcfgChunkResponder::handle, &responder);
  CapturingWriter writer;

  EXPECT_TRUE(processor.processLine(
      "{\"calcfg\":\"getDefinitionConfig\",\"definitionId\":1234,"
      "\"definitionVersion\":2}",
      &writer));

  EXPECT_NE(std::string::npos,
            writer.response.find(
                "{\"ok\":false,\"error\":\"malformed_definition_chunk\"}\n"));
  EXPECT_EQ(std::string::npos, writer.response.find("definition.config.data"));
}

TEST(CommandProcessorTests, RejectsInstanceConfigWithChangingTotalSize) {
  CalcfgChunkResponder responder({{6, "abc"}, {7, "def"}});
  Supla::Debug::CommandProcessor processor(
      nullptr, &CalcfgChunkResponder::handle, &responder);
  CapturingWriter writer;

  EXPECT_TRUE(processor.processLine(
      "{\"calcfg\":\"getInstanceConfig\",\"instanceId\":7}", &writer));

  EXPECT_NE(std::string::npos,
            writer.response.find(
                "{\"ok\":false,\"error\":\"malformed_config_chunk\"}\n"));
  EXPECT_EQ(std::string::npos, writer.response.find("instance.config.data"));
}

TEST(CommandProcessorTests, UpsertInstanceCanKeepArtifact) {
  InstanceBeginResponder responder;
  Supla::Debug::CommandProcessor processor(
      nullptr, &InstanceBeginResponder::handle, &responder);
  CapturingWriter writer;

  EXPECT_TRUE(processor.processLine(
      "{\"calcfg\":\"upsertInstance\",\"instanceId\":7,"
      "\"definitionId\":1234,\"definitionVersion\":2,"
      "\"revision\":3,\"keepArtifact\":true,\"paramsJson\":\"{}\"}",
      &writer));
  EXPECT_EQ(responder.begin.Flags,
            SUPLA_CALCFG_SUPLET_INSTANCE_FLAG_KEEP_ARTIFACT);
  EXPECT_NE(std::string::npos, writer.response.find("\"ok\":true"));
}

}  // namespace

// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>
#include <stddef.h>
#include <string.h>
#include <supla-common/proto.h>
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
               SUPLA_CALCFG_CMD_SUPLET_GET_INSTANCE_CONFIG) {
      TCalCfg_SupletInstanceConfigRequest configRequest = {};
      memcpy(&configRequest, request->Data, sizeof(configRequest));
      if (configRequest.Offset != expectedOffset) {
        return SUPLA_CALCFG_RESULT_FALSE;
      }
      TCalCfg_SupletInstanceConfigChunk output = {};
      output.InstanceId = instanceId;
      output.Offset = expectedOffset;
      output.TotalSize = chunk.totalSize;
      output.Size = chunk.data.size();
      memcpy(output.Data, chunk.data.data(), output.Size);
      result->DataSize =
          offsetof(TCalCfg_SupletInstanceConfigChunk, Data) + output.Size;
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

}  // namespace

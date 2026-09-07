// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <supla/debug/command_processor.h>

#if SUPLA_INSECURE_DEBUG_INTERFACE

#include <SuplaDevice.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <supla-common/proto.h>
#include <supla-common/proto_suplet.h>
#include <supla/suplet/config.h>

#if SUPLA_SUPLET_ENABLED
#include <supla/suplet/server_config.h>
#endif

namespace {

bool equalText(const char *a, const char *b) {
  return a != nullptr && b != nullptr && strcmp(a, b) == 0;
}

class JsonReader {
 public:
  explicit JsonReader(const char *json) : pos(json) {}

  void skipWhitespace() {
    while (pos != nullptr &&
           (*pos == ' ' || *pos == '\n' || *pos == '\r' || *pos == '\t')) {
      pos++;
    }
  }

  bool consume(char expected) {
    skipWhitespace();
    if (pos == nullptr || *pos != expected) {
      return false;
    }
    pos++;
    return true;
  }

  bool readString(char *output, size_t outputSize) {
    if (output == nullptr || outputSize == 0 || !consume('"')) {
      return false;
    }
    size_t out = 0;
    while (pos != nullptr && *pos != '\0') {
      char c = *pos++;
      if (c == '"') {
        output[out] = '\0';
        return true;
      }
      if (c == '\\') {
        if (*pos == '\0') {
          return false;
        }
        c = *pos++;
        switch (c) {
          case '"':
          case '\\':
          case '/':
            break;
          case 'b':
            c = '\b';
            break;
          case 'f':
            c = '\f';
            break;
          case 'n':
            c = '\n';
            break;
          case 'r':
            c = '\r';
            break;
          case 't':
            c = '\t';
            break;
          default:
            return false;
        }
      }
      if (out + 1 >= outputSize) {
        return false;
      }
      output[out++] = c;
    }
    return false;
  }

  bool skipString() {
    if (!consume('"')) {
      return false;
    }
    while (pos != nullptr && *pos != '\0') {
      char c = *pos++;
      if (c == '"') {
        return true;
      }
      if (c == '\\') {
        if (*pos == '\0') {
          return false;
        }
        c = *pos++;
        if (c == 'u') {
          for (int i = 0; i < 4; i++) {
            char hex = *pos++;
            bool valid = (hex >= '0' && hex <= '9') ||
                (hex >= 'a' && hex <= 'f') || (hex >= 'A' && hex <= 'F');
            if (!valid) {
              return false;
            }
          }
        }
      }
    }
    return false;
  }

  bool readUInt32(uint32_t *value) {
    if (value == nullptr) {
      return false;
    }
    skipWhitespace();
    if (pos == nullptr || *pos < '0' || *pos > '9') {
      return false;
    }
    uint32_t result = 0;
    while (*pos >= '0' && *pos <= '9') {
      uint32_t digit = static_cast<uint32_t>(*pos - '0');
      if (result > (UINT32_MAX - digit) / 10) {
        return false;
      }
      result = result * 10 + digit;
      pos++;
    }
    *value = result;
    return true;
  }

  bool skipNumber() {
    skipWhitespace();
    if (*pos == '-') {
      pos++;
    }
    if (*pos < '0' || *pos > '9') {
      return false;
    }
    while (*pos >= '0' && *pos <= '9') {
      pos++;
    }
    if (*pos == '.') {
      pos++;
      if (*pos < '0' || *pos > '9') {
        return false;
      }
      while (*pos >= '0' && *pos <= '9') {
        pos++;
      }
    }
    if (*pos == 'e' || *pos == 'E') {
      pos++;
      if (*pos == '+' || *pos == '-') {
        pos++;
      }
      if (*pos < '0' || *pos > '9') {
        return false;
      }
      while (*pos >= '0' && *pos <= '9') {
        pos++;
      }
    }
    return true;
  }

  bool consumeLiteral(const char *text) {
    skipWhitespace();
    size_t len = strlen(text);
    if (strncmp(pos, text, len) != 0) {
      return false;
    }
    pos += len;
    return true;
  }

  bool readBool(bool *value) {
    if (value == nullptr) {
      return false;
    }
    if (consumeLiteral("true")) {
      *value = true;
      return true;
    }
    if (consumeLiteral("false")) {
      *value = false;
      return true;
    }
    return false;
  }

  bool skipValue() {
    skipWhitespace();
    if (pos == nullptr) {
      return false;
    }
    if (*pos == '"') {
      return skipString();
    }
    if (*pos == '{') {
      pos++;
      skipWhitespace();
      if (*pos == '}') {
        pos++;
        return true;
      }
      while (true) {
        if (!skipString() || !consume(':') || !skipValue()) {
          return false;
        }
        skipWhitespace();
        if (*pos == '}') {
          pos++;
          return true;
        }
        if (!consume(',')) {
          return false;
        }
      }
    }
    if (*pos == '[') {
      pos++;
      skipWhitespace();
      if (*pos == ']') {
        pos++;
        return true;
      }
      while (true) {
        if (!skipValue()) {
          return false;
        }
        skipWhitespace();
        if (*pos == ']') {
          pos++;
          return true;
        }
        if (!consume(',')) {
          return false;
        }
      }
    }
    if ((*pos >= '0' && *pos <= '9') || *pos == '-') {
      return skipNumber();
    }
    return consumeLiteral("true") || consumeLiteral("false") ||
           consumeLiteral("null");
  }

  bool atEnd() {
    skipWhitespace();
    return pos != nullptr && *pos == '\0';
  }

 private:
  const char *pos = nullptr;
};

char *allocString(size_t size) {
  char *result = new char[size];
  if (result != nullptr) {
    result[0] = '\0';
  }
  return result;
}

}  // namespace

namespace Supla {
namespace Debug {

struct CommandProcessor::Command {
  ~Command() {
    delete[] definitionJson;
    delete[] paramsJson;
  }

  char operation[32] = {};
  uint32_t instanceId = 0;
  uint32_t definitionId = 0;
  uint32_t definitionVersion = 0;
  uint32_t revision = 0;
  bool keepArtifact = false;
  char *definitionJson = nullptr;
  char *paramsJson = nullptr;
};

CommandProcessor::CommandProcessor(SuplaDeviceClass *device) : device(device) {
}

#if SUPLA_TEST
CommandProcessor::CommandProcessor(SuplaDeviceClass *device,
                                   TestCalcfgHandler testCalcfgHandler,
                                   void *testCalcfgContext)
    : device(device),
      testCalcfgHandler(testCalcfgHandler),
      testCalcfgContext(testCalcfgContext) {
}
#endif

bool CommandProcessor::processLine(const char *line, ResponseWriter *writer) {
  if (line == nullptr || line[0] == '\0') {
    return false;
  }

  Command command;
  if (parseCommand(line, &command)) {
    processCommand(command, writer);
    return true;
  }

  processDirectCommandJson(line, writer);
  return true;
}

bool CommandProcessor::parseCommand(const char *json, Command *command) {
  if (json == nullptr || command == nullptr) {
    return false;
  }

  JsonReader reader(json);
  if (!reader.consume('{')) {
    return false;
  }
  reader.skipWhitespace();
  if (reader.consume('}')) {
    return false;
  }

  while (true) {
    char key[32] = {};
    if (!reader.readString(key, sizeof(key)) || !reader.consume(':')) {
      return false;
    }

    if (equalText(key, "calcfg") || equalText(key, "op")) {
      if (!reader.readString(command->operation, sizeof(command->operation))) {
        return false;
      }
    } else if (equalText(key, "instanceId")) {
      if (!reader.readUInt32(&command->instanceId)) {
        return false;
      }
    } else if (equalText(key, "definitionId")) {
      if (!reader.readUInt32(&command->definitionId)) {
        return false;
      }
    } else if (equalText(key, "definitionVersion")) {
      if (!reader.readUInt32(&command->definitionVersion)) {
        return false;
      }
    } else if (equalText(key, "revision")) {
      if (!reader.readUInt32(&command->revision)) {
        return false;
      }
    } else if (equalText(key, "keepArtifact")) {
      if (!reader.readBool(&command->keepArtifact)) {
        return false;
      }
    } else if (equalText(key, "definitionJson")) {
      if (command->definitionJson != nullptr) {
        return false;
      }
      command->definitionJson =
          allocString(SUPLA_SUPLET_MAX_DEFINITION_JSON_SIZE + 1);
      if (command->definitionJson == nullptr ||
          !reader.readString(command->definitionJson,
                             SUPLA_SUPLET_MAX_DEFINITION_JSON_SIZE + 1)) {
        return false;
      }
    } else if (equalText(key, "paramsJson")) {
      if (command->paramsJson != nullptr) {
        return false;
      }
      command->paramsJson = allocString(SUPLA_SUPLET_MAX_CONFIG_SIZE + 1);
      if (command->paramsJson == nullptr ||
          !reader.readString(command->paramsJson,
                             SUPLA_SUPLET_MAX_CONFIG_SIZE + 1)) {
        return false;
      }
    } else if (!reader.skipValue()) {
      return false;
    }

    reader.skipWhitespace();
    if (reader.consume('}')) {
      break;
    }
    if (!reader.consume(',')) {
      return false;
    }
  }

  return reader.atEnd() && command->operation[0] != '\0';
}

void CommandProcessor::processCommand(const Command &command,
                                      ResponseWriter *writer) {
#if SUPLA_SUPLET_ENABLED
  auto sendLocalCalcfg =
      [&](uint32_t commandId, const void *data, uint32_t dataSize,
          const char *op, TDS_DeviceCalCfgResult **output = nullptr) -> int {
    if (
#if SUPLA_TEST
        (device == nullptr && testCalcfgHandler == nullptr) ||
#else
        device == nullptr ||
#endif
        dataSize > SUPLA_CALCFG_DATA_MAXSIZE) {
      sendError(writer, "invalid_calcfg_payload");
      return SUPLA_CALCFG_RESULT_FALSE;
    }

    auto *request = new TSD_DeviceCalCfgRequest;
    auto *result = new TDS_DeviceCalCfgResult;
    if (request == nullptr || result == nullptr) {
      delete request;
      delete result;
      sendError(writer, "no_memory");
      return SUPLA_CALCFG_RESULT_FALSE;
    }
    memset(request, 0, sizeof(*request));
    memset(result, 0, sizeof(*result));
    request->ChannelNumber = -1;
    request->Command = commandId;
    request->SuperUserAuthorized = 1;
    request->DataType = 0;
    request->DataSize = dataSize;
    if (data != nullptr && dataSize > 0) {
      memcpy(request->Data, data, dataSize);
    }
    result->ChannelNumber = -1;
    result->Command = commandId;
    int resultCode =
#if SUPLA_TEST
        testCalcfgHandler != nullptr
            ? testCalcfgHandler(testCalcfgContext, request, result)
            :
#endif
            device->handleCalcfgFromServer(request, result);
    result->Result = resultCode;
    char line[320] = {};
    if (result->DataSize == sizeof(TCalCfg_SupletResult)) {
      TCalCfg_SupletResult supletResult = {};
      memcpy(&supletResult, result->Data, sizeof(supletResult));
      snprintf(line,
               sizeof(line),
               "{\"op\":\"%s\",\"result\":%d,\"detail\":%u,\"phase\":%u,"
               "\"instanceId\":%u,\"definitionId\":%u,"
               "\"definitionVersion\":%u,\"required\":%u,\"available\":%u}\n",
               op ? op : "",
               resultCode,
               supletResult.DetailCode,
               supletResult.Phase,
               supletResult.InstanceId,
               static_cast<unsigned>(supletResult.DefinitionId),
               supletResult.DefinitionVersion,
               supletResult.Required,
               supletResult.Available);
    } else {
      snprintf(line,
               sizeof(line),
               "{\"op\":\"%s\",\"result\":%d,\"dataSize\":%u}\n",
               op ? op : "",
               resultCode,
               static_cast<unsigned>(result->DataSize));
    }
    sendText(writer, line);
    delete request;
    if (output != nullptr) {
      *output = result;
    } else {
      delete result;
    }
    return resultCode;
  };

  if (equalText(command.operation, "saveDefinition")) {
    if (command.definitionId == 0 || command.definitionVersion == 0 ||
        command.definitionVersion > UINT16_MAX ||
        command.definitionJson == nullptr || command.definitionJson[0] == 0) {
      sendError(writer, "invalid_arguments");
      return;
    }
    size_t jsonSize = strlen(command.definitionJson);
    if (jsonSize > UINT16_MAX ||
        jsonSize > SUPLA_SUPLET_MAX_DEFINITION_JSON_SIZE) {
      sendError(writer, "definition_too_large");
      return;
    }
    TCalCfg_SupletDefinitionBegin begin = {};
    begin.DefinitionId = command.definitionId;
    begin.DefinitionVersion = command.definitionVersion;
    begin.Size = static_cast<uint16_t>(jsonSize);
    if (sendLocalCalcfg(SUPLA_CALCFG_CMD_SUPLET_DEFINITION_BEGIN,
                        &begin,
                        sizeof(begin),
                        "definition.begin") != SUPLA_CALCFG_RESULT_DONE) {
      sendDone(writer, false);
      return;
    }
    uint16_t offset = 0;
    auto *chunk = new TCalCfg_SupletTransferChunk;
    if (chunk == nullptr) {
      sendError(writer, "no_memory");
      return;
    }
    while (offset < jsonSize) {
      memset(chunk, 0, sizeof(*chunk));
      uint8_t chunkSize =
          jsonSize - offset > SUPLA_CALCFG_SUPLET_TRANSFER_CHUNK_MAXSIZE
              ? SUPLA_CALCFG_SUPLET_TRANSFER_CHUNK_MAXSIZE
              : static_cast<uint8_t>(jsonSize - offset);
      chunk->Part = SUPLA_CALCFG_SUPLET_TRANSFER_PART_DEFINITION;
      chunk->Offset = offset;
      chunk->Size = chunkSize;
      memcpy(chunk->Data, command.definitionJson + offset, chunkSize);
      uint32_t payloadSize =
          offsetof(TCalCfg_SupletTransferChunk, Data) + chunkSize;
      if (sendLocalCalcfg(SUPLA_CALCFG_CMD_SUPLET_TRANSFER_CHUNK,
                          chunk,
                          payloadSize,
                          "definition.chunk") != SUPLA_CALCFG_RESULT_DONE) {
        delete chunk;
        sendDone(writer, false);
        return;
      }
      offset += chunkSize;
    }
    delete chunk;
    bool ok = sendLocalCalcfg(SUPLA_CALCFG_CMD_SUPLET_TRANSFER_COMMIT,
                              nullptr,
                              0,
                              "definition.commit") == SUPLA_CALCFG_RESULT_DONE;
    sendDone(writer, ok);
    return;
  }

  if (equalText(command.operation, "removeDefinition")) {
    if (command.definitionId == 0 || command.definitionVersion == 0 ||
        command.definitionVersion > UINT16_MAX) {
      sendError(writer, "invalid_arguments");
      return;
    }
    TCalCfg_SupletDefinitionRequest request = {};
    request.DefinitionId = command.definitionId;
    request.DefinitionVersion = command.definitionVersion;
    int result = sendLocalCalcfg(SUPLA_CALCFG_CMD_SUPLET_DEFINITION_REMOVE,
                                 &request,
                                 sizeof(request),
                                 "definition.remove");
    sendDone(writer, result == SUPLA_CALCFG_RESULT_DONE);
    return;
  }

  if (equalText(command.operation, "upsertInstance")) {
    if (command.definitionId == 0 || command.definitionVersion == 0 ||
        command.definitionVersion > UINT16_MAX ||
        command.instanceId > UINT8_MAX || command.revision == 0) {
      sendError(writer, "invalid_arguments");
      return;
    }
    const char *paramsJson = command.paramsJson ? command.paramsJson : "{}";
    size_t paramsSize = strlen(paramsJson);
    if (paramsSize > UINT16_MAX || paramsSize > SUPLA_SUPLET_MAX_CONFIG_SIZE) {
      sendError(writer, "params_too_large");
      return;
    }
    TCalCfg_SupletInstanceBegin begin = {};
    begin.InstanceId = command.instanceId;
    begin.DefinitionId = command.definitionId;
    begin.DefinitionVersion = command.definitionVersion;
    begin.Revision = command.revision;
    begin.ConfigSize = static_cast<uint16_t>(paramsSize);
    begin.Flags = command.keepArtifact
                      ? SUPLA_CALCFG_SUPLET_INSTANCE_FLAG_KEEP_ARTIFACT
                      : 0;
    if (sendLocalCalcfg(SUPLA_CALCFG_CMD_SUPLET_INSTANCE_BEGIN,
                        &begin,
                        sizeof(begin),
                        "instance.begin") != SUPLA_CALCFG_RESULT_DONE) {
      sendDone(writer, false);
      return;
    }
    uint16_t offset = 0;
    auto *chunk = new TCalCfg_SupletTransferChunk;
    if (chunk == nullptr) {
      sendError(writer, "no_memory");
      return;
    }
    while (offset < paramsSize) {
      memset(chunk, 0, sizeof(*chunk));
      uint8_t chunkSize =
          paramsSize - offset > SUPLA_CALCFG_SUPLET_TRANSFER_CHUNK_MAXSIZE
              ? SUPLA_CALCFG_SUPLET_TRANSFER_CHUNK_MAXSIZE
              : static_cast<uint8_t>(paramsSize - offset);
      chunk->Part = SUPLA_CALCFG_SUPLET_TRANSFER_PART_CONFIG;
      chunk->Offset = offset;
      chunk->Size = chunkSize;
      memcpy(chunk->Data, paramsJson + offset, chunkSize);
      uint32_t payloadSize =
          offsetof(TCalCfg_SupletTransferChunk, Data) + chunkSize;
      if (sendLocalCalcfg(SUPLA_CALCFG_CMD_SUPLET_TRANSFER_CHUNK,
                          chunk,
                          payloadSize,
                          "instance.chunk") != SUPLA_CALCFG_RESULT_DONE) {
        delete chunk;
        sendDone(writer, false);
        return;
      }
      offset += chunkSize;
    }
    delete chunk;
    bool ok = sendLocalCalcfg(SUPLA_CALCFG_CMD_SUPLET_TRANSFER_COMMIT,
                              nullptr,
                              0,
                              "instance.commit") == SUPLA_CALCFG_RESULT_DONE;
    sendDone(writer, ok);
    return;
  }

  if (equalText(command.operation, "removeInstance")) {
    if (command.instanceId == 0 || command.instanceId > UINT8_MAX) {
      sendError(writer, "invalid_arguments");
      return;
    }
    TCalCfg_SupletInstanceRequest request = {};
    request.InstanceId = command.instanceId;
    int result = sendLocalCalcfg(SUPLA_CALCFG_CMD_SUPLET_INSTANCE_REMOVE,
                                 &request,
                                 sizeof(request),
                                 "instance.remove");
    sendDone(writer, result == SUPLA_CALCFG_RESULT_DONE);
    return;
  }

  if (equalText(command.operation, "getCapabilities")) {
    TCalCfg_SupletListRequest request = {};
    request.Limit = SUPLA_CALCFG_SUPLET_CAPABILITY_MAX_ITEMS;
    TDS_DeviceCalCfgResult *result = nullptr;
    if (sendLocalCalcfg(SUPLA_CALCFG_CMD_SUPLET_GET_CAPABILITIES,
                        &request,
                        sizeof(request),
                        "capabilities",
                        &result) != SUPLA_CALCFG_RESULT_TRUE ||
        result == nullptr ||
        result->DataSize != sizeof(TCalCfg_SupletCapabilityList)) {
      delete result;
      return;
    }
    auto *list = reinterpret_cast<TCalCfg_SupletCapabilityList *>(result->Data);
    char header[96] = {};
    snprintf(header,
             sizeof(header),
             "{\"op\":\"capabilities.data\",\"count\":%u,\"total\":%u,"
             "\"items\":[",
             list->Count,
             list->Total);
    sendText(writer, header);
    for (uint8_t i = 0; i < list->Count; i++) {
      const auto &item = list->Items[i];
      char line[288] = {};
      snprintf(line,
               sizeof(line),
               "%s{\"category\":%u,\"kind\":%u,\"schemaMin\":%u,"
               "\"schemaMax\":%u,\"handler\":%u,\"maxInstances\":%u,"
               "\"downloaded\":%u,\"definitionId\":%u,"
               "\"definitionVersionMin\":%u,\"definitionVersionMax\":%u,"
               "\"maxArtifactSize\":%u}",
               i == 0 ? "" : ",",
               item.Category,
               item.Kind,
               item.MinSchemaVersion,
               item.MaxSchemaVersion,
               item.HandlerVersion,
               item.MaxInstances,
               item.SupportsDownloadedDefinition,
               static_cast<unsigned>(item.DefinitionId),
               item.MinDefinitionVersion,
               item.MaxDefinitionVersion,
               static_cast<unsigned>(item.MaxArtifactSize));
      sendText(writer, line);
    }
    sendText(writer, "]}\n");
    delete result;
    return;
  }

  if (equalText(command.operation, "getDefinitionList")) {
    TCalCfg_SupletListRequest request = {};
    request.Limit = SUPLA_CALCFG_SUPLET_DEFINITION_LIST_MAX_ITEMS;
    TDS_DeviceCalCfgResult *result = nullptr;
    if (sendLocalCalcfg(SUPLA_CALCFG_CMD_SUPLET_GET_DEFINITION_LIST,
                        &request,
                        sizeof(request),
                        "definition.list",
                        &result) != SUPLA_CALCFG_RESULT_TRUE ||
        result == nullptr ||
        result->DataSize != sizeof(TCalCfg_SupletDefinitionList)) {
      delete result;
      return;
    }
    auto *list = reinterpret_cast<TCalCfg_SupletDefinitionList *>(result->Data);
    char header[96] = {};
    snprintf(header,
             sizeof(header),
             "{\"op\":\"definition.list.data\",\"count\":%u,\"total\":%u,"
             "\"items\":[",
             list->Count,
             list->Total);
    sendText(writer, header);
    for (uint8_t i = 0; i < list->Count; i++) {
      const auto &item = list->Items[i];
      char line[256] = {};
      snprintf(line,
               sizeof(line),
               "%s{\"source\":%u,\"definitionId\":%u,"
               "\"definitionVersion\":%u,\"size\":%u}",
               i == 0 ? "" : ",",
               item.Source,
               static_cast<unsigned>(item.DefinitionId),
               item.DefinitionVersion,
               item.Size);
      sendText(writer, line);
    }
    sendText(writer, "]}\n");
    delete result;
    return;
  }

  if (equalText(command.operation, "getDefinitionConfig")) {
    if (command.definitionId == 0 || command.definitionVersion == 0 ||
        command.definitionVersion > UINT16_MAX) {
      sendError(writer, "invalid_arguments");
      return;
    }
    char *definitionJson =
        new char[SUPLA_SUPLET_MAX_DEFINITION_JSON_SIZE + 1];
    if (definitionJson == nullptr) {
      sendError(writer, "no_memory");
      return;
    }
    uint16_t offset = 0;
    uint16_t totalSize = 0;
    uint8_t source = 0;
    while (true) {
      TCalCfg_SupletDefinitionConfigRequest request = {};
      request.DefinitionId = command.definitionId;
      request.DefinitionVersion = command.definitionVersion;
      request.Offset = offset;
      request.MaxSize = SUPLA_CALCFG_SUPLET_DATA_CHUNK_MAXSIZE;
      TDS_DeviceCalCfgResult *result = nullptr;
      if (sendLocalCalcfg(SUPLA_CALCFG_CMD_SUPLET_GET_DEFINITION_CONFIG,
                          &request,
                          sizeof(request),
                          "definition.config",
                          &result) != SUPLA_CALCFG_RESULT_TRUE ||
          result == nullptr ||
          result->DataSize <
              offsetof(TCalCfg_SupletDefinitionConfigChunk, Data)) {
        delete result;
        delete[] definitionJson;
        return;
      }
      auto *chunk =
          reinterpret_cast<TCalCfg_SupletDefinitionConfigChunk *>(result->Data);
      const uint16_t chunkTotalSize = chunk->TotalSize;
      const uint8_t chunkSize = chunk->Size;
      if (chunk->DefinitionId != command.definitionId ||
          chunk->DefinitionVersion != command.definitionVersion ||
          result->DataSize !=
              offsetof(TCalCfg_SupletDefinitionConfigChunk, Data) +
                  chunkSize ||
          chunkTotalSize > SUPLA_SUPLET_MAX_DEFINITION_JSON_SIZE ||
          offset + chunkSize > SUPLA_SUPLET_MAX_DEFINITION_JSON_SIZE ||
          (offset != 0 && chunkTotalSize != totalSize) ||
          offset + chunkSize > chunkTotalSize) {
        delete result;
        delete[] definitionJson;
        sendError(writer, "malformed_definition_chunk");
        return;
      }
      if (offset == 0) {
        totalSize = chunkTotalSize;
        source = chunk->Source;
      }
      memcpy(definitionJson + offset, chunk->Data, chunkSize);
      offset += chunkSize;
      delete result;
      if (offset >= chunkTotalSize || chunkSize == 0) {
        break;
      }
    }
    definitionJson[totalSize] = '\0';
    char prefix[128] = {};
    snprintf(prefix,
             sizeof(prefix),
             "{\"op\":\"definition.config.data\",\"definitionId\":%u,"
             "\"definitionVersion\":%u,\"source\":%u,\"json\":",
             static_cast<unsigned>(command.definitionId),
             static_cast<unsigned>(command.definitionVersion),
             source);
    sendJsonString(writer, prefix, definitionJson, "}\n");
    delete[] definitionJson;
    return;
  }

  if (equalText(command.operation, "getInstanceList")) {
    TCalCfg_SupletListRequest request = {};
    request.Limit = SUPLA_CALCFG_SUPLET_INSTANCE_LIST_MAX_ITEMS;
    TDS_DeviceCalCfgResult *result = nullptr;
    if (sendLocalCalcfg(SUPLA_CALCFG_CMD_SUPLET_GET_INSTANCE_LIST,
                        &request,
                        sizeof(request),
                        "instance.list",
                        &result) != SUPLA_CALCFG_RESULT_TRUE ||
        result == nullptr ||
        result->DataSize != sizeof(TCalCfg_SupletInstanceList)) {
      delete result;
      return;
    }
    auto *list = reinterpret_cast<TCalCfg_SupletInstanceList *>(result->Data);
    char header[96] = {};
    snprintf(header,
             sizeof(header),
             "{\"op\":\"instance.list.data\",\"count\":%u,\"total\":%u,"
             "\"items\":[",
             list->Count,
             list->Total);
    sendText(writer, header);
    for (uint8_t i = 0; i < list->Count; i++) {
      const auto &item = list->Items[i];
      char line[256] = {};
      snprintf(line,
               sizeof(line),
               "%s{\"instanceId\":%u,\"definitionId\":%u,"
               "\"definitionVersion\":%u,\"revision\":%u,"
               "\"subDeviceId\":%u,\"channelCount\":%u,"
               "\"configSize\":%u,\"artifactSize\":%u}",
               i == 0 ? "" : ",",
               item.InstanceId,
               static_cast<unsigned>(item.DefinitionId),
               item.DefinitionVersion,
               static_cast<unsigned>(item.Revision),
               item.SubDeviceId,
               item.ChannelCount,
               item.ConfigSize,
               static_cast<unsigned>(item.ArtifactSize));
      sendText(writer, line);
    }
    sendText(writer, "]}\n");
    delete result;
    return;
  }

  if (equalText(command.operation, "getInstanceConfig")) {
    if (command.instanceId == 0 || command.instanceId > UINT8_MAX) {
      sendError(writer, "invalid_arguments");
      return;
    }
    char *config = new char[SUPLA_SUPLET_MAX_CONFIG_SIZE + 1];
    if (config == nullptr) {
      sendError(writer, "no_memory");
      return;
    }
    uint16_t offset = 0;
    uint16_t totalSize = 0;
    while (true) {
      TCalCfg_SupletInstanceDataRequest request = {};
      request.InstanceId = command.instanceId;
      request.Part = SUPLA_CALCFG_SUPLET_TRANSFER_PART_CONFIG;
      request.Offset = offset;
      request.MaxSize = SUPLA_CALCFG_SUPLET_DATA_CHUNK_MAXSIZE;
      TDS_DeviceCalCfgResult *result = nullptr;
      if (sendLocalCalcfg(SUPLA_CALCFG_CMD_SUPLET_GET_INSTANCE_DATA,
                          &request,
                          sizeof(request),
                          "instance.config",
                          &result) != SUPLA_CALCFG_RESULT_TRUE ||
          result == nullptr ||
          result->DataSize <
              offsetof(TCalCfg_SupletInstanceDataChunk, Data)) {
        delete result;
        delete[] config;
        return;
      }
      auto *chunk =
          reinterpret_cast<TCalCfg_SupletInstanceDataChunk *>(result->Data);
      const uint32_t chunkTotalSize = chunk->TotalSize;
      const uint8_t chunkSize = chunk->Size;
      if (chunk->InstanceId != command.instanceId ||
          result->DataSize !=
              offsetof(TCalCfg_SupletInstanceDataChunk, Data) +
                  chunkSize ||
          chunkTotalSize > SUPLA_SUPLET_MAX_CONFIG_SIZE ||
          offset + chunkSize > SUPLA_SUPLET_MAX_CONFIG_SIZE ||
          (offset != 0 && chunkTotalSize != totalSize) ||
          offset + chunkSize > chunkTotalSize) {
        delete result;
        delete[] config;
        sendError(writer, "malformed_config_chunk");
        return;
      }
      if (offset == 0) {
        totalSize = chunkTotalSize;
      }
      memcpy(config + offset, chunk->Data, chunkSize);
      offset += chunkSize;
      delete result;
      if (offset >= chunkTotalSize || chunkSize == 0) {
        break;
      }
    }
    config[totalSize] = '\0';
    char prefix[96] = {};
    snprintf(prefix,
             sizeof(prefix),
             "{\"op\":\"instance.config.data\",\"instanceId\":%u,\"json\":",
             static_cast<unsigned>(command.instanceId));
    sendJsonString(writer, prefix, config, "}\n");
    delete[] config;
    return;
  }
#endif

  sendError(writer, "unsupported_operation");
}

void CommandProcessor::processDirectCommandJson(const char *line,
                                                ResponseWriter *writer) {
#if SUPLA_SUPLET_ENABLED
  if (device == nullptr) {
    sendError(writer, "device_not_available");
    return;
  }
  auto validationResult = device->validateSupletCommandJson(line);
  if (validationResult != Supla::Suplet::ServerConfigResult::Applied &&
      validationResult != Supla::Suplet::ServerConfigResult::Removed) {
    char response[96] = {};
    snprintf(response,
             sizeof(response),
             "{\"ok\":false,\"error\":\"validation_failed\",\"result\":%d}\n",
             static_cast<int>(validationResult));
    sendText(writer, response);
    return;
  }

  auto applyResult = device->applySupletCommandJson(line);
  char response[80] = {};
  snprintf(response,
           sizeof(response),
           "{\"ok\":%s,\"result\":%d}\n",
           applyResult == Supla::Suplet::ServerConfigResult::Applied ||
                   applyResult == Supla::Suplet::ServerConfigResult::Removed
               ? "true"
               : "false",
           static_cast<int>(applyResult));
  sendText(writer, response);
#else
  (void)(line);
  sendError(writer, "unsupported_operation");
#endif
}

void CommandProcessor::sendError(ResponseWriter *writer, const char *error) {
  char line[128] = {};
  snprintf(line,
           sizeof(line),
           "{\"ok\":false,\"error\":\"%s\"}\n",
           error ? error : "unknown");
  sendText(writer, line);
}

void CommandProcessor::sendDone(ResponseWriter *writer, bool ok) {
  sendText(writer,
           ok ? "{\"op\":\"done\",\"ok\":true}\n"
              : "{\"op\":\"done\",\"ok\":false}\n");
}

void CommandProcessor::sendText(ResponseWriter *writer, const char *text) {
  if (writer != nullptr && text != nullptr) {
    writer->write(text);
  }
}

void CommandProcessor::sendJsonString(ResponseWriter *writer,
                                      const char *prefix,
                                      const char *value,
                                      const char *suffix) {
  sendText(writer, prefix);
  sendText(writer, "\"");
  if (value != nullptr) {
    for (const char *ptr = value; *ptr != '\0'; ptr++) {
      char escaped[8] = {};
      switch (*ptr) {
        case '\\':
          sendText(writer, "\\\\");
          break;
        case '"':
          sendText(writer, "\\\"");
          break;
        case '\n':
          sendText(writer, "\\n");
          break;
        case '\r':
          sendText(writer, "\\r");
          break;
        case '\t':
          sendText(writer, "\\t");
          break;
        default:
          if (static_cast<unsigned char>(*ptr) < 0x20) {
            snprintf(escaped, sizeof(escaped), "\\u%04x", *ptr);
            sendText(writer, escaped);
          } else {
            char one[2] = {*ptr, '\0'};
            sendText(writer, one);
          }
          break;
      }
    }
  }
  sendText(writer, "\"");
  sendText(writer, suffix);
}

}  // namespace Debug
}  // namespace Supla

#endif  // SUPLA_INSECURE_DEBUG_INTERFACE

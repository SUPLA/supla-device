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

#include <supla/debug/command_processor.h>

#if SUPLA_INSECURE_DEBUG_INTERFACE

#include <SuplaDevice.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <supla-common/proto.h>
#include <supla/sha256.h>
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

void calculateSha256(const char *data, uint16_t dataSize, uint8_t *output) {
  if (output == nullptr) {
    return;
  }
  Supla::Sha256 sha256;
  if (data != nullptr && dataSize > 0) {
    sha256.update(reinterpret_cast<const uint8_t *>(data), dataSize);
  }
  sha256.digest(output, 32);
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
  uint32_t sessionId = 0;
  uint32_t instanceId = 0;
  uint32_t definitionId = 0;
  uint32_t definitionVersion = 0;
  uint32_t fromDefinitionVersion = 0;
  uint32_t toDefinitionVersion = 0;
  char *definitionJson = nullptr;
  char *paramsJson = nullptr;
};

CommandProcessor::CommandProcessor(SuplaDeviceClass *device) : device(device) {
}

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
    } else if (equalText(key, "sessionId")) {
      if (!reader.readUInt32(&command->sessionId)) {
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
    } else if (equalText(key, "fromDefinitionVersion")) {
      if (!reader.readUInt32(&command->fromDefinitionVersion)) {
        return false;
      }
    } else if (equalText(key, "toDefinitionVersion")) {
      if (!reader.readUInt32(&command->toDefinitionVersion)) {
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
    if (device == nullptr || dataSize > SUPLA_CALCFG_DATA_MAXSIZE) {
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
    int resultCode = device->handleCalcfgFromServer(request, result);
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
    uint32_t sessionId =
        command.sessionId == 0 ? nextSessionId() : command.sessionId;
    uint8_t sha256[32] = {};
    calculateSha256(command.definitionJson, static_cast<uint16_t>(jsonSize),
                    sha256);
    TCalCfg_SupletDefinitionBegin begin = {};
    begin.SessionId = sessionId;
    begin.DefinitionId = command.definitionId;
    begin.DefinitionVersion = command.definitionVersion;
    begin.JsonSize = static_cast<uint16_t>(jsonSize);
    memcpy(begin.JsonSha256, sha256, sizeof(sha256));
    if (sendLocalCalcfg(SUPLA_CALCFG_CMD_SUPLET_DEFINITION_BEGIN,
                        &begin,
                        sizeof(begin),
                        "definition.begin") != SUPLA_CALCFG_RESULT_DONE) {
      sendDone(writer, false);
      return;
    }
    uint16_t offset = 0;
    auto *chunk = new TCalCfg_SupletDefinitionChunk;
    if (chunk == nullptr) {
      sendError(writer, "no_memory");
      return;
    }
    while (offset < jsonSize) {
      memset(chunk, 0, sizeof(*chunk));
      uint8_t chunkSize =
          jsonSize - offset > SUPLA_CALCFG_SUPLET_DEFINITION_CHUNK_MAXSIZE
              ? SUPLA_CALCFG_SUPLET_DEFINITION_CHUNK_MAXSIZE
              : static_cast<uint8_t>(jsonSize - offset);
      chunk->SessionId = sessionId;
      chunk->Offset = offset;
      chunk->Size = chunkSize;
      memcpy(chunk->Data, command.definitionJson + offset, chunkSize);
      uint32_t payloadSize =
          offsetof(TCalCfg_SupletDefinitionChunk, Data) + chunkSize;
      if (sendLocalCalcfg(SUPLA_CALCFG_CMD_SUPLET_DEFINITION_CHUNK,
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
    TCalCfg_SupletSessionRequest commit = {};
    bool ok = sendLocalCalcfg(SUPLA_CALCFG_CMD_SUPLET_DEFINITION_COMMIT,
                              &commit,
                              sizeof(commit),
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
        command.instanceId > UINT8_MAX) {
      sendError(writer, "invalid_arguments");
      return;
    }
    const char *paramsJson = command.paramsJson ? command.paramsJson : "{}";
    size_t paramsSize = strlen(paramsJson);
    if (paramsSize > UINT16_MAX || paramsSize > SUPLA_SUPLET_MAX_CONFIG_SIZE) {
      sendError(writer, "params_too_large");
      return;
    }
    uint32_t sessionId =
        command.sessionId == 0 ? nextSessionId() : command.sessionId;
    uint8_t sha256[32] = {};
    calculateSha256(paramsJson, static_cast<uint16_t>(paramsSize), sha256);
    TCalCfg_SupletInstanceBegin begin = {};
    begin.SessionId = sessionId;
    begin.InstanceId = command.instanceId;
    begin.DefinitionId = command.definitionId;
    begin.DefinitionVersion = command.definitionVersion;
    begin.ParamsSize = static_cast<uint16_t>(paramsSize);
    memcpy(begin.ParamsSha256, sha256, sizeof(sha256));
    if (sendLocalCalcfg(SUPLA_CALCFG_CMD_SUPLET_INSTANCE_BEGIN,
                        &begin,
                        sizeof(begin),
                        "instance.begin") != SUPLA_CALCFG_RESULT_DONE) {
      sendDone(writer, false);
      return;
    }
    uint16_t offset = 0;
    auto *chunk = new TCalCfg_SupletInstanceChunk;
    if (chunk == nullptr) {
      sendError(writer, "no_memory");
      return;
    }
    while (offset < paramsSize) {
      memset(chunk, 0, sizeof(*chunk));
      uint8_t chunkSize =
          paramsSize - offset > SUPLA_CALCFG_SUPLET_INSTANCE_CHUNK_MAXSIZE
              ? SUPLA_CALCFG_SUPLET_INSTANCE_CHUNK_MAXSIZE
              : static_cast<uint8_t>(paramsSize - offset);
      chunk->SessionId = sessionId;
      chunk->Offset = offset;
      chunk->Size = chunkSize;
      memcpy(chunk->Data, paramsJson + offset, chunkSize);
      uint32_t payloadSize =
          offsetof(TCalCfg_SupletInstanceChunk, Data) + chunkSize;
      if (sendLocalCalcfg(SUPLA_CALCFG_CMD_SUPLET_INSTANCE_CHUNK,
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
    TCalCfg_SupletSessionRequest commit = {};
    bool ok = sendLocalCalcfg(SUPLA_CALCFG_CMD_SUPLET_INSTANCE_COMMIT,
                              &commit,
                              sizeof(commit),
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

  if (equalText(command.operation, "upgradeInstance")) {
    if (command.instanceId == 0 || command.instanceId > UINT8_MAX ||
        command.definitionId == 0 || command.fromDefinitionVersion == 0 ||
        command.toDefinitionVersion == 0 ||
        command.fromDefinitionVersion > UINT16_MAX ||
        command.toDefinitionVersion > UINT16_MAX) {
      sendError(writer, "invalid_arguments");
      return;
    }
    const char *paramsJson = command.paramsJson ? command.paramsJson : "{}";
    size_t paramsSize = strlen(paramsJson);
    if (paramsSize > UINT16_MAX || paramsSize > SUPLA_SUPLET_MAX_CONFIG_SIZE) {
      sendError(writer, "params_too_large");
      return;
    }
    uint32_t sessionId =
        command.sessionId == 0 ? nextSessionId() : command.sessionId;
    uint8_t sha256[32] = {};
    calculateSha256(paramsJson, static_cast<uint16_t>(paramsSize), sha256);
    TCalCfg_SupletInstanceUpgradeBegin begin = {};
    begin.SessionId = sessionId;
    begin.InstanceId = command.instanceId;
    begin.DefinitionId = command.definitionId;
    begin.FromDefinitionVersion = command.fromDefinitionVersion;
    begin.ToDefinitionVersion = command.toDefinitionVersion;
    begin.ParamsSize = static_cast<uint16_t>(paramsSize);
    memcpy(begin.ParamsSha256, sha256, sizeof(sha256));
    if (sendLocalCalcfg(SUPLA_CALCFG_CMD_SUPLET_INSTANCE_UPGRADE_BEGIN,
                        &begin,
                        sizeof(begin),
                        "instance.upgrade.begin") != SUPLA_CALCFG_RESULT_DONE) {
      sendDone(writer, false);
      return;
    }
    uint16_t offset = 0;
    auto *chunk = new TCalCfg_SupletInstanceChunk;
    if (chunk == nullptr) {
      sendError(writer, "no_memory");
      return;
    }
    while (offset < paramsSize) {
      memset(chunk, 0, sizeof(*chunk));
      uint8_t chunkSize =
          paramsSize - offset > SUPLA_CALCFG_SUPLET_INSTANCE_CHUNK_MAXSIZE
              ? SUPLA_CALCFG_SUPLET_INSTANCE_CHUNK_MAXSIZE
              : static_cast<uint8_t>(paramsSize - offset);
      chunk->SessionId = sessionId;
      chunk->Offset = offset;
      chunk->Size = chunkSize;
      memcpy(chunk->Data, paramsJson + offset, chunkSize);
      uint32_t payloadSize =
          offsetof(TCalCfg_SupletInstanceChunk, Data) + chunkSize;
      if (sendLocalCalcfg(SUPLA_CALCFG_CMD_SUPLET_INSTANCE_UPGRADE_CHUNK,
                          chunk,
                          payloadSize,
                          "instance.upgrade.chunk") !=
          SUPLA_CALCFG_RESULT_DONE) {
        delete chunk;
        sendDone(writer, false);
        return;
      }
      offset += chunkSize;
    }
    delete chunk;
    TCalCfg_SupletSessionRequest commit = {};
    bool ok = sendLocalCalcfg(SUPLA_CALCFG_CMD_SUPLET_INSTANCE_UPGRADE_COMMIT,
                              &commit,
                              sizeof(commit),
                              "instance.upgrade.commit") ==
        SUPLA_CALCFG_RESULT_DONE;
    sendDone(writer, ok);
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
               "\"definitionVersionMin\":%u,\"definitionVersionMax\":%u}",
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
               item.MaxDefinitionVersion);
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
               "%s{\"source\":%u,\"category\":%u,\"kind\":%u,"
               "\"schema\":%u,\"handler\":%u,\"maxInstances\":%u,"
               "\"definitionId\":%u,\"definitionVersion\":%u,"
               "\"jsonSize\":%u}",
               i == 0 ? "" : ",",
               item.Source,
               item.Category,
               item.Kind,
               item.SchemaVersion,
               item.HandlerVersion,
               item.MaxInstances,
               static_cast<unsigned>(item.DefinitionId),
               item.DefinitionVersion,
               item.JsonSize);
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
      request.MaxSize = SUPLA_CALCFG_SUPLET_CONFIG_CHUNK_MAXSIZE;
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
      if (chunk->DefinitionId != command.definitionId ||
          chunk->DefinitionVersion != command.definitionVersion ||
          result->DataSize !=
              offsetof(TCalCfg_SupletDefinitionConfigChunk, Data) +
                  chunk->Size ||
          chunk->TotalSize > SUPLA_SUPLET_MAX_DEFINITION_JSON_SIZE ||
          offset + chunk->Size > SUPLA_SUPLET_MAX_DEFINITION_JSON_SIZE) {
        delete result;
        delete[] definitionJson;
        sendError(writer, "malformed_definition_chunk");
        return;
      }
      if (offset == 0) {
        totalSize = chunk->TotalSize;
        source = chunk->Source;
      }
      memcpy(definitionJson + offset, chunk->Data, chunk->Size);
      offset += chunk->Size;
      delete result;
      if (offset >= chunk->TotalSize || chunk->Size == 0) {
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

  if (equalText(command.operation, "getInstanceCount")) {
    TDS_DeviceCalCfgResult *result = nullptr;
    if (sendLocalCalcfg(SUPLA_CALCFG_CMD_SUPLET_GET_INSTANCE_COUNT,
                        nullptr,
                        0,
                        "instance.count",
                        &result) != SUPLA_CALCFG_RESULT_TRUE ||
        result == nullptr ||
        result->DataSize != sizeof(TCalCfg_SupletInstanceCount)) {
      delete result;
      return;
    }
    auto *count =
        reinterpret_cast<TCalCfg_SupletInstanceCount *>(result->Data);
    char line[192] = {};
    snprintf(line,
             sizeof(line),
             "{\"op\":\"instance.count.data\",\"count\":%u,"
             "\"maxInstances\":%u,\"maxChannelsPerInstance\":%u,"
             "\"maxCachedDefinitions\":%u}\n",
             count->Count,
             count->MaxInstances,
             count->MaxChannelsPerInstance,
             count->MaxCachedDefinitions);
    sendText(writer, line);
    delete result;
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
      char line[192] = {};
      snprintf(line,
               sizeof(line),
               "%s{\"instanceId\":%u,\"definitionId\":%u,"
               "\"definitionVersion\":%u,\"subDeviceId\":%u,"
               "\"channelCount\":%u}",
               i == 0 ? "" : ",",
               item.InstanceId,
               static_cast<unsigned>(item.DefinitionId),
               item.DefinitionVersion,
               item.SubDeviceId,
               item.ChannelCount);
      sendText(writer, line);
    }
    sendText(writer, "]}\n");
    delete result;
    return;
  }

  if (equalText(command.operation, "getInstanceInfo")) {
    if (command.instanceId == 0 || command.instanceId > UINT8_MAX) {
      sendError(writer, "invalid_arguments");
      return;
    }
    TCalCfg_SupletInstanceRequest request = {};
    request.InstanceId = command.instanceId;
    TDS_DeviceCalCfgResult *result = nullptr;
    if (sendLocalCalcfg(SUPLA_CALCFG_CMD_SUPLET_GET_INSTANCE_INFO,
                        &request,
                        sizeof(request),
                        "instance.info",
                        &result) != SUPLA_CALCFG_RESULT_TRUE ||
        result == nullptr ||
        result->DataSize != sizeof(TCalCfg_SupletInstanceInfo)) {
      delete result;
      return;
    }
    auto *info = reinterpret_cast<TCalCfg_SupletInstanceInfo *>(result->Data);
    char line[256] = {};
    snprintf(line,
             sizeof(line),
             "{\"op\":\"instance.info.data\",\"instanceId\":%u,"
             "\"definitionId\":%u,\"definitionVersion\":%u,"
             "\"subDeviceId\":%u,\"channelCount\":%u,\"paramsSize\":%u}\n",
             info->InstanceId,
             static_cast<unsigned>(info->DefinitionId),
             info->DefinitionVersion,
             info->SubDeviceId,
             info->ChannelCount,
             info->ParamsSize);
    sendText(writer, line);
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
      TCalCfg_SupletInstanceConfigRequest request = {};
      request.InstanceId = command.instanceId;
      request.Offset = offset;
      request.MaxSize = SUPLA_CALCFG_SUPLET_CONFIG_CHUNK_MAXSIZE;
      TDS_DeviceCalCfgResult *result = nullptr;
      if (sendLocalCalcfg(SUPLA_CALCFG_CMD_SUPLET_GET_INSTANCE_CONFIG,
                          &request,
                          sizeof(request),
                          "instance.config",
                          &result) != SUPLA_CALCFG_RESULT_TRUE ||
          result == nullptr ||
          result->DataSize <
              offsetof(TCalCfg_SupletInstanceConfigChunk, Data)) {
        delete result;
        delete[] config;
        return;
      }
      auto *chunk =
          reinterpret_cast<TCalCfg_SupletInstanceConfigChunk *>(result->Data);
      if (chunk->InstanceId != command.instanceId ||
          result->DataSize !=
              offsetof(TCalCfg_SupletInstanceConfigChunk, Data) +
                  chunk->Size ||
          chunk->TotalSize > SUPLA_SUPLET_MAX_CONFIG_SIZE ||
          offset + chunk->Size > SUPLA_SUPLET_MAX_CONFIG_SIZE) {
        delete result;
        delete[] config;
        sendError(writer, "malformed_config_chunk");
        return;
      }
      if (offset == 0) {
        totalSize = chunk->TotalSize;
      }
      memcpy(config + offset, chunk->Data, chunk->Size);
      offset += chunk->Size;
      delete result;
      if (offset >= chunk->TotalSize || chunk->Size == 0) {
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

uint32_t CommandProcessor::nextSessionId() {
  if (sessionCounter == 0) {
    sessionCounter = 1;
  }
  return sessionCounter++;
}

}  // namespace Debug
}  // namespace Supla

#endif  // SUPLA_INSECURE_DEBUG_INTERFACE

/*
   Copyright (C) AC SOFTWARE SP. Z O.O

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

#include <supla/suplet/config.h>
#include <supla/suplet/runtime.h>

#if SUPLA_SUPLET_ENABLED

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <supla/suplet/json_instance_config.h>
#include <supla/suplet/server_config.h>

namespace {

class JsonCommandReader {
 public:
  explicit JsonCommandReader(const char *json) : pos(json) {
  }

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

  bool consumeLiteral(const char *literal) {
    skipWhitespace();
    size_t len = strlen(literal);
    if (pos == nullptr || strncmp(pos, literal, len) != 0) {
      return false;
    }
    pos += len;
    return true;
  }

  bool nextIs(char expected) {
    skipWhitespace();
    return pos != nullptr && *pos == expected;
  }

  bool readString(char *output, size_t outputSize) {
    skipWhitespace();
    if (pos == nullptr || output == nullptr || outputSize == 0 || *pos != '"') {
      return false;
    }
    pos++;
    size_t index = 0;
    while (*pos != '\0' && *pos != '"') {
      char value = *pos++;
      if (value == '\\') {
        value = *pos++;
        if (value == '\0') {
          return false;
        }
        switch (value) {
          case '"':
          case '\\':
          case '/':
            break;
          case 'b':
            value = '\b';
            break;
          case 'f':
            value = '\f';
            break;
          case 'n':
            value = '\n';
            break;
          case 'r':
            value = '\r';
            break;
          case 't':
            value = '\t';
            break;
          default:
            return false;
        }
      }
      if (index + 1 >= outputSize) {
        return false;
      }
      output[index++] = value;
    }
    if (*pos != '"') {
      return false;
    }
    pos++;
    output[index] = '\0';
    return true;
  }

  bool skipString() {
    skipWhitespace();
    if (pos == nullptr || *pos != '"') {
      return false;
    }
    pos++;
    while (*pos != '\0' && *pos != '"') {
      if (*pos == '\\') {
        pos++;
        if (*pos == '\0') {
          return false;
        }
      }
      pos++;
    }
    if (*pos != '"') {
      return false;
    }
    pos++;
    return true;
  }

  bool readUInt32(uint32_t *value) {
    skipWhitespace();
    if (pos == nullptr || value == nullptr || *pos < '0' || *pos > '9') {
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

  bool readInt32(int32_t *value) {
    skipWhitespace();
    if (pos == nullptr || value == nullptr) {
      return false;
    }
    bool negative = false;
    if (*pos == '-') {
      negative = true;
      pos++;
    }
    uint32_t unsignedValue = 0;
    if (!readUInt32(&unsignedValue)) {
      return false;
    }
    if ((!negative && unsignedValue > static_cast<uint32_t>(INT32_MAX)) ||
        (negative && unsignedValue > static_cast<uint32_t>(INT32_MAX) + 1)) {
      return false;
    }
    *value = negative ? -static_cast<int32_t>(unsignedValue)
                      : static_cast<int32_t>(unsignedValue);
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

  bool skipNumber() {
    skipWhitespace();
    if (pos == nullptr) {
      return false;
    }
    if (*pos == '-') {
      pos++;
    }
    if (*pos < '0' || *pos > '9') {
      return false;
    }
    while (*pos >= '0' && *pos <= '9') {
      pos++;
    }
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

bool equalText(const char *a, const char *b) {
  return a != nullptr && b != nullptr && strcmp(a, b) == 0;
}

class ScopedJsonDefinition {
 public:
  ScopedJsonDefinition() = default;
  ScopedJsonDefinition(const ScopedJsonDefinition &) = delete;
  ScopedJsonDefinition &operator=(const ScopedJsonDefinition &) = delete;

  ~ScopedJsonDefinition() {
    delete definition;
  }

  bool allocate() {
    if (definition == nullptr) {
      definition = new Supla::Suplet::JsonDefinition();
    }
    return definition != nullptr;
  }

  Supla::Suplet::JsonDefinition *get() {
    return definition;
  }

 private:
  Supla::Suplet::JsonDefinition *definition = nullptr;
};

struct Command {
  Command() = default;
  Command(const Command &) = delete;
  Command &operator=(const Command &) = delete;

  ~Command() {
    clear();
  }

  void clear() {
    if (definitionJson != nullptr) {
      delete[] definitionJson;
      definitionJson = nullptr;
    }
    operation[0] = '\0';
    instanceId = 0;
    definitionId = 0;
    definitionVersion = 0;
    sha256Hex[0] = '\0';
  }

  char operation[20] = {};
  uint32_t instanceId = 0;
  uint32_t definitionId = 0;
  uint32_t definitionVersion = 0;
  char sha256Hex[65] = {};
  char *definitionJson = nullptr;
};

bool parseCommand(const char *json, Command *command) {
  if (json == nullptr || command == nullptr) {
    return false;
  }

  command->clear();
  JsonCommandReader reader(json);
  if (!reader.consume('{')) {
    return false;
  }

  reader.skipWhitespace();
  if (reader.consume('}')) {
    return false;
  }

  while (true) {
    char key[24] = {};
    if (!reader.readString(key, sizeof(key)) || !reader.consume(':')) {
      return false;
    }

    if (equalText(key, "op") || equalText(key, "operation")) {
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
    } else if (equalText(key, "sha256")) {
      if (!reader.readString(command->sha256Hex, sizeof(command->sha256Hex))) {
        return false;
      }
    } else if (equalText(key, "definitionJson")) {
      if (command->definitionJson != nullptr) {
        return false;
      }
      command->definitionJson =
          new char[SUPLA_SUPLET_MAX_DEFINITION_JSON_SIZE + 1];
      if (command->definitionJson == nullptr ||
          !reader.readString(command->definitionJson,
                             SUPLA_SUPLET_MAX_DEFINITION_JSON_SIZE + 1)) {
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

int hexNibble(char value) {
  if (value >= '0' && value <= '9') {
    return value - '0';
  }
  if (value >= 'a' && value <= 'f') {
    return value - 'a' + 10;
  }
  if (value >= 'A' && value <= 'F') {
    return value - 'A' + 10;
  }
  return -1;
}

bool parseSha256(const char *hex, uint8_t *sha256) {
  if (hex == nullptr || sha256 == nullptr || strlen(hex) != 64) {
    return false;
  }
  for (uint8_t i = 0; i < 32; i++) {
    int high = hexNibble(hex[i * 2]);
    int low = hexNibble(hex[i * 2 + 1]);
    if (high < 0 || low < 0) {
      return false;
    }
    sha256[i] = static_cast<uint8_t>((high << 4) | low);
  }
  return true;
}

const Supla::Suplet::ParameterDefinition *findParameter(
    const Supla::Suplet::Definition &definition, const char *key) {
  if (key == nullptr) {
    return nullptr;
  }
  for (uint8_t i = 0; i < definition.parameterCount; i++) {
    const auto &parameter = definition.parameters[i];
    if (parameter.key != nullptr && strcmp(parameter.key, key) == 0) {
      return &parameter;
    }
  }
  return nullptr;
}

bool enumContains(const char *csv, const char *value) {
  if (csv == nullptr || value == nullptr || value[0] == '\0') {
    return false;
  }
  size_t valueLen = strlen(value);
  const char *cursor = csv;
  while (*cursor != '\0') {
    const char *end = strchr(cursor, ',');
    size_t len =
        end == nullptr ? strlen(cursor) : static_cast<size_t>(end - cursor);
    if (len == valueLen && strncmp(cursor, value, len) == 0) {
      return true;
    }
    if (end == nullptr) {
      break;
    }
    cursor = end + 1;
  }
  return false;
}

bool appendText(char *output, size_t outputSize, const char *text) {
  if (output == nullptr || outputSize == 0 || text == nullptr) {
    return false;
  }
  size_t currentSize = strlen(output);
  size_t textSize = strlen(text);
  if (currentSize + textSize + 1 > outputSize) {
    return false;
  }
  memcpy(output + currentSize, text, textSize + 1);
  return true;
}

bool appendNumber(char *output, size_t outputSize, uint32_t value) {
  char buffer[12] = {};
  snprintf(buffer, sizeof(buffer), "%u", static_cast<unsigned>(value));
  return appendText(output, outputSize, buffer);
}

bool readChannelListNormalized(JsonCommandReader *reader,
                               char *output,
                               size_t outputSize) {
  if (output == nullptr || outputSize == 0) {
    return false;
  }
  output[0] = '\0';
  if (!reader->consume('[')) {
    return false;
  }
  reader->skipWhitespace();
  if (reader->consume(']')) {
    return true;
  }
  bool first = true;
  while (true) {
    uint32_t channel = 0;
    if (!reader->readUInt32(&channel) || channel >= SUPLA_CHANNELMAXCOUNT) {
      return false;
    }
    if (!first && !appendText(output, outputSize, ",")) {
      return false;
    }
    if (!appendNumber(output, outputSize, channel)) {
      return false;
    }
    first = false;
    reader->skipWhitespace();
    if (reader->consume(']')) {
      return true;
    }
    if (!reader->consume(',')) {
      return false;
    }
  }
}

bool writeDefaultParameterValueNormalized(
    const Supla::Suplet::ParameterDefinition &parameter,
    char *output,
    size_t outputSize) {
  if (output == nullptr || outputSize == 0 || !parameter.hasDefault) {
    return false;
  }
  output[0] = '\0';
  switch (parameter.type) {
    case Supla::Suplet::ParameterType::Bool:
      return appendText(
          output, outputSize, parameter.defaultNumber == 0 ? "false" : "true");
    case Supla::Suplet::ParameterType::UInt8:
    case Supla::Suplet::ParameterType::UInt16:
    case Supla::Suplet::ParameterType::Int16: {
      char buffer[16] = {};
      snprintf(buffer, sizeof(buffer), "%" PRId32, parameter.defaultNumber);
      return appendText(output, outputSize, buffer);
    }
    case Supla::Suplet::ParameterType::String:
    case Supla::Suplet::ParameterType::Secret:
    case Supla::Suplet::ParameterType::Enum:
      return appendText(
          output,
          outputSize,
          parameter.defaultText == nullptr ? "" : parameter.defaultText);
    default:
      return false;
  }
}

bool readParameterValueNormalized(
    JsonCommandReader *reader,
    const Supla::Suplet::ParameterDefinition &parameter,
    char *output,
    size_t outputSize) {
  if (reader == nullptr || output == nullptr || outputSize == 0) {
    return false;
  }
  output[0] = '\0';
  switch (parameter.type) {
    case Supla::Suplet::ParameterType::Bool: {
      bool value = false;
      return reader->readBool(&value) &&
             appendText(output, outputSize, value ? "true" : "false");
    }
    case Supla::Suplet::ParameterType::UInt8:
    case Supla::Suplet::ParameterType::UInt16:
    case Supla::Suplet::ParameterType::Int16: {
      int32_t value = 0;
      if (!reader->readInt32(&value) || value < parameter.min ||
          value > parameter.max) {
        return false;
      }
      if (parameter.type == Supla::Suplet::ParameterType::UInt8 &&
          (value < 0 || value > UINT8_MAX)) {
        return false;
      }
      if (parameter.type == Supla::Suplet::ParameterType::UInt16 &&
          (value < 0 || value > UINT16_MAX)) {
        return false;
      }
      if (parameter.type == Supla::Suplet::ParameterType::Int16 &&
          (value < INT16_MIN || value > INT16_MAX)) {
        return false;
      }
      char buffer[16] = {};
      snprintf(buffer, sizeof(buffer), "%" PRId32, value);
      return appendText(output, outputSize, buffer);
    }
    case Supla::Suplet::ParameterType::String:
    case Supla::Suplet::ParameterType::Secret: {
      return reader->readString(output, outputSize);
    }
    case Supla::Suplet::ParameterType::Enum: {
      return reader->readString(output, outputSize) &&
             enumContains(parameter.enumValues, output);
    }
    case Supla::Suplet::ParameterType::ChannelList:
      return readChannelListNormalized(reader, output, outputSize);
    case Supla::Suplet::ParameterType::Unknown:
    default:
      return false;
  }
}

bool validateParameterValue(
    JsonCommandReader *reader,
    const Supla::Suplet::ParameterDefinition &parameter) {
  char *value = new char[SUPLA_SUPLET_MAX_CONFIG_SIZE + 1];
  if (value == nullptr) {
    return false;
  }
  value[0] = '\0';
  bool result = readParameterValueNormalized(
      reader, parameter, value, SUPLA_SUPLET_MAX_CONFIG_SIZE + 1);
  delete[] value;
  return result;
}

bool getParameterValueNormalized(
    const char *json,
    uint16_t jsonSize,
    const Supla::Suplet::ParameterDefinition &parameter,
    char *output,
    size_t outputSize) {
  if (output == nullptr || outputSize == 0 || parameter.key == nullptr ||
      jsonSize > SUPLA_SUPLET_MAX_CONFIG_SIZE) {
    return false;
  }
  output[0] = '\0';
  if (jsonSize == 0) {
    return writeDefaultParameterValueNormalized(parameter, output, outputSize);
  }
  if (json == nullptr) {
    return false;
  }
  char *jsonCopy = new char[static_cast<size_t>(jsonSize) + 1];
  if (jsonCopy == nullptr) {
    return false;
  }
  memcpy(jsonCopy, json, jsonSize);
  jsonCopy[jsonSize] = '\0';

  JsonCommandReader reader(jsonCopy);
  if (!reader.consume('{')) {
    delete[] jsonCopy;
    return false;
  }
  reader.skipWhitespace();
  if (reader.consume('}')) {
    bool result =
        writeDefaultParameterValueNormalized(parameter, output, outputSize);
    delete[] jsonCopy;
    return result;
  }

  bool found = false;
  while (true) {
    char key[SUPLA_SUPLET_MAX_PARAMETER_KEY_SIZE] = {};
    if (!reader.readString(key, sizeof(key)) || !reader.consume(':')) {
      delete[] jsonCopy;
      return false;
    }
    if (strcmp(key, parameter.key) == 0) {
      if (!readParameterValueNormalized(
              &reader, parameter, output, outputSize)) {
        delete[] jsonCopy;
        return false;
      }
      found = true;
    } else if (!reader.skipValue()) {
      delete[] jsonCopy;
      return false;
    }
    reader.skipWhitespace();
    if (reader.consume('}')) {
      bool result = found || writeDefaultParameterValueNormalized(
                                 parameter, output, outputSize);
      delete[] jsonCopy;
      return result;
    }
    if (!reader.consume(',')) {
      delete[] jsonCopy;
      return false;
    }
  }
}

bool createOnlyParamsChanged(const char *json,
                             uint16_t jsonSize,
                             const Supla::Suplet::Definition &definition,
                             const Supla::Suplet::InstanceRecord *existing) {
  if (existing == nullptr) {
    return false;
  }
  for (uint8_t i = 0; i < definition.parameterCount; i++) {
    const auto &parameter = definition.parameters[i];
    if (parameter.lifecycle != Supla::Suplet::ParameterLifecycle::CreateOnly) {
      continue;
    }
    char *oldValue = new char[SUPLA_SUPLET_MAX_CONFIG_SIZE + 1];
    char *newValue = new char[SUPLA_SUPLET_MAX_CONFIG_SIZE + 1];
    if (oldValue == nullptr || newValue == nullptr) {
      delete[] oldValue;
      delete[] newValue;
      return true;
    }
    oldValue[0] = '\0';
    newValue[0] = '\0';
    if (!getParameterValueNormalized(
            reinterpret_cast<const char *>(existing->config),
            existing->configSize,
            parameter,
            oldValue,
            SUPLA_SUPLET_MAX_CONFIG_SIZE + 1) ||
        !getParameterValueNormalized(json,
                                     jsonSize,
                                     parameter,
                                     newValue,
                                     SUPLA_SUPLET_MAX_CONFIG_SIZE + 1) ||
        strcmp(oldValue, newValue) != 0) {
      delete[] oldValue;
      delete[] newValue;
      return true;
    }
    delete[] oldValue;
    delete[] newValue;
  }
  return false;
}

bool validateParamsJson(const char *json,
                        uint16_t jsonSize,
                        const Supla::Suplet::Definition &definition,
                        const Supla::Suplet::InstanceRecord *existing,
                        bool *createOnlyChanged) {
  if (createOnlyChanged != nullptr) {
    *createOnlyChanged = false;
  }
  if ((json == nullptr && jsonSize > 0) ||
      jsonSize > SUPLA_SUPLET_MAX_CONFIG_SIZE ||
      definition.parameterCount > SUPLA_SUPLET_MAX_PARAMETERS) {
    return false;
  }
  if (jsonSize == 0) {
    for (uint8_t i = 0; i < definition.parameterCount; i++) {
      if (definition.parameters[i].required &&
          !definition.parameters[i].hasDefault) {
        return false;
      }
    }
    return true;
  }

  bool seen[SUPLA_SUPLET_MAX_PARAMETERS] = {};
  JsonCommandReader reader(json);
  if (!reader.consume('{')) {
    return false;
  }
  reader.skipWhitespace();
  if (reader.consume('}')) {
    for (uint8_t i = 0; i < definition.parameterCount; i++) {
      if (definition.parameters[i].required &&
          !definition.parameters[i].hasDefault) {
        return false;
      }
    }
    return true;
  }

  while (true) {
    char key[SUPLA_SUPLET_MAX_PARAMETER_KEY_SIZE] = {};
    if (!reader.readString(key, sizeof(key)) || !reader.consume(':')) {
      return false;
    }

    const auto *parameter = findParameter(definition, key);
    if (parameter == nullptr) {
      return false;
    }
    uint8_t parameterIndex =
        static_cast<uint8_t>(parameter - definition.parameters);
    seen[parameterIndex] = true;
    if (!validateParameterValue(&reader, *parameter)) {
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

  if (!reader.atEnd()) {
    return false;
  }

  for (uint8_t i = 0; i < definition.parameterCount; i++) {
    if (!seen[i] && definition.parameters[i].required &&
        !definition.parameters[i].hasDefault) {
      return false;
    }
  }

  if (existing != nullptr &&
      createOnlyParamsChanged(json, jsonSize, definition, existing)) {
    if (createOnlyChanged != nullptr) {
      *createOnlyChanged = true;
    }
    return false;
  }

  return true;
}

const Supla::Suplet::InstanceRecord *loadExistingConfigIfNeeded(
    Supla::Suplet::Manager *manager,
    const Supla::Suplet::InstanceRecord *existing,
    Supla::Suplet::InstanceRecord *loaded) {
  if (manager == nullptr || existing == nullptr || loaded == nullptr ||
      existing->config != nullptr || existing->configSize == 0) {
    return existing;
  }
  if (!manager->loadInstance(existing->instanceId, loaded)) {
    return nullptr;
  }
  return loaded;
}

uint8_t countInstancesForDefinition(const Supla::Suplet::Manager *manager,
                                    const Supla::Suplet::InstanceRecord &record,
                                    uint32_t definitionId,
                                    uint16_t definitionVersion) {
  if (manager == nullptr || record.instanceId == 0) {
    return UINT8_MAX;
  }

  const auto *table = manager->getInstanceTable();
  if (table == nullptr) {
    return UINT8_MAX;
  }

  uint8_t count = 0;
  for (uint8_t i = 0; i < table->getCount(); i++) {
    const auto *existing = table->getRecord(i);
    if (existing != nullptr && existing->instanceId != record.instanceId &&
        existing->definitionId == definitionId &&
        existing->definitionVersion == definitionVersion) {
      count++;
    }
  }
  return count;
}

bool isInstanceLimitAvailable(const Supla::Suplet::Manager *manager,
                              const Supla::Suplet::InstanceRecord &record,
                              uint32_t definitionId,
                              uint16_t definitionVersion,
                              uint8_t maxInstances) {
  if (maxInstances == 0) {
    return false;
  }

  uint8_t count = countInstancesForDefinition(
      manager, record, definitionId, definitionVersion);
  if (count == UINT8_MAX) {
    return false;
  }

  return count < maxInstances;
}

const Supla::Suplet::Definition *findDefinitionOnDemand(
    const Supla::Suplet::Registry *registry,
    const Supla::Suplet::DownloadedDefinitionStore *downloadedDefinitions,
    const Supla::Suplet::DefinitionCache *definitionCache,
    uint32_t definitionId,
    uint16_t definitionVersion,
    ScopedJsonDefinition *downloadedDefinition,
    bool *ramError,
    uint8_t *maxInstances) {
  if (maxInstances != nullptr) {
    *maxInstances = 0;
  }
  if (ramError != nullptr) {
    *ramError = false;
  }

  if (registry != nullptr) {
    const auto *definition =
        registry->findDefinition(definitionId, definitionVersion);
    if (definition != nullptr) {
      Supla::Suplet::Capability capability = {};
      if (maxInstances != nullptr &&
          registry->getCapability(
              definitionId, definitionVersion, &capability)) {
        *maxInstances = capability.maxInstances;
      }
      return definition;
    }
  }

  if (downloadedDefinitions == nullptr || definitionCache == nullptr ||
      downloadedDefinition == nullptr) {
    return nullptr;
  }
  if (!downloadedDefinition->allocate()) {
    if (ramError != nullptr) {
      *ramError = true;
    }
    return nullptr;
  }
  if (downloadedDefinition->get() == nullptr ||
      !downloadedDefinitions->load(*definitionCache,
                                   definitionId,
                                   definitionVersion,
                                   downloadedDefinition->get())) {
    return nullptr;
  }

  const auto *definition = downloadedDefinition->get()->getDefinition();
  if (maxInstances != nullptr && definition != nullptr) {
    *maxInstances = definition->maxInstances;
  }
  return definition;
}

bool isDefinitionUsed(const Supla::Suplet::Manager *manager,
                      uint32_t definitionId,
                      uint16_t definitionVersion) {
  if (manager == nullptr || definitionId == 0 || definitionVersion == 0) {
    return false;
  }
  const auto *table = manager->getInstanceTable();
  if (table == nullptr) {
    return false;
  }
  for (uint8_t i = 0; i < table->getCount(); i++) {
    const auto *record = table->getRecord(i);
    if (record != nullptr && record->definitionId == definitionId &&
        record->definitionVersion == definitionVersion) {
      return true;
    }
  }
  return false;
}

}  // namespace

namespace Supla {
namespace Suplet {

bool DownloadedDefinitionStore::load(const DefinitionCache &cache,
                                     uint32_t definitionId,
                                     uint16_t definitionVersion,
                                     JsonDefinition *definition,
                                     CachedDefinitionInfo *info) const {
  if (definition == nullptr || definitionId == 0 || definitionVersion == 0) {
    return false;
  }

  CachedDefinitionInfo loadedInfo = {};
  CachedDefinitionInfo *infoOutput = info == nullptr ? &loadedInfo : info;
  uint16_t jsonSize = 0;
  for (uint8_t i = 0; i < SUPLA_SUPLET_MAX_CACHED_DEFINITIONS; i++) {
    CachedDefinitionInfo current = {};
    if (!cache.getInfo(i, &current) || current.definitionId != definitionId ||
        current.definitionVersion != definitionVersion) {
      continue;
    }
    jsonSize = current.jsonSize;
    *infoOutput = current;
    break;
  }
  if (jsonSize == 0 || jsonSize > SUPLA_SUPLET_MAX_DEFINITION_JSON_SIZE) {
    return false;
  }

  char *json = new char[static_cast<size_t>(jsonSize) + 1];
  if (json == nullptr) {
    return false;
  }

  bool result = cache.load(definitionId,
                           definitionVersion,
                           json,
                           static_cast<size_t>(jsonSize) + 1,
                           infoOutput) &&
                JsonDefinitionParser::parse(json, definition) &&
                Runtime::validateDefinition(*definition->getDefinition());
  delete[] json;
  return result;
}

uint8_t DownloadedDefinitionStore::getCount(
    const DefinitionCache &cache) const {
  uint8_t count = 0;
  for (uint8_t i = 0; i < SUPLA_SUPLET_MAX_CACHED_DEFINITIONS; i++) {
    CachedDefinitionInfo info = {};
    if (cache.getInfo(i, &info)) {
      count++;
    }
  }
  return count;
}

bool ServerConfigHandler::loadDownloadedDefinition(
    uint32_t definitionId,
    uint16_t definitionVersion,
    JsonDefinition *definition,
    CachedDefinitionInfo *info) const {
  return definitionCache != nullptr && downloadedDefinitions != nullptr &&
         downloadedDefinitions->load(*definitionCache,
                                     definitionId,
                                     definitionVersion,
                                     definition,
                                     info);
}

ServerConfigResult ServerConfigHandler::loadDownloadedDefinitions() {
  if (definitionCache == nullptr || downloadedDefinitions == nullptr) {
    return ServerConfigResult::InvalidArgument;
  }

  for (uint8_t i = 0; i < SUPLA_SUPLET_MAX_CACHED_DEFINITIONS; i++) {
    CachedDefinitionInfo info = {};
    if (!definitionCache->getInfo(i, &info)) {
      continue;
    }
    ScopedJsonDefinition definition;
    if (!definition.allocate()) {
      return ServerConfigResult::StorageError;
    }
    if (!downloadedDefinitions->load(*definitionCache,
                                     info.definitionId,
                                     info.definitionVersion,
                                     definition.get())) {
      return ServerConfigResult::InvalidDefinition;
    }
  }
  return ServerConfigResult::Applied;
}

ServerConfigHandler::ServerConfigHandler(
    Manager *manager,
    Registry *registry,
    DefinitionCache *definitionCache,
    DownloadedDefinitionStore *downloadedDefinitions)
    : manager(manager),
      registry(registry),
      definitionCache(definitionCache),
      downloadedDefinitions(downloadedDefinitions) {
}

ServerConfigResult ServerConfigHandler::saveDownloadedDefinition(
    uint32_t definitionId,
    uint16_t definitionVersion,
    const char *definitionJson,
    const uint8_t *sha256) {
  if (definitionCache == nullptr || downloadedDefinitions == nullptr ||
      definitionJson == nullptr || sha256 == nullptr || definitionId == 0 ||
      definitionVersion == 0) {
    return ServerConfigResult::InvalidArgument;
  }

  ScopedJsonDefinition parsed;
  if (!parsed.allocate()) {
    return ServerConfigResult::StorageError;
  }
  if (!JsonDefinitionParser::parse(definitionJson, parsed.get()) ||
      parsed.get()->getDefinition()->definitionId != definitionId ||
      parsed.get()->getDefinition()->definitionVersion != definitionVersion ||
      !Runtime::validateDefinition(*parsed.get()->getDefinition())) {
    return ServerConfigResult::InvalidDefinition;
  }

  if (definitionCache->contains(definitionId, definitionVersion)) {
    CachedDefinitionInfo info = {};
    bool foundInfo = false;
    for (uint8_t i = 0; i < SUPLA_SUPLET_MAX_CACHED_DEFINITIONS; i++) {
      CachedDefinitionInfo current = {};
      if (definitionCache->getInfo(i, &current) &&
          current.definitionId == definitionId &&
          current.definitionVersion == definitionVersion) {
        info = current;
        foundInfo = true;
        break;
      }
    }
    if (!foundInfo) {
      return ServerConfigResult::StorageError;
    }
    if (memcmp(info.sha256, sha256, sizeof(info.sha256)) != 0) {
      if (manager == nullptr || manager->getInstanceTable() == nullptr) {
        return ServerConfigResult::DefinitionCannotBeChanged;
      }
      if (isDefinitionUsed(manager, definitionId, definitionVersion)) {
        return ServerConfigResult::DefinitionCannotBeChanged;
      }
      if (!definitionCache->save(
              definitionId, definitionVersion, definitionJson, sha256)) {
        return ServerConfigResult::StorageError;
      }
      runtimeRefreshRequired = true;
      return ServerConfigResult::Applied;
    }
    runtimeRefreshRequired = true;
    return ServerConfigResult::Applied;
  }

  if (!definitionCache->save(
          definitionId, definitionVersion, definitionJson, sha256)) {
    return ServerConfigResult::StorageError;
  }

  runtimeRefreshRequired = true;
  return ServerConfigResult::Applied;
}

ServerConfigResult ServerConfigHandler::beginStagedDownloadedDefinition(
    uint32_t definitionId,
    uint16_t definitionVersion,
    uint16_t jsonSize,
    const uint8_t *sha256,
    DefinitionCacheHandle *handle) {
  if (definitionCache == nullptr || downloadedDefinitions == nullptr ||
      definitionId == 0 || definitionVersion == 0 || jsonSize == 0 ||
      jsonSize > SUPLA_SUPLET_MAX_DEFINITION_JSON_SIZE || sha256 == nullptr ||
      handle == nullptr) {
    return ServerConfigResult::InvalidArgument;
  }

  if (!definitionCache->beginStagedSave(
          definitionId, definitionVersion, jsonSize, sha256, handle)) {
    return ServerConfigResult::StorageError;
  }
  return ServerConfigResult::Applied;
}

ServerConfigResult ServerConfigHandler::writeStagedDownloadedDefinitionChunk(
    DefinitionCacheHandle handle,
    uint16_t chunkIndex,
    const uint8_t *data,
    uint16_t size) {
  if (definitionCache == nullptr || data == nullptr || size == 0) {
    return ServerConfigResult::InvalidArgument;
  }
  if (!definitionCache->writeStagedChunk(handle, chunkIndex, data, size)) {
    return ServerConfigResult::StorageError;
  }
  return ServerConfigResult::Applied;
}

ServerConfigResult ServerConfigHandler::commitStagedDownloadedDefinition(
    DefinitionCacheHandle handle,
    uint32_t definitionId,
    uint16_t definitionVersion,
    uint16_t jsonSize,
    const uint8_t *sha256) {
  if (definitionCache == nullptr || downloadedDefinitions == nullptr ||
      definitionId == 0 || definitionVersion == 0 || jsonSize == 0 ||
      jsonSize > SUPLA_SUPLET_MAX_DEFINITION_JSON_SIZE || sha256 == nullptr) {
    return ServerConfigResult::InvalidArgument;
  }

  char *definitionJson = new char[static_cast<size_t>(jsonSize) + 1];
  if (definitionJson == nullptr) {
    return ServerConfigResult::StorageError;
  }

  bool loaded = definitionCache->loadStaged(handle,
                                            definitionId,
                                            definitionVersion,
                                            definitionJson,
                                            static_cast<size_t>(jsonSize) + 1);
  if (!loaded) {
    delete[] definitionJson;
    return ServerConfigResult::StorageError;
  }

  ScopedJsonDefinition parsed;
  if (!parsed.allocate()) {
    delete[] definitionJson;
    return ServerConfigResult::StorageError;
  }

  if (!JsonDefinitionParser::parse(definitionJson, parsed.get()) ||
      parsed.get()->getDefinition()->definitionId != definitionId ||
      parsed.get()->getDefinition()->definitionVersion != definitionVersion ||
      !Runtime::validateDefinition(*parsed.get()->getDefinition())) {
    delete[] definitionJson;
    return ServerConfigResult::InvalidDefinition;
  }
  delete[] definitionJson;

  if (definitionCache->contains(definitionId, definitionVersion)) {
    CachedDefinitionInfo info = {};
    bool foundInfo = false;
    for (uint8_t i = 0; i < SUPLA_SUPLET_MAX_CACHED_DEFINITIONS; i++) {
      CachedDefinitionInfo current = {};
      if (definitionCache->getInfo(i, &current) &&
          current.definitionId == definitionId &&
          current.definitionVersion == definitionVersion) {
        info = current;
        foundInfo = true;
        break;
      }
    }
    if (!foundInfo) {
      return ServerConfigResult::StorageError;
    }
    if (memcmp(info.sha256, sha256, sizeof(info.sha256)) != 0) {
      if (manager == nullptr || manager->getInstanceTable() == nullptr ||
          isDefinitionUsed(manager, definitionId, definitionVersion)) {
        return ServerConfigResult::DefinitionCannotBeChanged;
      }
    }
  }

  if (!definitionCache->commitStaged(
          handle, definitionId, definitionVersion, jsonSize, sha256)) {
    return ServerConfigResult::StorageError;
  }

  runtimeRefreshRequired = true;
  return ServerConfigResult::Applied;
}

void ServerConfigHandler::abortStagedDownloadedDefinition(
    DefinitionCacheHandle handle) {
  if (definitionCache != nullptr) {
    definitionCache->abortStaged(handle);
  }
}

ServerConfigResult ServerConfigHandler::removeDownloadedDefinition(
    uint32_t definitionId, uint16_t definitionVersion) {
  if (definitionCache == nullptr || downloadedDefinitions == nullptr ||
      manager == nullptr || definitionId == 0 || definitionVersion == 0) {
    return ServerConfigResult::InvalidArgument;
  }

  if (manager->getInstanceTable() == nullptr) {
    return ServerConfigResult::StorageError;
  }
  if (isDefinitionUsed(manager, definitionId, definitionVersion)) {
    return ServerConfigResult::TopologyChangeNotAllowed;
  }

  if (!definitionCache->contains(definitionId, definitionVersion)) {
    return ServerConfigResult::DefinitionNotFound;
  }
  if (!definitionCache->erase(definitionId, definitionVersion)) {
    return ServerConfigResult::StorageError;
  }

  runtimeRefreshRequired = true;
  return ServerConfigResult::Removed;
}

uint8_t ServerConfigHandler::getCachedDefinitionCount() const {
  if (definitionCache == nullptr) {
    return 0;
  }

  uint8_t count = 0;
  for (uint8_t i = 0; i < SUPLA_SUPLET_MAX_CACHED_DEFINITIONS; i++) {
    CachedDefinitionInfo info = {};
    if (definitionCache->getInfo(i, &info)) {
      count++;
    }
  }
  return count;
}

bool ServerConfigHandler::getCachedDefinitionDetails(
    uint8_t listIndex, CachedDefinitionDetails *details) const {
  if (definitionCache == nullptr || details == nullptr) {
    return false;
  }

  uint8_t currentIndex = 0;
  for (uint8_t i = 0; i < SUPLA_SUPLET_MAX_CACHED_DEFINITIONS; i++) {
    CachedDefinitionInfo info = {};
    if (!definitionCache->getInfo(i, &info)) {
      continue;
    }
    if (currentIndex != listIndex) {
      currentIndex++;
      continue;
    }

    ScopedJsonDefinition parsed;
    if (!parsed.allocate()) {
      return false;
    }
    if (!downloadedDefinitions->load(*definitionCache,
                                     info.definitionId,
                                     info.definitionVersion,
                                     parsed.get(),
                                     &info) ||
        parsed.get()->getDefinition() == nullptr) {
      return false;
    }

    details->cache = info;
    details->category = parsed.get()->getDefinition()->category;
    details->kind = parsed.get()->getDefinition()->kind;
    details->schemaVersion = parsed.get()->getDefinition()->schemaVersion;
    details->handlerVersion = parsed.get()->getDefinition()->handlerVersion;
    details->maxInstances = parsed.get()->getDefinition()->maxInstances;
    return true;
  }

  return false;
}

ServerConfigResult ServerConfigHandler::garbageCollectUnusedDefinitions() {
  if (definitionCache == nullptr || downloadedDefinitions == nullptr) {
    return ServerConfigResult::Applied;
  }
  if (manager == nullptr) {
    return ServerConfigResult::InvalidArgument;
  }
  if (manager->getInstanceTable() == nullptr) {
    return ServerConfigResult::StorageError;
  }

  bool removedAny = false;
  for (uint8_t i = 0; i < SUPLA_SUPLET_MAX_CACHED_DEFINITIONS; i++) {
    CachedDefinitionInfo info = {};
    if (!definitionCache->getInfo(i, &info)) {
      continue;
    }
    if (isDefinitionUsed(manager, info.definitionId, info.definitionVersion)) {
      continue;
    }
    if (!definitionCache->erase(info.definitionId, info.definitionVersion)) {
      return ServerConfigResult::StorageError;
    }
    removedAny = true;
  }

  if (!removedAny) {
    return ServerConfigResult::Applied;
  }

  runtimeRefreshRequired = true;
  return ServerConfigResult::Applied;
}

ServerConfigResult ServerConfigHandler::applyAssignmentJson(
    const char *assignmentJson,
    uint32_t definitionId,
    uint16_t definitionVersion) {
  if (manager == nullptr || assignmentJson == nullptr || definitionId == 0 ||
      definitionVersion == 0) {
    return ServerConfigResult::InvalidArgument;
  }

  ScopedJsonDefinition downloadedDefinition;
  bool ramError = false;
  uint8_t maxInstances = 0;
  const Definition *definition = findDefinitionOnDemand(registry,
                                                        downloadedDefinitions,
                                                        definitionCache,
                                                        definitionId,
                                                        definitionVersion,
                                                        &downloadedDefinition,
                                                        &ramError,
                                                        &maxInstances);
  if (definition == nullptr) {
    if (ramError) {
      return ServerConfigResult::StorageError;
    }
    return ServerConfigResult::DefinitionNotSupported;
  }

  InstanceRecord record = {};
  if (!JsonInstanceConfigParser::parse(assignmentJson, *definition, &record)) {
    return ServerConfigResult::InvalidConfig;
  }

  if (!isInstanceLimitAvailable(
          manager, record, definitionId, definitionVersion, maxInstances)) {
    return ServerConfigResult::InstanceLimitExceeded;
  }

  if (!manager->canUpsertInstanceFromDefinition(record, *definition)) {
    return ServerConfigResult::ChannelLimitExceeded;
  }

  if (!manager->upsertInstanceFromDefinition(record, *definition)) {
    return ServerConfigResult::StorageError;
  }

  runtimeRefreshRequired = true;
  return ServerConfigResult::Applied;
}

ServerConfigResult ServerConfigHandler::applyInstanceParams(
    uint8_t instanceId,
    uint32_t definitionId,
    uint16_t definitionVersion,
    const char *paramsJson,
    uint16_t paramsSize,
    uint8_t *appliedInstanceId) {
  if (appliedInstanceId != nullptr) {
    *appliedInstanceId = 0;
  }
  if (manager == nullptr || definitionId == 0 || definitionVersion == 0 ||
      paramsSize > SUPLA_SUPLET_MAX_CONFIG_SIZE ||
      (paramsSize > 0 && paramsJson == nullptr)) {
    return ServerConfigResult::InvalidArgument;
  }

  if (instanceId == 0) {
    instanceId = manager->getFirstFreeSubDeviceId();
    if (instanceId == 0) {
      return ServerConfigResult::ChannelLimitExceeded;
    }
  }

  auto table = manager->getInstanceTable();
  const InstanceRecord *existing =
      table == nullptr ? nullptr : table->findByInstanceId(instanceId);
  if (existing != nullptr &&
      (existing->definitionId != definitionId ||
       existing->definitionVersion != definitionVersion)) {
    return ServerConfigResult::TopologyChangeNotAllowed;
  }

  ScopedJsonDefinition downloadedDefinition;
  bool ramError = false;
  uint8_t maxInstances = 0;
  const Definition *definition = findDefinitionOnDemand(registry,
                                                        downloadedDefinitions,
                                                        definitionCache,
                                                        definitionId,
                                                        definitionVersion,
                                                        &downloadedDefinition,
                                                        &ramError,
                                                        &maxInstances);
  if (definition == nullptr) {
    if (ramError) {
      return ServerConfigResult::StorageError;
    }
    return ServerConfigResult::DefinitionNotSupported;
  }

  bool createOnlyChanged = false;
  InstanceRecord existingWithConfig = {};
  bool hadExisting = existing != nullptr;
  existing = loadExistingConfigIfNeeded(manager, existing, &existingWithConfig);
  if (hadExisting && existing == nullptr) {
    return ServerConfigResult::StorageError;
  }
  if (!validateParamsJson(
          paramsJson, paramsSize, *definition, existing, &createOnlyChanged)) {
    return createOnlyChanged ? ServerConfigResult::CreateOnlyParamChanged
                             : ServerConfigResult::InvalidConfig;
  }

  InstanceRecord record = {};
  record.instanceId = instanceId;
  record.definitionId = definitionId;
  record.definitionVersion = definitionVersion;
  if (!record.setConfig(reinterpret_cast<const uint8_t *>(paramsJson),
                        paramsSize)) {
    return ServerConfigResult::InvalidArgument;
  }

  if (!isInstanceLimitAvailable(
          manager, record, definitionId, definitionVersion, maxInstances)) {
    return ServerConfigResult::InstanceLimitExceeded;
  }

  if (!manager->canUpsertInstanceFromDefinition(record, *definition)) {
    return ServerConfigResult::ChannelLimitExceeded;
  }

  if (!manager->upsertInstanceFromDefinition(record, *definition)) {
    return ServerConfigResult::StorageError;
  }

  if (appliedInstanceId != nullptr) {
    *appliedInstanceId = record.instanceId;
  }
  runtimeRefreshRequired = true;
  return ServerConfigResult::Applied;
}

ServerConfigResult ServerConfigHandler::validateAssignmentJson(
    const char *assignmentJson,
    uint32_t definitionId,
    uint16_t definitionVersion) const {
  if (manager == nullptr || assignmentJson == nullptr || definitionId == 0 ||
      definitionVersion == 0) {
    return ServerConfigResult::InvalidArgument;
  }

  ScopedJsonDefinition downloadedDefinition;
  bool ramError = false;
  uint8_t maxInstances = 0;
  const Definition *definition = findDefinitionOnDemand(registry,
                                                        downloadedDefinitions,
                                                        definitionCache,
                                                        definitionId,
                                                        definitionVersion,
                                                        &downloadedDefinition,
                                                        &ramError,
                                                        &maxInstances);
  if (definition == nullptr) {
    if (ramError) {
      return ServerConfigResult::StorageError;
    }
    return ServerConfigResult::DefinitionNotSupported;
  }

  InstanceRecord record = {};
  if (!JsonInstanceConfigParser::parse(assignmentJson, *definition, &record)) {
    return ServerConfigResult::InvalidConfig;
  }

  if (!isInstanceLimitAvailable(
          manager, record, definitionId, definitionVersion, maxInstances)) {
    return ServerConfigResult::InstanceLimitExceeded;
  }

  if (!manager->canUpsertInstanceFromDefinition(record, *definition)) {
    return ServerConfigResult::ChannelLimitExceeded;
  }

  return ServerConfigResult::Applied;
}

ServerConfigResult ServerConfigHandler::validateInstanceParams(
    uint8_t instanceId,
    uint32_t definitionId,
    uint16_t definitionVersion,
    const char *paramsJson,
    uint16_t paramsSize) const {
  if (manager == nullptr || definitionId == 0 || definitionVersion == 0 ||
      paramsSize > SUPLA_SUPLET_MAX_CONFIG_SIZE ||
      (paramsSize > 0 && paramsJson == nullptr)) {
    return ServerConfigResult::InvalidArgument;
  }

  if (instanceId == 0) {
    instanceId = manager->getFirstFreeSubDeviceId();
    if (instanceId == 0) {
      return ServerConfigResult::ChannelLimitExceeded;
    }
  }

  auto table = manager->getInstanceTable();
  const InstanceRecord *existing =
      table == nullptr ? nullptr : table->findByInstanceId(instanceId);
  if (existing != nullptr &&
      (existing->definitionId != definitionId ||
       existing->definitionVersion != definitionVersion)) {
    return ServerConfigResult::TopologyChangeNotAllowed;
  }

  ScopedJsonDefinition downloadedDefinition;
  bool ramError = false;
  uint8_t maxInstances = 0;
  const Definition *definition = findDefinitionOnDemand(registry,
                                                        downloadedDefinitions,
                                                        definitionCache,
                                                        definitionId,
                                                        definitionVersion,
                                                        &downloadedDefinition,
                                                        &ramError,
                                                        &maxInstances);
  if (definition == nullptr) {
    if (ramError) {
      return ServerConfigResult::StorageError;
    }
    return ServerConfigResult::DefinitionNotSupported;
  }

  bool createOnlyChanged = false;
  InstanceRecord existingWithConfig = {};
  bool hadExisting = existing != nullptr;
  existing = loadExistingConfigIfNeeded(manager, existing, &existingWithConfig);
  if (hadExisting && existing == nullptr) {
    return ServerConfigResult::StorageError;
  }
  if (!validateParamsJson(
          paramsJson, paramsSize, *definition, existing, &createOnlyChanged)) {
    return createOnlyChanged ? ServerConfigResult::CreateOnlyParamChanged
                             : ServerConfigResult::InvalidConfig;
  }

  InstanceRecord record = {};
  record.instanceId = instanceId;
  record.definitionId = definitionId;
  record.definitionVersion = definitionVersion;
  if (!record.setConfig(reinterpret_cast<const uint8_t *>(paramsJson),
                        paramsSize)) {
    return ServerConfigResult::InvalidArgument;
  }

  if (!isInstanceLimitAvailable(
          manager, record, definitionId, definitionVersion, maxInstances)) {
    return ServerConfigResult::InstanceLimitExceeded;
  }

  if (!manager->canUpsertInstanceFromDefinition(record, *definition)) {
    return ServerConfigResult::ChannelLimitExceeded;
  }

  return ServerConfigResult::Applied;
}

ServerConfigResult ServerConfigHandler::applyCommandJson(
    const char *commandJson) {
  Command command;
  if (!parseCommand(commandJson, &command)) {
    return ServerConfigResult::InvalidArgument;
  }

  if (equalText(command.operation, "upsert")) {
    if (command.instanceId == 0 || command.definitionId == 0 ||
        command.definitionVersion == 0 || command.instanceId > UINT8_MAX ||
        command.definitionVersion > UINT16_MAX) {
      return ServerConfigResult::InvalidArgument;
    }
    return applyAssignmentJson(commandJson,
                               command.definitionId,
                               static_cast<uint16_t>(
                                   command.definitionVersion));
  }

  if (equalText(command.operation, "remove") ||
      equalText(command.operation, "delete")) {
    if (command.instanceId == 0 || command.instanceId > UINT8_MAX) {
      return ServerConfigResult::InvalidArgument;
    }
    return removeAssignment(static_cast<uint8_t>(command.instanceId));
  }

  if (equalText(command.operation, "saveDefinition")) {
    if (command.definitionId == 0 || command.definitionVersion == 0 ||
        command.definitionVersion > UINT16_MAX ||
        command.sha256Hex[0] == '\0' || command.definitionJson == nullptr ||
        command.definitionJson[0] == '\0') {
      return ServerConfigResult::InvalidArgument;
    }

    uint8_t sha256[32] = {};
    if (!parseSha256(command.sha256Hex, sha256)) {
      return ServerConfigResult::InvalidArgument;
    }

    return saveDownloadedDefinition(
        command.definitionId,
        static_cast<uint16_t>(command.definitionVersion),
        command.definitionJson,
        sha256);
  }

  if (equalText(command.operation, "removeDefinition") ||
      equalText(command.operation, "deleteDefinition")) {
    if (command.definitionId == 0 || command.definitionVersion == 0 ||
        command.definitionVersion > UINT16_MAX) {
      return ServerConfigResult::InvalidArgument;
    }
    return removeDownloadedDefinition(
        command.definitionId, static_cast<uint16_t>(command.definitionVersion));
  }

  return ServerConfigResult::InvalidArgument;
}

ServerConfigResult ServerConfigHandler::validateCommandJson(
    const char *commandJson) const {
  Command command;
  if (!parseCommand(commandJson, &command)) {
    return ServerConfigResult::InvalidArgument;
  }

  if (equalText(command.operation, "upsert")) {
    if (command.instanceId == 0 || command.definitionId == 0 ||
        command.definitionVersion == 0 || command.instanceId > UINT8_MAX ||
        command.definitionVersion > UINT16_MAX) {
      return ServerConfigResult::InvalidArgument;
    }
    return validateAssignmentJson(
        commandJson,
        command.definitionId,
        static_cast<uint16_t>(command.definitionVersion));
  }

  if (equalText(command.operation, "remove") ||
      equalText(command.operation, "delete")) {
    return command.instanceId == 0 || command.instanceId > UINT8_MAX
               ? ServerConfigResult::InvalidArgument
               : ServerConfigResult::Removed;
  }

  if (equalText(command.operation, "saveDefinition")) {
    if (command.definitionId == 0 || command.definitionVersion == 0 ||
        command.definitionVersion > UINT16_MAX ||
        command.sha256Hex[0] == '\0' || command.definitionJson == nullptr ||
        command.definitionJson[0] == '\0') {
      return ServerConfigResult::InvalidArgument;
    }
    uint8_t sha256[32] = {};
    if (!parseSha256(command.sha256Hex, sha256)) {
      return ServerConfigResult::InvalidArgument;
    }
    ScopedJsonDefinition parsed;
    if (!parsed.allocate()) {
      return ServerConfigResult::StorageError;
    }
    if (!JsonDefinitionParser::parse(command.definitionJson, parsed.get()) ||
        parsed.get()->getDefinition()->definitionId != command.definitionId ||
        parsed.get()->getDefinition()->definitionVersion !=
            command.definitionVersion ||
        !Runtime::validateDefinition(*parsed.get()->getDefinition())) {
      return ServerConfigResult::InvalidDefinition;
    }
    return ServerConfigResult::Applied;
  }

  if (equalText(command.operation, "removeDefinition") ||
      equalText(command.operation, "deleteDefinition")) {
    return command.definitionId == 0 || command.definitionVersion == 0 ||
                   command.definitionVersion > UINT16_MAX
               ? ServerConfigResult::InvalidArgument
               : ServerConfigResult::Removed;
  }

  return ServerConfigResult::InvalidArgument;
}

ServerConfigResult ServerConfigHandler::removeAssignment(uint8_t instanceId) {
  AssignmentApplier applier(manager, registry);
  ServerConfigResult result = fromAssignmentResult(applier.remove(instanceId));
  if (result == ServerConfigResult::Removed) {
    runtimeRefreshRequired = true;
    ServerConfigResult gcResult = garbageCollectUnusedDefinitions();
    if (gcResult != ServerConfigResult::Applied) {
      return gcResult;
    }
  }
  return result;
}

bool ServerConfigHandler::isRuntimeRefreshRequired() const {
  return runtimeRefreshRequired;
}

void ServerConfigHandler::clearRuntimeRefreshRequired() {
  runtimeRefreshRequired = false;
}

ServerConfigResult ServerConfigHandler::fromAssignmentResult(
    AssignmentResult result) {
  switch (result) {
    case AssignmentResult::Applied:
      return ServerConfigResult::Applied;
    case AssignmentResult::Removed:
      return ServerConfigResult::Removed;
    case AssignmentResult::InvalidArgument:
      return ServerConfigResult::InvalidArgument;
    case AssignmentResult::DefinitionNotSupported:
      return ServerConfigResult::DefinitionNotSupported;
    case AssignmentResult::InvalidConfig:
      return ServerConfigResult::InvalidConfig;
    case AssignmentResult::ResourceLimitExceeded:
    case AssignmentResult::InstanceLimitExceeded:
      return ServerConfigResult::InstanceLimitExceeded;
    case AssignmentResult::ChannelLimitExceeded:
      return ServerConfigResult::ChannelLimitExceeded;
    case AssignmentResult::StorageError:
    default:
      return ServerConfigResult::StorageError;
  }
}

}  // namespace Suplet
}  // namespace Supla

#endif  // SUPLA_SUPLET_ENABLED

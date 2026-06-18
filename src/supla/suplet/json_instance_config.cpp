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

#if SUPLA_SUPLET_ENABLED

#include <supla/suplet/json_instance_config.h>
#include <supla/suplet/runtime.h>
#include <supla/suplet/thermometer_group.h>

#include <string.h>

namespace {

class JsonReader {
 public:
  explicit JsonReader(const char *json) : pos(json) {}

  void skipWhitespace() {
    while (pos != nullptr && (*pos == ' ' || *pos == '\n' || *pos == '\r' ||
                             *pos == '\t')) {
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

  bool readString(char *output, size_t outputSize) {
    skipWhitespace();
    if (pos == nullptr || output == nullptr || outputSize == 0 ||
        *pos != '"') {
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
      int32_t ignored = 0;
      return readInt32(&ignored);
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

bool readUInt8(JsonReader *reader, uint8_t *value) {
  uint32_t tmp = 0;
  if (!reader->readUInt32(&tmp) || tmp > UINT8_MAX) {
    return false;
  }
  *value = static_cast<uint8_t>(tmp);
  return true;
}

bool readUInt16(JsonReader *reader, uint16_t *value) {
  uint32_t tmp = 0;
  if (!reader->readUInt32(&tmp) || tmp > UINT16_MAX) {
    return false;
  }
  *value = static_cast<uint16_t>(tmp);
  return true;
}

bool parseState(const char *value, Supla::Suplet::InstanceState *state) {
  if (state == nullptr) {
    return false;
  }
  if (equalText(value, "active")) {
    *state = Supla::Suplet::InstanceState::Active;
  } else if (equalText(value, "disabled")) {
    *state = Supla::Suplet::InstanceState::Disabled;
  } else if (equalText(value, "staged")) {
    *state = Supla::Suplet::InstanceState::Staged;
  } else if (equalText(value, "deletePending")) {
    *state = Supla::Suplet::InstanceState::DeletePending;
  } else {
    return false;
  }
  return true;
}

bool parseThermometerMode(
    const char *value,
    Supla::Suplet::ThermometerGroupMode *mode) {
  if (mode == nullptr) {
    return false;
  }
  if (equalText(value, "avg")) {
    *mode = Supla::Suplet::ThermometerGroupMode::Avg;
  } else if (equalText(value, "min")) {
    *mode = Supla::Suplet::ThermometerGroupMode::Min;
  } else if (equalText(value, "max")) {
    *mode = Supla::Suplet::ThermometerGroupMode::Max;
  } else {
    return false;
  }
  return true;
}

bool readSourceChannels(JsonReader *reader,
                        Supla::Suplet::ThermometerGroupConfig *config) {
  if (!reader->consume('[')) {
    return false;
  }
  config->sourceCount = 0;
  reader->skipWhitespace();
  if (reader->consume(']')) {
    return false;
  }
  while (true) {
    if (config->sourceCount >=
        SUPLA_SUPLET_THERMOMETER_GROUP_MAX_SOURCES) {
      return false;
    }
    int32_t value = 0;
    if (!reader->readInt32(&value) || value < 0 || value > INT16_MAX) {
      return false;
    }
    config->sourceChannels[config->sourceCount++] =
        static_cast<int16_t>(value);

    reader->skipWhitespace();
    if (reader->consume(']')) {
      return true;
    }
    if (!reader->consume(',')) {
      return false;
    }
  }
}

bool readThermometerGroupConfig(JsonReader *reader,
                                Supla::Suplet::InstanceRecord *output) {
  if (!reader->consume('{')) {
    return false;
  }

  Supla::Suplet::ThermometerGroupConfig config = {};
  reader->skipWhitespace();
  if (reader->consume('}')) {
    return false;
  }

  while (true) {
    char key[24] = {};
    char text[16] = {};
    if (!reader->readString(key, sizeof(key)) || !reader->consume(':')) {
      return false;
    }

    if (equalText(key, "mode")) {
      if (!reader->readString(text, sizeof(text)) ||
          !parseThermometerMode(text, &config.mode)) {
        return false;
      }
    } else if (equalText(key, "refreshIntervalMs")) {
      if (!readUInt16(reader, &config.refreshIntervalMs)) {
        return false;
      }
    } else if (equalText(key, "sourceChannels")) {
      if (!readSourceChannels(reader, &config)) {
        return false;
      }
    } else {
      if (!reader->skipValue()) {
        return false;
      }
    }

    reader->skipWhitespace();
    if (reader->consume('}')) {
      break;
    }
    if (!reader->consume(',')) {
      return false;
    }
  }

  uint8_t buffer[SUPLA_SUPLET_MAX_CONFIG_SIZE] = {};
  uint16_t configSize = 0;
  if (!Supla::Suplet::serializeThermometerGroupConfig(
          config, buffer, sizeof(buffer), &configSize)) {
    return false;
  }
  return output->setConfig(buffer, configSize);
}

bool readTypedConfig(JsonReader *reader,
                     const Supla::Suplet::Definition &definition,
                     Supla::Suplet::InstanceRecord *output) {
  if (definition.kind == Supla::Suplet::Kind::ThermometerGroup) {
    return readThermometerGroupConfig(reader, output);
  }
  return reader->skipValue();
}

}  // namespace

namespace Supla {
namespace Suplet {

bool JsonInstanceConfigParser::parse(const char *json,
                                     const Definition &definition,
                                     InstanceRecord *output) {
  if (json == nullptr || output == nullptr ||
      !Runtime::validateDefinition(definition)) {
    return false;
  }

  InstanceRecord record = {};
  record.definitionId = definition.definitionId;
  record.definitionVersion = definition.definitionVersion;
  record.state = InstanceState::Active;

  JsonReader reader(json);
  if (!reader.consume('{')) {
    return false;
  }

  reader.skipWhitespace();
  if (reader.consume('}')) {
    return false;
  }

  while (true) {
    char key[24] = {};
    char text[16] = {};
    if (!reader.readString(key, sizeof(key)) || !reader.consume(':')) {
      return false;
    }

    if (equalText(key, "instanceId")) {
      if (!readUInt8(&reader, &record.instanceId) || record.instanceId == 0) {
        return false;
      }
    } else if (equalText(key, "definitionId")) {
      uint32_t value = 0;
      if (!reader.readUInt32(&value) || value != definition.definitionId) {
        return false;
      }
    } else if (equalText(key, "definitionVersion")) {
      uint32_t value = 0;
      if (!reader.readUInt32(&value) ||
          value > UINT16_MAX ||
          value != definition.definitionVersion) {
        return false;
      }
    } else if (equalText(key, "subDeviceId")) {
      if (!readUInt8(&reader, &record.subDeviceId)) {
        return false;
      }
    } else if (equalText(key, "state")) {
      if (!reader.readString(text, sizeof(text)) ||
          !parseState(text, &record.state)) {
        return false;
      }
    } else if (equalText(key, "config")) {
      if (!readTypedConfig(&reader, definition, &record)) {
        return false;
      }
    } else {
      if (!reader.skipValue()) {
        return false;
      }
    }

    reader.skipWhitespace();
    if (reader.consume('}')) {
      break;
    }
    if (!reader.consume(',')) {
      return false;
    }
  }

  if (!reader.atEnd() || record.instanceId == 0) {
    return false;
  }
  if (definition.kind == Kind::ThermometerGroup && record.configSize == 0) {
    return false;
  }

  *output = record;
  return true;
}

}  // namespace Suplet
}  // namespace Supla

#endif  // SUPLA_SUPLET_ENABLED

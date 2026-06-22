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

#include <supla/suplet/json_definition.h>
#include <supla/suplet/runtime.h>

#include <stdlib.h>
#include <string.h>

namespace {

class JsonReader {
 public:
  explicit JsonReader(const char *json) : pos(json) {}

  bool ok() const {
    return pos != nullptr;
  }

  void skipWhitespace() {
    while (ok() && (*pos == ' ' || *pos == '\n' || *pos == '\r' ||
                    *pos == '\t')) {
      pos++;
    }
  }

  bool consume(char expected) {
    skipWhitespace();
    if (!ok() || *pos != expected) {
      return false;
    }
    pos++;
    return true;
  }

  bool consumeLiteral(const char *literal) {
    skipWhitespace();
    size_t len = strlen(literal);
    if (!ok() || strncmp(pos, literal, len) != 0) {
      return false;
    }
    pos += len;
    return true;
  }

  bool nextIs(char expected) {
    skipWhitespace();
    return ok() && *pos == expected;
  }

  bool readString(char *output, size_t outputSize) {
    skipWhitespace();
    if (!ok() || output == nullptr || outputSize == 0 || *pos != '"') {
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
    if (!ok() || *pos != '"') {
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
    if (!ok() || value == nullptr || *pos < '0' || *pos > '9') {
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
    if (!ok() || value == nullptr) {
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

  bool skipValue() {
    skipWhitespace();
    if (!ok()) {
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
    return ok() && *pos == '\0';
  }

 private:
  const char *pos = nullptr;
};

bool equalText(const char *a, const char *b) {
  return a != nullptr && b != nullptr && strcmp(a, b) == 0;
}

bool equalText(const char *value, const char *verbose, const char *compact) {
  return equalText(value, verbose) || equalText(value, compact);
}

/**
 * Suplet definition JSON accepts two equivalent naming styles:
 *
 * Top-level fields:
 * - schemaVersion/sv, handlerVersion/hv, definitionId/di,
 *   definitionVersion/dv, maxInstances/mi, category/c, kind/k, name/n,
 *   parameters/p, channels/ch.
 *
 * Channel fields:
 * - channelId/id, kind/k, function/fn, defaultFunction/df, caption/cap.
 *
 * Parameter fields:
 * - key/key, type/t, default/d, min/min, max/max, lifecycle/lc,
 *   required/r, affectsTopology/at, values/v.
 *
 * Category values:
 * - virtual/virt, aggregate/aggr, modbus/modbus, httpIntegration/http.
 *
 * Definition and channel kind values:
 * - virtualRelay/virtRelay, virtualBinarySensor/virtBinSensor,
 *   thermometerGroup/thermoGroup, modbusRtu/modbusRtu,
 *   httpInverter/httpInverter, virtualThermometer/virtThermo.
 *
 * Parameter type values:
 * - bool/b, uint8/u8, uint16/u16, int16/i16, string/s, secret/sec,
 *   enum/e, channelList/cl.
 *
 * Parameter lifecycle values:
 * - editable/ed, createOnly/co, readonly/ro, secret/sec.
 *
 * Channel function values:
 * - powerSwitch/ps, lightSwitch/ls, openingSensorDoor/osd, thermometer/th.
 */

bool readUInt8(JsonReader *reader, uint8_t *value) {
  uint32_t tmp = 0;
  if (!reader->readUInt32(&tmp) || tmp > UINT8_MAX) {
    return false;
  }
  *value = static_cast<uint8_t>(tmp);
  return true;
}

bool readChannel(JsonReader *reader,
                 Supla::Suplet::JsonDefinition *output,
                 uint8_t index) {
  if (!reader->consume('{')) {
    return false;
  }

  auto channel = output->getChannel(index);
  channel->channelId = Supla::Suplet::kInvalidChannelId;
  channel->kind = Supla::Suplet::ChannelKind::Unknown;
  channel->defaultFunction = 0;
  channel->caption = nullptr;

  char tmp[SUPLA_SUPLET_MAX_CAPTION_SIZE] = {};
  reader->skipWhitespace();
  if (reader->consume('}')) {
    return false;
  }

  while (true) {
    char key[24] = {};
    if (!reader->readString(key, sizeof(key)) || !reader->consume(':')) {
      return false;
    }

    if (equalText(key, "channelId", "id")) {
      if (!readUInt8(reader, &channel->channelId) ||
          channel->channelId == Supla::Suplet::kInvalidChannelId) {
        return false;
      }
    } else if (equalText(key, "kind", "k")) {
      if (!reader->readString(tmp, sizeof(tmp)) ||
          !Supla::Suplet::JsonDefinitionParser::parseChannelKind(
              tmp, &channel->kind)) {
        return false;
      }
    } else if (equalText(key, "defaultFunction", "df")) {
      if (reader->nextIs('"')) {
        return false;
      }
      int32_t value = 0;
      if (!reader->readInt32(&value)) {
        return false;
      }
      channel->defaultFunction = value;
    } else if (equalText(key, "function", "fn")) {
      if (!reader->readString(tmp, sizeof(tmp)) ||
          !Supla::Suplet::JsonDefinitionParser::parseDefaultFunction(
              tmp, &channel->defaultFunction)) {
        return false;
      }
    } else if (equalText(key, "caption", "cap")) {
      char *caption = output->getCaptionBuffer(index);
      if (!reader->readString(caption, SUPLA_SUPLET_MAX_CAPTION_SIZE)) {
        return false;
      }
      channel->caption = caption;
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

  return channel->channelId != Supla::Suplet::kInvalidChannelId &&
         channel->kind != Supla::Suplet::ChannelKind::Unknown;
}

bool readChannels(JsonReader *reader, Supla::Suplet::JsonDefinition *output) {
  if (!reader->consume('[')) {
    return false;
  }

  auto definition = output->getDefinition();
  definition->channelCount = 0;

  reader->skipWhitespace();
  if (reader->consume(']')) {
    return true;
  }

  while (true) {
    if (definition->channelCount >=
        SUPLA_SUPLET_MAX_CHANNELS_PER_INSTANCE) {
      return false;
    }
    if (!readChannel(reader, output, definition->channelCount)) {
      return false;
    }
    definition->channelCount++;

    reader->skipWhitespace();
    if (reader->consume(']')) {
      return true;
    }
    if (!reader->consume(',')) {
      return false;
    }
  }
}

bool readEnumValues(JsonReader *reader, char *output, size_t outputSize) {
  if (!reader->consume('[') || output == nullptr || outputSize == 0) {
    return false;
  }
  output[0] = '\0';
  size_t used = 0;

  reader->skipWhitespace();
  if (reader->consume(']')) {
    return false;
  }

  while (true) {
    char value[SUPLA_SUPLET_MAX_PARAMETER_KEY_SIZE] = {};
    if (!reader->readString(value, sizeof(value))) {
      return false;
    }
    size_t len = strlen(value);
    size_t needed = len + (used == 0 ? 0 : 1);
    if (used + needed + 1 > outputSize) {
      return false;
    }
    if (used != 0) {
      output[used++] = ',';
    }
    memcpy(output + used, value, len);
    used += len;
    output[used] = '\0';

    reader->skipWhitespace();
    if (reader->consume(']')) {
      return true;
    }
    if (!reader->consume(',')) {
      return false;
    }
  }
}

bool readParameter(JsonReader *reader,
                   Supla::Suplet::JsonDefinition *output,
                   uint8_t index) {
  if (!reader->consume('{')) {
    return false;
  }

  auto parameter = output->getParameter(index);
  *parameter = Supla::Suplet::ParameterDefinition();
  parameter->min = INT32_MIN;
  parameter->max = INT32_MAX;

  char tmp[SUPLA_SUPLET_MAX_PARAMETER_TEXT_SIZE] = {};
  reader->skipWhitespace();
  if (reader->consume('}')) {
    return false;
  }

  while (true) {
    char key[24] = {};
    if (!reader->readString(key, sizeof(key)) || !reader->consume(':')) {
      return false;
    }

    if (equalText(key, "key")) {
      char *keyBuffer = output->getParameterKeyBuffer(index);
      if (!reader->readString(keyBuffer, SUPLA_SUPLET_MAX_PARAMETER_KEY_SIZE)) {
        return false;
      }
      parameter->key = keyBuffer;
    } else if (equalText(key, "type", "t")) {
      if (!reader->readString(tmp, sizeof(tmp)) ||
          !Supla::Suplet::JsonDefinitionParser::parseParameterType(
              tmp, &parameter->type)) {
        return false;
      }
    } else if (equalText(key, "lifecycle", "lc")) {
      if (!reader->readString(tmp, sizeof(tmp)) ||
          !Supla::Suplet::JsonDefinitionParser::parseParameterLifecycle(
              tmp, &parameter->lifecycle)) {
        return false;
      }
    } else if (equalText(key, "required", "r")) {
      bool value = false;
      if (!reader->readBool(&value)) {
        return false;
      }
      parameter->required = value ? 1 : 0;
    } else if (equalText(key, "affectsTopology", "at")) {
      bool value = false;
      if (!reader->readBool(&value)) {
        return false;
      }
      parameter->affectsTopology = value ? 1 : 0;
    } else if (equalText(key, "min")) {
      if (!reader->readInt32(&parameter->min)) {
        return false;
      }
    } else if (equalText(key, "max")) {
      if (!reader->readInt32(&parameter->max)) {
        return false;
      }
    } else if (equalText(key, "default", "d")) {
      parameter->hasDefault = 1;
      if (reader->nextIs('"')) {
        char *buffer = output->getParameterDefaultTextBuffer(index);
        if (!reader->readString(buffer,
                                SUPLA_SUPLET_MAX_PARAMETER_TEXT_SIZE)) {
          return false;
        }
        parameter->defaultText = buffer;
      } else if (reader->nextIs('t') || reader->nextIs('f')) {
        bool value = false;
        if (!reader->readBool(&value)) {
          return false;
        }
        parameter->defaultNumber = value ? 1 : 0;
      } else {
        if (!reader->readInt32(&parameter->defaultNumber)) {
          return false;
        }
      }
    } else if (equalText(key, "values", "v")) {
      char *buffer = output->getParameterEnumValuesBuffer(index);
      if (!readEnumValues(reader, buffer,
                          SUPLA_SUPLET_MAX_PARAMETER_TEXT_SIZE)) {
        return false;
      }
      parameter->enumValues = buffer;
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

  return parameter->key != nullptr &&
         parameter->key[0] != '\0' &&
         parameter->type != Supla::Suplet::ParameterType::Unknown &&
         parameter->min <= parameter->max;
}

bool readParameters(JsonReader *reader,
                    Supla::Suplet::JsonDefinition *output) {
  if (!reader->consume('[')) {
    return false;
  }

  auto definition = output->getDefinition();
  definition->parameterCount = 0;

  reader->skipWhitespace();
  if (reader->consume(']')) {
    return true;
  }

  while (true) {
    if (definition->parameterCount >= SUPLA_SUPLET_MAX_PARAMETERS) {
      return false;
    }
    if (!readParameter(reader, output, definition->parameterCount)) {
      return false;
    }
    definition->parameterCount++;

    reader->skipWhitespace();
    if (reader->consume(']')) {
      return true;
    }
    if (!reader->consume(',')) {
      return false;
    }
  }
}

}  // namespace

namespace Supla {
namespace Suplet {

JsonDefinition::JsonDefinition() {
  clear();
}

void JsonDefinition::clear() {
  definition = Definition();
  for (uint8_t i = 0; i < SUPLA_SUPLET_MAX_CHANNELS_PER_INSTANCE; i++) {
    channels[i] = ChannelDefinition();
  }
  for (uint8_t i = 0; i < SUPLA_SUPLET_MAX_PARAMETERS; i++) {
    parameters[i] = ParameterDefinition();
  }
  memset(name, 0, sizeof(name));
  memset(captions, 0, sizeof(captions));
  memset(parameterKeys, 0, sizeof(parameterKeys));
  memset(parameterDefaultText, 0, sizeof(parameterDefaultText));
  memset(parameterEnumValues, 0, sizeof(parameterEnumValues));
  definition.schemaVersion = 1;
  definition.handlerVersion = 1;
  definition.channels = channels;
  definition.parameters = parameters;
  definition.name = name;
}

Definition *JsonDefinition::getDefinition() {
  return &definition;
}

const Definition *JsonDefinition::getDefinition() const {
  return &definition;
}

ChannelDefinition *JsonDefinition::getChannel(uint8_t index) {
  if (index >= SUPLA_SUPLET_MAX_CHANNELS_PER_INSTANCE) {
    return nullptr;
  }
  return &channels[index];
}

ParameterDefinition *JsonDefinition::getParameter(uint8_t index) {
  if (index >= SUPLA_SUPLET_MAX_PARAMETERS) {
    return nullptr;
  }
  return &parameters[index];
}

char *JsonDefinition::getNameBuffer() {
  return name;
}

char *JsonDefinition::getCaptionBuffer(uint8_t index) {
  if (index >= SUPLA_SUPLET_MAX_CHANNELS_PER_INSTANCE) {
    return nullptr;
  }
  return captions[index];
}

char *JsonDefinition::getParameterKeyBuffer(uint8_t index) {
  if (index >= SUPLA_SUPLET_MAX_PARAMETERS) {
    return nullptr;
  }
  return parameterKeys[index];
}

char *JsonDefinition::getParameterDefaultTextBuffer(uint8_t index) {
  if (index >= SUPLA_SUPLET_MAX_PARAMETERS) {
    return nullptr;
  }
  return parameterDefaultText[index];
}

char *JsonDefinition::getParameterEnumValuesBuffer(uint8_t index) {
  if (index >= SUPLA_SUPLET_MAX_PARAMETERS) {
    return nullptr;
  }
  return parameterEnumValues[index];
}

bool JsonDefinitionParser::parse(const char *json, JsonDefinition *output) {
  if (json == nullptr || output == nullptr) {
    return false;
  }

  output->clear();
  JsonReader reader(json);
  Definition *definition = output->getDefinition();
  char tmp[SUPLA_SUPLET_MAX_NAME_SIZE] = {};

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

    if (equalText(key, "schemaVersion", "sv")) {
      if (!readUInt8(&reader, &definition->schemaVersion)) {
        return false;
      }
    } else if (equalText(key, "handlerVersion", "hv")) {
      if (!readUInt8(&reader, &definition->handlerVersion)) {
        return false;
      }
    } else if (equalText(key, "definitionId", "di")) {
      if (!reader.readUInt32(&definition->definitionId)) {
        return false;
      }
    } else if (equalText(key, "definitionVersion", "dv")) {
      uint32_t value = 0;
      if (!reader.readUInt32(&value) || value > UINT16_MAX) {
        return false;
      }
      definition->definitionVersion = static_cast<uint16_t>(value);
    } else if (equalText(key, "maxInstances", "mi")) {
      if (!readUInt8(&reader, &definition->maxInstances) ||
          definition->maxInstances == 0) {
        return false;
      }
    } else if (equalText(key, "category", "c")) {
      if (!reader.readString(tmp, sizeof(tmp)) ||
          !parseCategory(tmp, &definition->category)) {
        return false;
      }
    } else if (equalText(key, "kind", "k")) {
      if (!reader.readString(tmp, sizeof(tmp)) ||
          !parseKind(tmp, &definition->kind)) {
        return false;
      }
    } else if (equalText(key, "name", "n")) {
      if (!reader.readString(output->getNameBuffer(),
                             SUPLA_SUPLET_MAX_NAME_SIZE)) {
        return false;
      }
    } else if (equalText(key, "channels", "ch")) {
      if (!readChannels(&reader, output)) {
        return false;
      }
    } else if (equalText(key, "parameters", "p")) {
      if (!readParameters(&reader, output)) {
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

  if (!reader.atEnd()) {
    return false;
  }
  return Runtime::validateDefinition(*definition);
}

bool JsonDefinitionParser::parseCategory(const char *value,
                                         Category *category) {
  if (category == nullptr) {
    return false;
  }
  if (equalText(value, "virtual", "virt")) {
    *category = Category::Virtual;
  } else if (equalText(value, "aggregate", "aggr")) {
    *category = Category::Aggregate;
  } else if (equalText(value, "modbus")) {
    *category = Category::Modbus;
  } else if (equalText(value, "httpIntegration", "http")) {
    *category = Category::HttpIntegration;
  } else {
    return false;
  }
  return true;
}

bool JsonDefinitionParser::parseKind(const char *value, Kind *kind) {
  if (kind == nullptr) {
    return false;
  }
  if (equalText(value, "virtualRelay", "virtRelay")) {
    *kind = Kind::VirtualRelay;
  } else if (equalText(value, "virtualBinarySensor", "virtBinSensor")) {
    *kind = Kind::VirtualBinarySensor;
  } else if (equalText(value, "thermometerGroup", "thermoGroup")) {
    *kind = Kind::ThermometerGroup;
  } else if (equalText(value, "modbusRtu")) {
    *kind = Kind::ModbusRtu;
  } else if (equalText(value, "httpInverter")) {
    *kind = Kind::HttpInverter;
  } else {
    return false;
  }
  return true;
}

bool JsonDefinitionParser::parseChannelKind(const char *value,
                                            ChannelKind *kind) {
  if (kind == nullptr) {
    return false;
  }
  if (equalText(value, "virtualRelay", "virtRelay")) {
    *kind = ChannelKind::VirtualRelay;
  } else if (equalText(value, "virtualBinarySensor", "virtBinSensor")) {
    *kind = ChannelKind::VirtualBinarySensor;
  } else if (equalText(value, "virtualThermometer", "virtThermo")) {
    *kind = ChannelKind::VirtualThermometer;
  } else {
    return false;
  }
  return true;
}

bool JsonDefinitionParser::parseParameterType(const char *value,
                                              ParameterType *type) {
  if (type == nullptr) {
    return false;
  }
  if (equalText(value, "bool", "b")) {
    *type = ParameterType::Bool;
  } else if (equalText(value, "uint8", "u8")) {
    *type = ParameterType::UInt8;
  } else if (equalText(value, "uint16", "u16")) {
    *type = ParameterType::UInt16;
  } else if (equalText(value, "int16", "i16")) {
    *type = ParameterType::Int16;
  } else if (equalText(value, "string", "s")) {
    *type = ParameterType::String;
  } else if (equalText(value, "secret", "sec")) {
    *type = ParameterType::Secret;
  } else if (equalText(value, "enum", "e")) {
    *type = ParameterType::Enum;
  } else if (equalText(value, "channelList", "cl")) {
    *type = ParameterType::ChannelList;
  } else {
    *type = ParameterType::Unknown;
    return false;
  }
  return true;
}

bool JsonDefinitionParser::parseParameterLifecycle(
    const char *value,
    ParameterLifecycle *lifecycle) {
  if (lifecycle == nullptr) {
    return false;
  }
  if (equalText(value, "editable", "ed")) {
    *lifecycle = ParameterLifecycle::Editable;
  } else if (equalText(value, "createOnly", "co")) {
    *lifecycle = ParameterLifecycle::CreateOnly;
  } else if (equalText(value, "readonly", "ro")) {
    *lifecycle = ParameterLifecycle::ReadOnly;
  } else if (equalText(value, "secret", "sec")) {
    *lifecycle = ParameterLifecycle::Secret;
  } else {
    return false;
  }
  return true;
}

bool JsonDefinitionParser::parseDefaultFunction(const char *value,
                                                int32_t *function) {
  if (function == nullptr) {
    return false;
  }
  if (equalText(value, "powerSwitch", "ps")) {
    *function = SUPLA_CHANNELFNC_POWERSWITCH;
  } else if (equalText(value, "lightSwitch", "ls")) {
    *function = SUPLA_CHANNELFNC_LIGHTSWITCH;
  } else if (equalText(value, "openingSensorDoor", "osd")) {
    *function = SUPLA_CHANNELFNC_OPENINGSENSOR_DOOR;
  } else if (equalText(value, "thermometer", "th")) {
    *function = SUPLA_CHANNELFNC_THERMOMETER;
  } else {
    return false;
  }
  return true;
}

}  // namespace Suplet
}  // namespace Supla

#endif  // SUPLA_SUPLET_ENABLED

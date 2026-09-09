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
#include <supla/channels/channel.h>
#include <supla/element.h>
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

  bool readInt32(int32_t *value) {
    if (value == nullptr) {
      return false;
    }
    skipWhitespace();
    if (pos == nullptr) {
      return false;
    }
    bool negative = *pos == '-';
    if (negative) {
      pos++;
    }
    uint32_t magnitude = 0;
    if (!readUInt32(&magnitude) ||
        (!negative && magnitude > INT32_MAX) ||
        (negative && magnitude > static_cast<uint32_t>(INT32_MAX) + 1)) {
      return false;
    }
    if (negative && magnitude == static_cast<uint32_t>(INT32_MAX) + 1) {
      *value = INT32_MIN;
    } else {
      *value = negative ? -static_cast<int32_t>(magnitude)
                        : static_cast<int32_t>(magnitude);
    }
    return true;
  }

  char peek() {
    skipWhitespace();
    return pos == nullptr ? '\0' : *pos;
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

int hexDigitValue(char digit) {
  if (digit >= '0' && digit <= '9') {
    return digit - '0';
  }
  if (digit >= 'a' && digit <= 'f') {
    return digit - 'a' + 10;
  }
  if (digit >= 'A' && digit <= 'F') {
    return digit - 'A' + 10;
  }
  return -1;
}

bool decodeChannelValue(const char *hex, char *value) {
  if (hex == nullptr || value == nullptr ||
      strlen(hex) != 2 * SUPLA_CHANNELVALUE_SIZE) {
    return false;
  }
  for (size_t i = 0; i < SUPLA_CHANNELVALUE_SIZE; i++) {
    int high = hexDigitValue(hex[2 * i]);
    int low = hexDigitValue(hex[2 * i + 1]);
    if (high < 0 || low < 0) {
      return false;
    }
    value[i] = static_cast<char>((high << 4) | low);
  }
  return true;
}

bool readWeeklyScheduleMode(JsonReader *reader, uint8_t *mode) {
  if (reader == nullptr || mode == nullptr) {
    return false;
  }
  if (reader->peek() != '"') {
    uint32_t numericMode = 0;
    if (!reader->readUInt32(&numericMode) || numericMode > UINT8_MAX) {
      return false;
    }
    *mode = static_cast<uint8_t>(numericMode);
    return true;
  }

  char name[24] = {};
  if (!reader->readString(name, sizeof(name))) {
    return false;
  }
  struct NamedMode {
    const char *name;
    uint8_t mode;
  };
  static constexpr NamedMode modes[] = {
      {"not_set", SUPLA_RELAY_MODE_NOT_SET},
      {"unlocked", SUPLA_BUTTON_MODE_NOT_SET},
      {"locked", SUPLA_BUTTON_MODE_LOCKED},
      {"on_once", SUPLA_RELAY_MODE_ON_ONCE},
      {"off_once", SUPLA_RELAY_MODE_OFF_ONCE},
      {"forced_on", SUPLA_RELAY_MODE_FORCED_ON},
      {"forced_off", SUPLA_RELAY_MODE_FORCED_OFF},
      {"automatic", SUPLA_RELAY_MODE_AUTOMATIC},
  };
  for (const auto &item : modes) {
    if (equalText(name, item.name)) {
      *mode = item.mode;
      return true;
    }
  }
  return false;
}

bool parseWeeklyScheduleProgram(JsonReader *reader,
                                TWeeklyScheduleProgram *program) {
  if (reader == nullptr || program == nullptr || !reader->consume('{')) {
    return false;
  }
  bool hasMode = false;
  bool hasFirst = false;
  bool hasSecond = false;
  while (true) {
    char key[32] = {};
    if (!reader->readString(key, sizeof(key)) || !reader->consume(':')) {
      return false;
    }
    if (equalText(key, "mode")) {
      if (hasMode || !readWeeklyScheduleMode(reader, &program->Mode)) {
        return false;
      }
      hasMode = true;
    } else if (equalText(key, "value1") || equalText(key, "value2") ||
               equalText(key, "relayModeDurationS") ||
               equalText(key, "relayOppositeModeDurationS")) {
      const bool named = equalText(key, "relayModeDurationS") ||
                         equalText(key, "relayOppositeModeDurationS");
      const bool first = equalText(key, "value1") ||
                         equalText(key, "relayModeDurationS");
      int32_t value = 0;
      if ((first ? hasFirst : hasSecond) ||
          !reader->readInt32(&value) || value < (named ? 0 : INT16_MIN) ||
          value > (named ? UINT16_MAX : INT16_MAX)) {
        return false;
      }
      if (first) {
        hasFirst = true;
        program->RelayModeDurationS = static_cast<uint16_t>(value);
      } else {
        hasSecond = true;
        program->RelayOppositeModeDurationS = static_cast<uint16_t>(value);
      }
    } else if (!reader->skipValue()) {
      return false;
    }

    if (reader->consume('}')) {
      return hasMode;
    }
    if (!reader->consume(',')) {
      return false;
    }
  }
}

bool parseWeeklySchedulePrograms(JsonReader *reader,
                                 TChannelConfig_WeeklySchedule *schedule,
                                 uint8_t *programCount) {
  if (reader == nullptr || schedule == nullptr || programCount == nullptr ||
      !reader->consume('[')) {
    return false;
  }
  *programCount = 0;
  if (reader->consume(']')) {
    return true;
  }
  while (*programCount < SUPLA_WEEKLY_SCHEDULE_PROGRAMS_MAX_SIZE) {
    if (!parseWeeklyScheduleProgram(reader,
                                    &schedule->Program[*programCount])) {
      return false;
    }
    (*programCount)++;
    if (reader->consume(']')) {
      return true;
    }
    if (!reader->consume(',')) {
      return false;
    }
  }
  return false;
}

int weeklyScheduleDay(const char *name) {
  static constexpr const char *days[] = {
      "sun", "mon", "tue", "wed", "thu", "fri", "sat"};
  for (int day = 0; day < 7; day++) {
    if (equalText(name, days[day])) {
      return day;
    }
  }
  return -1;
}

bool parseWeeklyScheduleDays(JsonReader *reader, uint8_t *dayMask) {
  if (reader == nullptr || dayMask == nullptr || !reader->consume('[')) {
    return false;
  }
  *dayMask = 0;
  while (true) {
    char name[8] = {};
    if (!reader->readString(name, sizeof(name))) {
      return false;
    }
    int day = weeklyScheduleDay(name);
    if (day < 0) {
      return false;
    }
    *dayMask |= static_cast<uint8_t>(1 << day);
    if (reader->consume(']')) {
      return *dayMask != 0;
    }
    if (!reader->consume(',')) {
      return false;
    }
  }
}

bool parseWeeklyScheduleTime(const char *text, bool allowEndOfDay,
                             uint8_t *quarter) {
  if (text == nullptr || quarter == nullptr || strlen(text) != 5 ||
      text[2] != ':' || text[0] < '0' || text[0] > '9' ||
      text[1] < '0' || text[1] > '9' || text[3] < '0' || text[3] > '9' ||
      text[4] < '0' || text[4] > '9') {
    return false;
  }
  int hour = (text[0] - '0') * 10 + text[1] - '0';
  int minute = (text[3] - '0') * 10 + text[4] - '0';
  if (minute >= 60 || minute % 15 != 0 || hour > 24 ||
      (hour == 24 && (minute != 0 || !allowEndOfDay))) {
    return false;
  }
  *quarter = static_cast<uint8_t>(hour * 4 + minute / 15);
  return true;
}

void setWeeklyScheduleQuarter(TChannelConfig_WeeklySchedule *schedule,
                              int day, int quarter, uint8_t programId) {
  int index = day * 96 + quarter;
  if (index % 2 == 0) {
    schedule->Quarters[index / 2] =
        (schedule->Quarters[index / 2] & 0xF0) | programId;
  } else {
    schedule->Quarters[index / 2] =
        (schedule->Quarters[index / 2] & 0x0F) | (programId << 4);
  }
}

bool applyWeeklyScheduleEntry(TChannelConfig_WeeklySchedule *schedule,
                              uint8_t dayMask, uint8_t from, uint8_t to,
                              uint8_t programId) {
  if (schedule == nullptr || dayMask == 0 || from >= 96 || to > 96 ||
      from == to || programId == 0 ||
      programId > SUPLA_WEEKLY_SCHEDULE_PROGRAMS_MAX_SIZE) {
    return false;
  }
  for (int day = 0; day < 7; day++) {
    if ((dayMask & (1 << day)) == 0) {
      continue;
    }
    if (from < to) {
      for (int quarter = from; quarter < to; quarter++) {
        setWeeklyScheduleQuarter(schedule, day, quarter, programId);
      }
    } else {
      for (int quarter = from; quarter < 96; quarter++) {
        setWeeklyScheduleQuarter(schedule, day, quarter, programId);
      }
      int nextDay = (day + 1) % 7;
      for (int quarter = 0; quarter < to; quarter++) {
        setWeeklyScheduleQuarter(schedule, nextDay, quarter, programId);
      }
    }
  }
  return true;
}

bool parseWeeklyScheduleEntry(JsonReader *reader,
                              TChannelConfig_WeeklySchedule *schedule,
                              uint8_t *maxProgramId) {
  if (reader == nullptr || schedule == nullptr || maxProgramId == nullptr ||
      !reader->consume('{')) {
    return false;
  }
  uint8_t dayMask = 0;
  uint8_t from = 0;
  uint8_t to = 0;
  uint8_t programId = 0;
  bool hasDays = false;
  bool hasFrom = false;
  bool hasTo = false;
  bool hasProgram = false;
  while (true) {
    char key[24] = {};
    if (!reader->readString(key, sizeof(key)) || !reader->consume(':')) {
      return false;
    }
    if (equalText(key, "days")) {
      if (hasDays || !parseWeeklyScheduleDays(reader, &dayMask)) {
        return false;
      }
      hasDays = true;
    } else if (equalText(key, "from") || equalText(key, "to")) {
      char time[6] = {};
      bool isTo = equalText(key, "to");
      if ((isTo ? hasTo : hasFrom) ||
          !reader->readString(time, sizeof(time)) ||
          !parseWeeklyScheduleTime(time, isTo, isTo ? &to : &from)) {
        return false;
      }
      if (isTo) {
        hasTo = true;
      } else {
        hasFrom = true;
      }
    } else if (equalText(key, "program")) {
      uint32_t value = 0;
      if (hasProgram || !reader->readUInt32(&value) || value == 0 ||
          value > SUPLA_WEEKLY_SCHEDULE_PROGRAMS_MAX_SIZE) {
        return false;
      }
      programId = static_cast<uint8_t>(value);
      hasProgram = true;
    } else if (!reader->skipValue()) {
      return false;
    }

    if (reader->consume('}')) {
      if (!hasDays || !hasFrom || !hasTo || !hasProgram ||
          !applyWeeklyScheduleEntry(
              schedule, dayMask, from, to, programId)) {
        return false;
      }
      if (programId > *maxProgramId) {
        *maxProgramId = programId;
      }
      return true;
    }
    if (!reader->consume(',')) {
      return false;
    }
  }
}

bool parseWeeklyScheduleEntries(JsonReader *reader,
                                TChannelConfig_WeeklySchedule *schedule,
                                uint8_t *maxProgramId) {
  if (reader == nullptr || schedule == nullptr || maxProgramId == nullptr ||
      !reader->consume('[')) {
    return false;
  }
  *maxProgramId = 0;
  if (reader->consume(']')) {
    return true;
  }
  while (true) {
    if (!parseWeeklyScheduleEntry(reader, schedule, maxProgramId)) {
      return false;
    }
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
  uint32_t channelNumber = UINT32_MAX;
  uint32_t senderId = 0;
  uint32_t durationMs = 0;
  uint32_t relayMode = UINT32_MAX;
  uint32_t buttonMode = UINT32_MAX;
  bool keepArtifact = false;
  bool hasWeeklySchedulePrograms = false;
  bool hasWeeklyScheduleEntries = false;
  uint8_t weeklyScheduleProgramCount = 0;
  uint8_t weeklyScheduleMaxProgramId = 0;
  char valueHex[2 * SUPLA_CHANNELVALUE_SIZE + 1] = {};
  TChannelConfig_WeeklySchedule weeklySchedule = {};
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

#if SUPLA_TEST
CommandProcessor::CommandProcessor(
    SuplaDeviceClass *device,
    TestChannelConfigHandler testChannelConfigHandler,
    void *testChannelConfigContext)
    : device(device),
      testChannelConfigHandler(testChannelConfigHandler),
      testChannelConfigContext(testChannelConfigContext) {
}
#endif

#if SUPLA_TEST
CommandProcessor::CommandProcessor(
    SuplaDeviceClass *device,
    TestChannelValueHandler testChannelValueHandler,
    void *testChannelValueContext)
    : device(device),
      testChannelValueHandler(testChannelValueHandler),
      testChannelValueContext(testChannelValueContext) {
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
    } else if (equalText(key, "channelNumber")) {
      if (!reader.readUInt32(&command->channelNumber)) {
        return false;
      }
    } else if (equalText(key, "senderId")) {
      if (!reader.readUInt32(&command->senderId)) {
        return false;
      }
    } else if (equalText(key, "durationMs")) {
      if (!reader.readUInt32(&command->durationMs)) {
        return false;
      }
    } else if (equalText(key, "relayMode")) {
      if (!reader.readUInt32(&command->relayMode)) {
        return false;
      }
    } else if (equalText(key, "buttonMode")) {
      if (!reader.readUInt32(&command->buttonMode)) {
        return false;
      }
    } else if (equalText(key, "valueHex")) {
      if (command->valueHex[0] != '\0' ||
          !reader.readString(command->valueHex, sizeof(command->valueHex))) {
        return false;
      }
    } else if (equalText(key, "programs")) {
      if (command->hasWeeklySchedulePrograms ||
          !parseWeeklySchedulePrograms(
              &reader,
              &command->weeklySchedule,
              &command->weeklyScheduleProgramCount)) {
        return false;
      }
      command->hasWeeklySchedulePrograms = true;
    } else if (equalText(key, "entries")) {
      if (command->hasWeeklyScheduleEntries ||
          !parseWeeklyScheduleEntries(
              &reader,
              &command->weeklySchedule,
              &command->weeklyScheduleMaxProgramId)) {
        return false;
      }
      command->hasWeeklyScheduleEntries = true;
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
  if (equalText(command.operation, "weeklySchedule")) {
    if (command.channelNumber > UINT8_MAX ||
        !command.hasWeeklySchedulePrograms ||
        !command.hasWeeklyScheduleEntries ||
        command.weeklyScheduleMaxProgramId >
            command.weeklyScheduleProgramCount) {
      sendError(writer, "invalid_arguments");
      return;
    }

    TSD_ChannelConfig config = {};
    config.ChannelNumber = static_cast<uint8_t>(command.channelNumber);
    config.ConfigType = SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE;
    config.ConfigSize = sizeof(command.weeklySchedule);
    memcpy(config.Config,
           &command.weeklySchedule,
           sizeof(command.weeklySchedule));

    uint8_t result = SUPLA_CONFIG_RESULT_FALSE;
#if SUPLA_TEST
    if (testChannelConfigHandler != nullptr) {
      result = testChannelConfigHandler(
          testChannelConfigContext, &config, true);
    } else {
#endif
      auto *element =
          Supla::Element::getElementByChannelNumber(command.channelNumber);
      if (element == nullptr || element->getChannel() == nullptr) {
        sendError(writer, "channel_not_found");
        return;
      }
      config.Func = element->getChannel()->getDefaultFunction();
      result = element->handleChannelConfig(&config, true);
#if SUPLA_TEST
    }
#endif

    char response[128] = {};
    snprintf(response,
             sizeof(response),
             "{\"op\":\"weeklySchedule.result\",\"channelNumber\":%u,"
             "\"result\":%u,\"ok\":%s}\n",
             static_cast<unsigned>(config.ChannelNumber),
             static_cast<unsigned>(result),
             result == SUPLA_CONFIG_RESULT_TRUE ? "true" : "false");
    sendText(writer, response);
    return;
  }

  if (equalText(command.operation, "channelValue")) {
    int payloadCount = (command.valueHex[0] != '\0' ? 1 : 0) +
        (command.relayMode != UINT32_MAX ? 1 : 0) +
        (command.buttonMode != UINT32_MAX ? 1 : 0);
    if (command.channelNumber > UINT8_MAX || payloadCount != 1 ||
        (command.relayMode != UINT32_MAX && command.relayMode > UINT8_MAX) ||
        (command.buttonMode != UINT32_MAX && command.buttonMode > UINT8_MAX)) {
      sendError(writer, "invalid_arguments");
      return;
    }

    TSD_SuplaChannelNewValue newValue = {};
    newValue.SenderID = static_cast<_supla_int_t>(command.senderId);
    newValue.ChannelNumber = static_cast<unsigned char>(command.channelNumber);
    newValue.DurationMS = command.durationMs;
    if (command.valueHex[0] != '\0') {
      if (!decodeChannelValue(command.valueHex, newValue.value)) {
        sendError(writer, "invalid_value_hex");
        return;
      }
    } else if (command.relayMode != UINT32_MAX) {
      TRelayChannel_Value relayValue = {};
      relayValue.RelayMode = static_cast<unsigned char>(command.relayMode);
      relayValue.hi =
          command.relayMode == SUPLA_RELAY_MODE_ON_ONCE ||
                  command.relayMode == SUPLA_RELAY_MODE_FORCED_ON
              ? 1
              : 0;
      memcpy(newValue.value, &relayValue, sizeof(relayValue));
    } else {
      TActionTriggerProperties properties = {};
      properties.ButtonMode = static_cast<unsigned char>(command.buttonMode);
      memcpy(newValue.value, &properties, sizeof(properties));
    }

    int32_t result = 0;
#if SUPLA_TEST
    if (testChannelValueHandler != nullptr) {
      result = testChannelValueHandler(testChannelValueContext, &newValue);
    } else {
#endif
      auto *element =
          Supla::Element::getElementByChannelNumber(command.channelNumber);
      if (element == nullptr) {
        sendError(writer, "channel_not_found");
        return;
      }
      result = element->handleNewValueFromServer(&newValue);
#if SUPLA_TEST
    }
#endif

    char response[128] = {};
    snprintf(response,
             sizeof(response),
             "{\"op\":\"channelValue.result\",\"channelNumber\":%u,"
             "\"result\":%d,\"ok\":%s}\n",
             static_cast<unsigned>(newValue.ChannelNumber),
             static_cast<int>(result),
             result == 1 ? "true" : "false");
    sendText(writer, response);
    return;
  }

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

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
#include <errno.h>
#include <fcntl.h>
#include <linux_network.h>
#include <supla-common/tools.h>
#include <supla/control/dimmer_leds.h>
#include <supla/control/rgb_leds.h>
#include <supla/control/rgbw_leds.h>
#include <supla/control/virtual_relay.h>
#include <supla/log_wrapper.h>
#include <supla/time.h>
#include <supla/version.h>
#include <sys/stat.h>
#include <unistd.h>

// Below includes are added just for CI compilation check. Some of them
// are not used in any cpp file, so they would not be compiled otherwise.
// Remove them and keep only required one in real application.
#include <linux_clock.h>
#include <linux_file_state_logger.h>
#include <linux_file_storage.h>
#include <linux_mqtt_client.h>
#include <linux_yaml_config.h>
#include <supla/IEEE754tools.h>
#include <supla/action_handler.h>
#include <supla/actions.h>
#include <supla/at_channel.h>
#include <supla/channel.h>
#include <supla/channel_element.h>
#include <supla/channel_extended.h>
#include <supla/condition.h>
#include <supla/condition_getter.h>
#include <supla/correction.h>
#include <supla/crc16.h>
#include <supla/definitions.h>
#include <supla/element.h>
#include <supla/events.h>
#include <supla/io.h>
#include <supla/local_action.h>
#include <supla/parser/json.h>
#include <supla/parser/simple.h>
#include <supla/pv/afore.h>
#include <supla/pv/fronius.h>
#include <supla/rsa_verificator.h>
#include <supla/sensor/binary.h>
#include <supla/sensor/binary_parsed.h>
#include <supla/sensor/distance.h>
#include <supla/sensor/electricity_meter.h>
#include <supla/sensor/electricity_meter_parsed.h>
#include <supla/sensor/general_purpose_channel_base.h>
#include <supla/sensor/hygro_meter.h>
#include <supla/sensor/impulse_counter_parsed.h>
#include <supla/sensor/normally_open.h>
#include <supla/sensor/one_phase_electricity_meter.h>
#include <supla/sensor/pressure.h>
#include <supla/sensor/rain.h>
#include <supla/sensor/therm_hygro_meter.h>
#include <supla/sensor/therm_hygro_press_meter.h>
#include <supla/sensor/thermometer.h>
#include <supla/sensor/thermometer_parsed.h>
#include <supla/sensor/virtual_binary.h>
#include <supla/sensor/weight.h>
#include <supla/sensor/wind.h>
#include <supla/sha256.h>
#include <supla/source/cmd.h>
#include <supla/source/file.h>
#include <supla/supla_lib_config.h>
#include <supla/suplet/capability_registry.h>
#include <supla/suplet/definition_cache.h>
#include <supla/suplet/manager.h>
#include <supla/suplet/registry.h>
#include <supla/suplet/server_config.h>
#include <supla/timer.h>
#include <supla/tools.h>
#include <supla/uptime.h>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cxxopts.hpp>
#include <iostream>
#include <memory>
#include <string>

// reguired by linux_log.c
int logLevel = LOG_INFO;
int runAsDaemon = 0;

namespace {

const char *serverConfigResultToString(
    Supla::Suplet::ServerConfigResult value) {
  switch (value) {
    case Supla::Suplet::ServerConfigResult::Applied:
      return "Applied";
    case Supla::Suplet::ServerConfigResult::Removed:
      return "Removed";
    case Supla::Suplet::ServerConfigResult::InvalidArgument:
      return "InvalidArgument";
    case Supla::Suplet::ServerConfigResult::DefinitionNotSupported:
      return "DefinitionNotSupported";
    case Supla::Suplet::ServerConfigResult::InvalidDefinition:
      return "InvalidDefinition";
    case Supla::Suplet::ServerConfigResult::InvalidConfig:
      return "InvalidConfig";
    case Supla::Suplet::ServerConfigResult::StorageError:
      return "StorageError";
    case Supla::Suplet::ServerConfigResult::ResourceLimitExceeded:
      return "ResourceLimitExceeded";
    case Supla::Suplet::ServerConfigResult::InstanceLimitExceeded:
      return "InstanceLimitExceeded";
    case Supla::Suplet::ServerConfigResult::ChannelLimitExceeded:
      return "ChannelLimitExceeded";
    case Supla::Suplet::ServerConfigResult::CreateOnlyParamChanged:
      return "CreateOnlyParamChanged";
    case Supla::Suplet::ServerConfigResult::Busy:
      return "Busy";
    case Supla::Suplet::ServerConfigResult::TopologyChangeNotAllowed:
      return "TopologyChangeNotAllowed";
    case Supla::Suplet::ServerConfigResult::DefinitionNotFound:
      return "DefinitionNotFound";
    case Supla::Suplet::ServerConfigResult::DefinitionCannotBeChanged:
      return "DefinitionCannotBeChanged";
  }

  return "Unknown";
}

const char *supletCategoryToString(Supla::Suplet::Category category) {
  switch (category) {
    case Supla::Suplet::Category::Virtual:
      return "virtual";
    case Supla::Suplet::Category::Aggregate:
      return "aggregate";
    case Supla::Suplet::Category::Modbus:
      return "modbus";
    case Supla::Suplet::Category::HttpIntegration:
      return "httpIntegration";
    case Supla::Suplet::Category::Unknown:
    default:
      return "unknown";
  }
}

const char *supletKindToString(Supla::Suplet::Kind kind) {
  switch (kind) {
    case Supla::Suplet::Kind::VirtualRelay:
      return "virtualRelay";
    case Supla::Suplet::Kind::VirtualBinarySensor:
      return "virtualBinarySensor";
    case Supla::Suplet::Kind::ThermometerGroup:
      return "thermometerGroup";
    case Supla::Suplet::Kind::ModbusRtu:
      return "modbusRtu";
    case Supla::Suplet::Kind::HttpInverter:
      return "httpInverter";
    case Supla::Suplet::Kind::Unknown:
    default:
      return "unknown";
  }
}

void logSupletCapabilities(
    const Supla::Suplet::CapabilityRegistry &capabilityRegistry) {
  const uint8_t count = capabilityRegistry.getCount();
  SUPLA_LOG_INFO("Suplet capabilities: total=%u", static_cast<unsigned>(count));
  for (uint8_t i = 0; i < count; i++) {
    Supla::Suplet::Capability capability = {};
    if (!capabilityRegistry.getCapability(i, &capability)) {
      SUPLA_LOG_WARNING("Suplet capabilities: failed to read item %u", i);
      continue;
    }
    SUPLA_LOG_INFO(
        "Suplet capability[%u]: category=%s, kind=%s, schema=%u-%u, "
        "handler=%u, maxInstances=%u, downloaded=%u",
        static_cast<unsigned>(i),
        supletCategoryToString(capability.category),
        supletKindToString(capability.kind),
        static_cast<unsigned>(capability.minSchemaVersion),
        static_cast<unsigned>(capability.maxSchemaVersion),
        static_cast<unsigned>(capability.handlerVersion),
        static_cast<unsigned>(capability.maxInstances),
        static_cast<unsigned>(capability.supportsDownloadedDefinition));
  }
}

bool equalText(const char *a, const char *b) {
  return a != nullptr && b != nullptr && strcmp(a, b) == 0;
}

const char *calcfgResultToString(int value) {
  switch (value) {
    case SUPLA_CALCFG_RESULT_TRUE:
      return "TRUE";
    case SUPLA_CALCFG_RESULT_FALSE:
      return "FALSE";
    case SUPLA_CALCFG_RESULT_DONE:
      return "DONE";
    case SUPLA_CALCFG_RESULT_UNAUTHORIZED:
      return "UNAUTHORIZED";
    case SUPLA_CALCFG_RESULT_NOT_SUPPORTED:
      return "NOT_SUPPORTED";
    case SUPLA_CALCFG_RESULT_ID_NOT_EXISTS:
      return "ID_NOT_EXISTS";
  }

  return "UNKNOWN";
}

const char *supletResultDetailToString(uint8_t value) {
  switch (value) {
    case SUPLA_CALCFG_SUPLET_RESULT_OK:
      return "OK";
    case SUPLA_CALCFG_SUPLET_RESULT_INVALID_REQUEST:
      return "InvalidRequest";
    case SUPLA_CALCFG_SUPLET_RESULT_UNSUPPORTED_DEFINITION:
      return "UnsupportedDefinition";
    case SUPLA_CALCFG_SUPLET_RESULT_DEFINITION_NOT_FOUND:
      return "DefinitionNotFound";
    case SUPLA_CALCFG_SUPLET_RESULT_DEFINITION_TRANSFER_FAILED:
      return "DefinitionTransferFailed";
    case SUPLA_CALCFG_SUPLET_RESULT_DEFINITION_SHA_MISMATCH:
      return "DefinitionShaMismatch";
    case SUPLA_CALCFG_SUPLET_RESULT_INSTANCE_LIMIT_EXCEEDED:
      return "InstanceLimitExceeded";
    case SUPLA_CALCFG_SUPLET_RESULT_CHANNEL_LIMIT_EXCEEDED:
      return "ChannelLimitExceeded";
    case SUPLA_CALCFG_SUPLET_RESULT_RAM_LIMIT_EXCEEDED:
      return "RamLimitExceeded";
    case SUPLA_CALCFG_SUPLET_RESULT_CONFIG_TOO_LARGE:
      return "ConfigTooLarge";
    case SUPLA_CALCFG_SUPLET_RESULT_INVALID_CONFIG:
      return "InvalidConfig";
    case SUPLA_CALCFG_SUPLET_RESULT_STORAGE_ERROR:
      return "StorageError";
    case SUPLA_CALCFG_SUPLET_RESULT_BUSY:
      return "Busy";
    case SUPLA_CALCFG_SUPLET_RESULT_CREATE_ONLY_PARAM_CHANGED:
      return "CreateOnlyParamChanged";
    case SUPLA_CALCFG_SUPLET_RESULT_TOPOLOGY_CHANGE_NOT_ALLOWED:
      return "TopologyChangeNotAllowed";
    case SUPLA_CALCFG_SUPLET_RESULT_DEFINITION_CANNOT_BE_CHANGED:
      return "DefinitionCannotBeChanged";
    case SUPLA_CALCFG_SUPLET_RESULT_INVALID_DEFINITION:
      return "InvalidDefinition";
  }

  return "Unknown";
}

class FifoJsonReader {
 public:
  explicit FifoJsonReader(const char *json) : pos(json) {
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

struct FifoCalcfgCommand {
  char operation[24] = {};
  uint32_t sessionId = 0;
  uint32_t instanceId = 0;
  uint32_t definitionId = 0;
  uint32_t definitionVersion = 0;
  char definitionJson[SUPLA_SUPLET_MAX_DEFINITION_JSON_SIZE + 1] = {};
  char paramsJson[SUPLA_SUPLET_MAX_CONFIG_SIZE + 1] = {};
};

class LinuxSupletSha256Provider : public Supla::Suplet::Sha256Provider {
 public:
  bool calculate(const uint8_t *data,
                 size_t dataSize,
                 uint8_t *output,
                 size_t outputSize) override {
    if (output == nullptr || outputSize < 32) {
      return false;
    }
    Supla::Sha256 sha256;
    if (data != nullptr && dataSize > 0) {
      sha256.update(data, static_cast<int>(dataSize));
    }
    sha256.digest(output, 32);
    return true;
  }
};

bool parseFifoCalcfgCommand(const char *json, FifoCalcfgCommand *command) {
  if (json == nullptr || command == nullptr) {
    return false;
  }

  *command = FifoCalcfgCommand();
  FifoJsonReader reader(json);
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
    if (equalText(key, "calcfg")) {
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
    } else if (equalText(key, "definitionJson")) {
      if (!reader.readString(command->definitionJson,
                             sizeof(command->definitionJson))) {
        return false;
      }
    } else if (equalText(key, "paramsJson")) {
      if (!reader.readString(command->paramsJson,
                             sizeof(command->paramsJson))) {
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

uint32_t nextFifoCalcfgSessionId() {
  static uint32_t nextSessionId = 1;
  return nextSessionId++;
}

void logCalcfgLocalResult(const char *label,
                          int resultCode,
                          const TDS_DeviceCalCfgResult &result) {
  if (result.DataSize == sizeof(TCalCfg_SupletResult)) {
    TCalCfg_SupletResult supletResult = {};
    memcpy(&supletResult, result.Data, sizeof(supletResult));
    SUPLA_LOG_INFO(
        "Suplet FIFO CALCFG %s: result=%s(%d), detail=%s(%u), phase=%u, "
        "instance=%u, definition=%u/%u, required=%u, available=%u",
        label,
        calcfgResultToString(resultCode),
        resultCode,
        supletResultDetailToString(supletResult.DetailCode),
        supletResult.DetailCode,
        supletResult.Phase,
        supletResult.InstanceId,
        supletResult.DefinitionId,
        supletResult.DefinitionVersion,
        supletResult.Required,
        supletResult.Available);
    return;
  }

  SUPLA_LOG_INFO("Suplet FIFO CALCFG %s: result=%s(%d), dataSize=%u",
                 label,
                 calcfgResultToString(resultCode),
                 resultCode,
                 result.DataSize);
}

void logSha256Hex(const char *prefix, const uint8_t *sha256) {
  if (prefix == nullptr || sha256 == nullptr) {
    return;
  }
  char hex[65] = {};
  static const char digits[] = "0123456789abcdef";
  for (uint8_t i = 0; i < 32; i++) {
    hex[i * 2] = digits[(sha256[i] >> 4) & 0x0F];
    hex[i * 2 + 1] = digits[sha256[i] & 0x0F];
  }
  SUPLA_LOG_INFO("%s%s", prefix, hex);
}

int sendLocalCalcfg(int command,
                    const void *payload,
                    uint32_t payloadSize,
                    TDS_DeviceCalCfgResult *output,
                    const char *label) {
  TSD_DeviceCalCfgRequest request = {};
  TDS_DeviceCalCfgResult result = {};
  request.ChannelNumber = -1;
  request.SuperUserAuthorized = 1;
  request.Command = command;
  request.DataSize = payloadSize;
  if (payloadSize > 0 && payload != nullptr) {
    memcpy(request.Data, payload, payloadSize);
  }
  int resultCode = SuplaDevice.handleCalcfgFromServer(&request, &result);
  logCalcfgLocalResult(label, resultCode, result);
  if (output != nullptr) {
    *output = result;
  }
  return resultCode;
}

bool setupSupletRuntime(Supla::Config *config) {
  static Supla::Suplet::Registry registry;
  static Supla::Suplet::CapabilityRegistry capabilityRegistry;
  static Supla::Suplet::Manager manager(config);
  static LinuxSupletSha256Provider sha256Provider;
  static Supla::Suplet::DefinitionCache definitionCache(config,
                                                        &sha256Provider);
  static Supla::Suplet::DownloadedDefinitionStore downloadedDefinitions;
  static Supla::Suplet::ServerConfigHandler serverConfigHandler(
      &manager, &registry, &definitionCache, &downloadedDefinitions);

  static Supla::Suplet::ChannelDefinition relayChannels[1] = {};
  relayChannels[0].channelId = 1;
  relayChannels[0].kind = Supla::Suplet::ChannelKind::VirtualRelay;
  relayChannels[0].defaultFunction = SUPLA_CHANNELFNC_POWERSWITCH;
  relayChannels[0].caption = "Suplet virtual relay";

  static Supla::Suplet::Definition relayDefinition = {};
  relayDefinition.category = Supla::Suplet::Category::Virtual;
  relayDefinition.kind = Supla::Suplet::Kind::VirtualRelay;
  relayDefinition.schemaVersion = 1;
  relayDefinition.handlerVersion = 1;
  relayDefinition.definitionId = 1000;
  relayDefinition.definitionVersion = 1;
  relayDefinition.name = "sd4linux virtual relay";
  relayDefinition.channels = relayChannels;
  relayDefinition.channelCount = 1;

  if (!registry.add(&relayDefinition)) {
    SUPLA_LOG_ERROR("Suplet runtime: failed to register virtual relay");
    return false;
  }

  Supla::Suplet::Capability relayCapability = {};
  relayCapability.category = Supla::Suplet::Category::Virtual;
  relayCapability.kind = Supla::Suplet::Kind::VirtualRelay;
  relayCapability.minSchemaVersion = 1;
  relayCapability.maxSchemaVersion = 1;
  relayCapability.handlerVersion = 1;
  relayCapability.maxInstances = 8;
  relayCapability.supportsDownloadedDefinition = 1;
  if (!capabilityRegistry.add(relayCapability)) {
    SUPLA_LOG_ERROR(
        "Suplet runtime: failed to register virtual relay capability");
    return false;
  }

  static Supla::Suplet::ChannelDefinition binaryChannels[1] = {};
  binaryChannels[0].channelId = 1;
  binaryChannels[0].kind = Supla::Suplet::ChannelKind::VirtualBinarySensor;
  binaryChannels[0].defaultFunction = SUPLA_CHANNELFNC_OPENINGSENSOR_DOOR;
  binaryChannels[0].caption = "Suplet virtual binary";

  static Supla::Suplet::Definition binaryDefinition = {};
  binaryDefinition.category = Supla::Suplet::Category::Virtual;
  binaryDefinition.kind = Supla::Suplet::Kind::VirtualBinarySensor;
  binaryDefinition.schemaVersion = 1;
  binaryDefinition.handlerVersion = 1;
  binaryDefinition.definitionId = 1002;
  binaryDefinition.definitionVersion = 1;
  binaryDefinition.name = "sd4linux virtual binary sensor";
  binaryDefinition.channels = binaryChannels;
  binaryDefinition.channelCount = 1;

  if (!registry.add(&binaryDefinition, 8)) {
    SUPLA_LOG_ERROR("Suplet runtime: failed to register virtual binary sensor");
    return false;
  }

  Supla::Suplet::Capability binaryCapability = {};
  binaryCapability.category = Supla::Suplet::Category::Virtual;
  binaryCapability.kind = Supla::Suplet::Kind::VirtualBinarySensor;
  binaryCapability.minSchemaVersion = 1;
  binaryCapability.maxSchemaVersion = 1;
  binaryCapability.handlerVersion = 1;
  binaryCapability.maxInstances = 8;
  binaryCapability.supportsDownloadedDefinition = 1;
  if (!capabilityRegistry.add(binaryCapability)) {
    SUPLA_LOG_ERROR(
        "Suplet runtime: failed to register virtual binary sensor capability");
    return false;
  }

  static Supla::Suplet::ChannelDefinition thermometerGroupChannels[1] = {};
  thermometerGroupChannels[0].channelId = 1;
  thermometerGroupChannels[0].kind =
      Supla::Suplet::ChannelKind::VirtualThermometer;
  thermometerGroupChannels[0].defaultFunction = SUPLA_CHANNELFNC_THERMOMETER;
  thermometerGroupChannels[0].caption = "Suplet thermometer aggregate";

  static Supla::Suplet::Definition thermometerGroupDefinition = {};
  thermometerGroupDefinition.category = Supla::Suplet::Category::Aggregate;
  thermometerGroupDefinition.kind = Supla::Suplet::Kind::ThermometerGroup;
  thermometerGroupDefinition.schemaVersion = 1;
  thermometerGroupDefinition.handlerVersion = 1;
  thermometerGroupDefinition.definitionId = 1001;
  thermometerGroupDefinition.definitionVersion = 1;
  thermometerGroupDefinition.name = "sd4linux thermometer aggregate";
  thermometerGroupDefinition.channels = thermometerGroupChannels;
  thermometerGroupDefinition.channelCount = 1;

  if (!registry.add(&thermometerGroupDefinition, 8)) {
    SUPLA_LOG_ERROR("Suplet runtime: failed to register thermometer aggregate");
    return false;
  }

  Supla::Suplet::Capability thermometerGroupCapability = {};
  thermometerGroupCapability.category = Supla::Suplet::Category::Aggregate;
  thermometerGroupCapability.kind = Supla::Suplet::Kind::ThermometerGroup;
  thermometerGroupCapability.minSchemaVersion = 1;
  thermometerGroupCapability.maxSchemaVersion = 1;
  thermometerGroupCapability.handlerVersion = 1;
  thermometerGroupCapability.maxInstances = 8;
  thermometerGroupCapability.supportsDownloadedDefinition = 1;
  if (!capabilityRegistry.add(thermometerGroupCapability)) {
    SUPLA_LOG_ERROR(
        "Suplet runtime: failed to register thermometer aggregate capability");
    return false;
  }

  serverConfigHandler.loadDownloadedDefinitions();

  SuplaDevice.setSupletRuntime(&manager, &registry);
  SuplaDevice.setSupletCapabilityRegistry(&capabilityRegistry);
  SuplaDevice.setSupletServerConfigHandler(&serverConfigHandler);

  logSupletCapabilities(capabilityRegistry);

  return true;
}

class SupletFifoInput {
 public:
  explicit SupletFifoInput(const std::string &path) : path(path) {
  }

  ~SupletFifoInput() {
    closeFd();
  }

  bool init() {
    if (path.empty()) {
      return true;
    }

    struct stat st = {};
    if (stat(path.c_str(), &st) == 0) {
      if (!S_ISFIFO(st.st_mode)) {
        SUPLA_LOG_ERROR("Suplet FIFO path exists but is not a FIFO: %s",
                        path.c_str());
        return false;
      }
    } else if (errno == ENOENT) {
      if (mkfifo(path.c_str(), 0600) != 0) {
        SUPLA_LOG_ERROR("Suplet FIFO create failed for %s: %s",
                        path.c_str(),
                        strerror(errno));
        return false;
      }
    } else {
      SUPLA_LOG_ERROR(
          "Suplet FIFO stat failed for %s: %s", path.c_str(), strerror(errno));
      return false;
    }

    SUPLA_LOG_INFO("Suplet FIFO enabled: %s", path.c_str());
    return openFifo();
  }

  void iterate() {
    if (path.empty()) {
      return;
    }

    if (fd < 0 && !openFifo()) {
      return;
    }

    char readBuffer[256] = {};
    while (true) {
      ssize_t result = read(fd, readBuffer, sizeof(readBuffer));
      if (result > 0) {
        append(readBuffer, static_cast<size_t>(result));
        continue;
      }

      if (result == 0) {
        closeFd();
        openFifo();
        return;
      }

      if (errno != EAGAIN && errno != EWOULDBLOCK) {
        SUPLA_LOG_WARNING("Suplet FIFO read failed: %s", strerror(errno));
        closeFd();
        openFifo();
      }
      return;
    }
  }

 private:
  bool openFifo() {
    if (fd >= 0 || path.empty()) {
      return true;
    }

    fd = open(path.c_str(), O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
      SUPLA_LOG_WARNING(
          "Suplet FIFO open failed for %s: %s", path.c_str(), strerror(errno));
      return false;
    }

    return true;
  }

  void closeFd() {
    if (fd >= 0) {
      close(fd);
      fd = -1;
    }
  }

  void append(const char *data, size_t size) {
    for (size_t i = 0; i < size; i++) {
      char c = data[i];
      if (c == '\r') {
        continue;
      }
      if (c == '\n') {
        processLine();
        buffer.clear();
        continue;
      }
      if (buffer.size() >= kMaxCommandSize) {
        SUPLA_LOG_WARNING("Suplet FIFO command too long, dropping buffer");
        buffer.clear();
        continue;
      }
      buffer.push_back(c);
    }
  }

  void processLine() {
    if (buffer.empty()) {
      return;
    }

    // FIFO is a local PoC transport for the device-side suplet contract.
    // Supported JSON commands:
    // {"op":"upsert","instanceId":1,"definitionId":1000,"definitionVersion":1}
    // {"op":"upsert","instanceId":2,"definitionId":1002,"definitionVersion":1}
    // {"op":"remove","instanceId":1}
    // {"op":"saveDefinition","definitionId":2000,"definitionVersion":1,...}
    // Thermometer aggregate instances use:
    // {"config":{"mode":"avg|min|max","refreshIntervalMs":1000,
    //            "sourceChannels":[1,2,3]}}
    // CALCFG injection commands call SuplaDevice.handleCalcfgFromServer()
    // locally and do not send any reply to the Supla server:
    // {"calcfg":"saveDefinition","definitionId":2000,
    //  "definitionVersion":1,"definitionJson":"{...}"}
    // {"calcfg":"removeDefinition","definitionId":2000,
    //  "definitionVersion":1}
    // {"calcfg":"upsertInstance","instanceId":1,"definitionId":2000,
    //  "definitionVersion":1,"paramsJson":"{}"}
    // {"calcfg":"getInstanceCount"}
    // {"calcfg":"getInstanceList"}
    // {"calcfg":"getInstanceInfo","instanceId":1}
    // {"calcfg":"getInstanceConfig","instanceId":1}
    SUPLA_LOG_INFO("Suplet FIFO command: %s", buffer.c_str());

    FifoCalcfgCommand calcfgCommand = {};
    if (parseFifoCalcfgCommand(buffer.c_str(), &calcfgCommand)) {
      processCalcfgCommand(calcfgCommand);
      return;
    }

    auto validationResult =
        SuplaDevice.validateSupletCommandJson(buffer.c_str());
    if (validationResult != Supla::Suplet::ServerConfigResult::Applied &&
        validationResult != Supla::Suplet::ServerConfigResult::Removed) {
      SUPLA_LOG_WARNING("Suplet FIFO validation failed: %s",
                        serverConfigResultToString(validationResult));
      return;
    }

    auto applyResult = SuplaDevice.applySupletCommandJson(buffer.c_str());
    SUPLA_LOG_INFO("Suplet FIFO apply result: %s",
                   serverConfigResultToString(applyResult));
  }

  void processCalcfgCommand(const FifoCalcfgCommand &command) {
    if (equalText(command.operation, "saveDefinition")) {
      processCalcfgSaveDefinition(command);
      return;
    }
    if (equalText(command.operation, "removeDefinition")) {
      processCalcfgRemoveDefinition(command);
      return;
    }
    if (equalText(command.operation, "upsertInstance")) {
      processCalcfgUpsertInstance(command);
      return;
    }
    if (equalText(command.operation, "getInstanceCount")) {
      processCalcfgGetInstanceCount();
      return;
    }
    if (equalText(command.operation, "getInstanceList")) {
      processCalcfgGetInstanceList();
      return;
    }
    if (equalText(command.operation, "getInstanceInfo")) {
      processCalcfgGetInstanceInfo(command);
      return;
    }
    if (equalText(command.operation, "getInstanceConfig")) {
      processCalcfgGetInstanceConfig(command);
      return;
    }
    SUPLA_LOG_WARNING("Suplet FIFO CALCFG unsupported operation: %s",
                      command.operation);
  }

  void processCalcfgSaveDefinition(const FifoCalcfgCommand &command) {
    const uint16_t jsonSize = strlen(command.definitionJson);
    if (command.definitionId == 0 || command.definitionVersion == 0 ||
        command.definitionVersion > UINT16_MAX || jsonSize == 0) {
      SUPLA_LOG_WARNING("Suplet FIFO CALCFG saveDefinition invalid arguments");
      return;
    }

    uint32_t sessionId =
        command.sessionId == 0 ? nextFifoCalcfgSessionId() : command.sessionId;
    uint8_t sha256[32] = {};
    calculateSha256(command.definitionJson, jsonSize, sha256);

    TCalCfg_SupletDefinitionBegin begin = {};
    begin.SessionId = sessionId;
    begin.DefinitionId = command.definitionId;
    begin.DefinitionVersion = static_cast<uint16_t>(command.definitionVersion);
    begin.JsonSize = jsonSize;
    memcpy(begin.JsonSha256, sha256, sizeof(sha256));
    if (sendLocalCalcfg(SUPLA_CALCFG_CMD_SUPLET_DEFINITION_BEGIN,
                        &begin,
                        sizeof(begin),
                        nullptr,
                        "definition.begin") != SUPLA_CALCFG_RESULT_DONE) {
      return;
    }

    uint16_t offset = 0;
    while (offset < jsonSize) {
      uint8_t chunkSize =
          jsonSize - offset > SUPLA_CALCFG_SUPLET_DEFINITION_CHUNK_MAXSIZE
              ? SUPLA_CALCFG_SUPLET_DEFINITION_CHUNK_MAXSIZE
              : static_cast<uint8_t>(jsonSize - offset);
      TCalCfg_SupletDefinitionChunk chunk = {};
      chunk.SessionId = sessionId;
      chunk.Offset = offset;
      chunk.Size = chunkSize;
      memcpy(chunk.Data, command.definitionJson + offset, chunkSize);
      uint32_t payloadSize =
          offsetof(TCalCfg_SupletDefinitionChunk, Data) + chunkSize;
      if (sendLocalCalcfg(SUPLA_CALCFG_CMD_SUPLET_DEFINITION_CHUNK,
                          &chunk,
                          payloadSize,
                          nullptr,
                          "definition.chunk") != SUPLA_CALCFG_RESULT_DONE) {
        return;
      }
      offset += chunkSize;
    }

    TCalCfg_SupletSessionRequest commit = {};
    commit.SessionId = sessionId;
    sendLocalCalcfg(SUPLA_CALCFG_CMD_SUPLET_DEFINITION_COMMIT,
                    &commit,
                    sizeof(commit),
                    nullptr,
                    "definition.commit");
  }

  void processCalcfgRemoveDefinition(const FifoCalcfgCommand &command) {
    if (command.definitionId == 0 || command.definitionVersion == 0 ||
        command.definitionVersion > UINT16_MAX) {
      SUPLA_LOG_WARNING(
          "Suplet FIFO CALCFG removeDefinition invalid arguments");
      return;
    }

    TCalCfg_SupletDefinitionRequest request = {};
    request.DefinitionId = command.definitionId;
    request.DefinitionVersion =
        static_cast<uint16_t>(command.definitionVersion);
    sendLocalCalcfg(SUPLA_CALCFG_CMD_SUPLET_DEFINITION_REMOVE,
                    &request,
                    sizeof(request),
                    nullptr,
                    "definition.remove");
  }

  void processCalcfgUpsertInstance(const FifoCalcfgCommand &command) {
    const uint16_t paramsSize = strlen(command.paramsJson);
    if (command.definitionId == 0 ||
        command.definitionVersion == 0 || command.instanceId > UINT8_MAX ||
        command.definitionVersion > UINT16_MAX) {
      SUPLA_LOG_WARNING("Suplet FIFO CALCFG upsertInstance invalid arguments");
      return;
    }

    uint32_t sessionId =
        command.sessionId == 0 ? nextFifoCalcfgSessionId() : command.sessionId;
    uint8_t sha256[32] = {};
    calculateSha256(command.paramsJson, paramsSize, sha256);

    TCalCfg_SupletInstanceBegin begin = {};
    begin.SessionId = sessionId;
    begin.InstanceId = static_cast<uint8_t>(command.instanceId);
    begin.DefinitionId = command.definitionId;
    begin.DefinitionVersion = static_cast<uint16_t>(command.definitionVersion);
    begin.ParamsSize = paramsSize;
    memcpy(begin.ParamsSha256, sha256, sizeof(sha256));
    if (sendLocalCalcfg(SUPLA_CALCFG_CMD_SUPLET_INSTANCE_BEGIN,
                        &begin,
                        sizeof(begin),
                        nullptr,
                        "instance.begin") != SUPLA_CALCFG_RESULT_DONE) {
      return;
    }

    uint16_t offset = 0;
    while (offset < paramsSize) {
      uint8_t chunkSize =
          paramsSize - offset > SUPLA_CALCFG_SUPLET_INSTANCE_CHUNK_MAXSIZE
              ? SUPLA_CALCFG_SUPLET_INSTANCE_CHUNK_MAXSIZE
              : static_cast<uint8_t>(paramsSize - offset);
      TCalCfg_SupletInstanceChunk chunk = {};
      chunk.SessionId = sessionId;
      chunk.Offset = offset;
      chunk.Size = chunkSize;
      memcpy(chunk.Data, command.paramsJson + offset, chunkSize);
      uint32_t payloadSize =
          offsetof(TCalCfg_SupletInstanceChunk, Data) + chunkSize;
      if (sendLocalCalcfg(SUPLA_CALCFG_CMD_SUPLET_INSTANCE_CHUNK,
                          &chunk,
                          payloadSize,
                          nullptr,
                          "instance.chunk") != SUPLA_CALCFG_RESULT_DONE) {
        return;
      }
      offset += chunkSize;
    }

    TCalCfg_SupletSessionRequest commit = {};
    commit.SessionId = sessionId;
    sendLocalCalcfg(SUPLA_CALCFG_CMD_SUPLET_INSTANCE_COMMIT,
                    &commit,
                    sizeof(commit),
                    nullptr,
                    "instance.commit");
  }

  void processCalcfgGetInstanceCount() {
    TDS_DeviceCalCfgResult result = {};
    int resultCode = sendLocalCalcfg(SUPLA_CALCFG_CMD_SUPLET_GET_INSTANCE_COUNT,
                                     nullptr,
                                     0,
                                     &result,
                                     "instance.count");
    if (resultCode != SUPLA_CALCFG_RESULT_TRUE ||
        result.DataSize != sizeof(TCalCfg_SupletInstanceCount)) {
      return;
    }
    TCalCfg_SupletInstanceCount count = {};
    memcpy(&count, result.Data, sizeof(count));
    SUPLA_LOG_INFO(
        "Suplet FIFO CALCFG instance.count data: count=%u, maxInstances=%u, "
        "maxChannelsPerInstance=%u, maxCachedDefinitions=%u",
        count.Count,
        count.MaxInstances,
        count.MaxChannelsPerInstance,
        count.MaxCachedDefinitions);
  }

  void processCalcfgGetInstanceList() {
    TCalCfg_SupletListRequest request = {};
    request.Offset = 0;
    request.Limit = SUPLA_CALCFG_SUPLET_INSTANCE_LIST_MAX_ITEMS;
    TDS_DeviceCalCfgResult result = {};
    int resultCode = sendLocalCalcfg(SUPLA_CALCFG_CMD_SUPLET_GET_INSTANCE_LIST,
                                     &request,
                                     sizeof(request),
                                     &result,
                                     "instance.list");
    if (resultCode != SUPLA_CALCFG_RESULT_TRUE ||
        result.DataSize != sizeof(TCalCfg_SupletInstanceList)) {
      return;
    }
    TCalCfg_SupletInstanceList list = {};
    memcpy(&list, result.Data, sizeof(list));
    SUPLA_LOG_INFO("Suplet FIFO CALCFG instance.list data: count=%u, total=%u",
                   list.Count,
                   list.Total);
    for (uint8_t i = 0; i < list.Count; i++) {
      const auto &item = list.Items[i];
      SUPLA_LOG_INFO(
          "Suplet FIFO CALCFG instance.list[%u]: instance=%u, "
          "definition=%u/%u, subDeviceId=%u, channelCount=%u",
          i,
          item.InstanceId,
          item.DefinitionId,
          item.DefinitionVersion,
          item.SubDeviceId,
          item.ChannelCount);
    }
  }

  void processCalcfgGetInstanceInfo(const FifoCalcfgCommand &command) {
    if (command.instanceId == 0 || command.instanceId > UINT8_MAX) {
      SUPLA_LOG_WARNING("Suplet FIFO CALCFG getInstanceInfo invalid arguments");
      return;
    }
    TCalCfg_SupletInstanceRequest request = {};
    request.InstanceId = static_cast<uint8_t>(command.instanceId);
    TDS_DeviceCalCfgResult result = {};
    int resultCode = sendLocalCalcfg(SUPLA_CALCFG_CMD_SUPLET_GET_INSTANCE_INFO,
                                     &request,
                                     sizeof(request),
                                     &result,
                                     "instance.info");
    if (resultCode != SUPLA_CALCFG_RESULT_TRUE ||
        result.DataSize != sizeof(TCalCfg_SupletInstanceInfo)) {
      return;
    }
    TCalCfg_SupletInstanceInfo info = {};
    memcpy(&info, result.Data, sizeof(info));
    SUPLA_LOG_INFO(
        "Suplet FIFO CALCFG instance.info data: instance=%u, "
        "definition=%u/%u, subDeviceId=%u, channelCount=%u, paramsSize=%u",
        info.InstanceId,
        info.DefinitionId,
        info.DefinitionVersion,
        info.SubDeviceId,
        info.ChannelCount,
        info.ParamsSize);
    logSha256Hex("Suplet FIFO CALCFG instance.info paramsSha256=",
                 info.ParamsSha256);
  }

  void processCalcfgGetInstanceConfig(const FifoCalcfgCommand &command) {
    if (command.instanceId == 0 || command.instanceId > UINT8_MAX) {
      SUPLA_LOG_WARNING(
          "Suplet FIFO CALCFG getInstanceConfig invalid arguments");
      return;
    }

    std::string config;
    uint16_t offset = 0;
    uint16_t totalSize = 0;
    while (true) {
      TCalCfg_SupletInstanceConfigRequest request = {};
      request.InstanceId = static_cast<uint8_t>(command.instanceId);
      request.Offset = offset;
      request.MaxSize = SUPLA_CALCFG_SUPLET_CONFIG_CHUNK_MAXSIZE;
      TDS_DeviceCalCfgResult result = {};
      int resultCode =
          sendLocalCalcfg(SUPLA_CALCFG_CMD_SUPLET_GET_INSTANCE_CONFIG,
                          &request,
                          sizeof(request),
                          &result,
                          "instance.config");
      if (resultCode != SUPLA_CALCFG_RESULT_TRUE ||
          result.DataSize < offsetof(TCalCfg_SupletInstanceConfigChunk, Data)) {
        return;
      }
      TCalCfg_SupletInstanceConfigChunk chunk = {};
      memcpy(&chunk, result.Data, result.DataSize);
      if (chunk.InstanceId != command.instanceId ||
          result.DataSize !=
              offsetof(TCalCfg_SupletInstanceConfigChunk, Data) + chunk.Size) {
        SUPLA_LOG_WARNING("Suplet FIFO CALCFG instance.config malformed chunk");
        return;
      }
      if (offset == 0) {
        totalSize = chunk.TotalSize;
        config.reserve(totalSize);
      }
      config.append(chunk.Data, chunk.Size);
      offset += chunk.Size;
      if (offset >= chunk.TotalSize || chunk.Size == 0) {
        break;
      }
    }

    SUPLA_LOG_INFO(
        "Suplet FIFO CALCFG instance.config data: instance=%u, "
        "size=%u, json=%s",
        command.instanceId,
        totalSize,
        config.c_str());
  }

  static constexpr size_t kMaxCommandSize = 4096;

  std::string path;
  std::string buffer;
  int fd = -1;
};

}  // namespace

int main(int argc, char *argv[]) {
  try {
    cxxopts::Options options(argv[0], "Supla device client. See www.supla.org");

    options.add_options()("D,debug", "Enable debug logs")(
        "V,verbose",
        "Enable verbose debug logs",
        cxxopts::value<bool>()->default_value("false"))(
        "i,integer", "Int param", cxxopts::value<int>())(
        "c,config",
        "Config file name",
        cxxopts::value<std::string>()->default_value("etc/supla-device.yaml"))(
        "suplet-fifo",
        "Read suplet command JSON lines from FIFO path",
        cxxopts::value<std::string>()->default_value(""))(
        "d,daemon", "Run in daemon mode (run in background and log to syslog)")(
        "s,service", "Run as a service (log to syslog but don't fork)")(
        "h,help", "Show this help")("v,version", "Show version");

    auto result = options.parse(argc, argv);

    if (result.count("help")) {
      std::cout << options.help() << std::endl;
      exit(0);
    }

    if (result.count("version")) {
      std::cout << argv[0] << " version: " << SUPLA_SHORT_VERSION << std::endl;
      exit(0);
    }

    if (result.count("daemon")) {
      runAsDaemon = 1;
      if (!st_try_fork()) {
        SUPLA_LOG_ERROR("Can't start daemon");
        exit(1);
      }
    }

    if (result.count("service") && result.count("daemon")) {
      SUPLA_LOG_ERROR("Can't use daemon and service mode at the same time");
      exit(1);
    }

    if (result.count("service")) {
      runAsDaemon = true;  // just for using syslog
      if ((chdir("/")) < 0) {
        SUPLA_LOG_ERROR("Can't start as a service");
        exit(1);
      }

      close(STDIN_FILENO);
      close(STDOUT_FILENO);
      close(STDERR_FILENO);
    }

    std::string cfgFile = result["config"].as<std::string>();
    auto config = std::make_unique<Supla::LinuxYamlConfig>(cfgFile);
    // we init it earlier than in SuplaDevice.begin, because it is needed
    // earlier
    config->init();

    if (result.count("debug") || config->isDebug()) {
      logLevel = LOG_DEBUG;
    }

    if (result.count("verbose") || config->isVerbose()) {
      logLevel = LOG_VERBOSE;
    }

    if (result.count("warning") || config->isWarning()) {
      logLevel = LOG_WARNING;
    }

    if (result.count("error") || config->isError()) {
      logLevel = LOG_ERR;
    }
    SuplaDevice.setLogLevel(logLevel);

    SUPLA_LOG_INFO(" *** Starting supla-device ***");
    SUPLA_LOG_INFO("Using config file %s", cfgFile.c_str());

    st_hook_signals();

    auto clock = std::make_unique<Supla::LinuxClock>();
    (void)(clock);

    if (!config->loadChannels()) {
      SUPLA_LOG_ERROR("Loading channels failed. Exit");
      exit(1);
    }

    Supla::LinuxFileStorage storage(config->getStateFilesPath());

    SuplaDevice.setLastStateLogger(
        new Supla::Device::FileStateLogger(config->getStateFilesPath()));
    Supla::LinuxNetwork network;

    if (!setupSupletRuntime(config.get())) {
      SUPLA_LOG_ERROR("Suplet runtime setup failed. Exit");
      exit(1);
    }

    SuplaDevice.setProtoVerboseLog(config->isProtoVerboseLog());
    SuplaDevice.begin(config->getProtoVersion());

    if (SuplaDevice.getCurrentStatus() != STATUS_INITIALIZED) {
      SUPLA_LOG_INFO("Incomplete configuration. Please fix it and try again");
      exit(1);
    }

    SupletFifoInput supletFifo(result["suplet-fifo"].as<std::string>());
    if (!supletFifo.init()) {
      SUPLA_LOG_ERROR("Suplet FIFO init failed. Exit");
      exit(1);
    }

    Supla::LinuxMqttClient::start();

    while (st_app_terminate == 0) {
      supletFifo.iterate();
      SuplaDevice.iterate();
      delay(10);
    }
    SUPLA_LOG_INFO("Exit");

    exit(0);
  } catch (const cxxopts::exceptions::exception &e) {
    std::cout << "error parsing options: " << e.what() << std::endl;
    exit(1);
  }
}

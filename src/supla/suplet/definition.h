// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_SUPLET_DEFINITION_H_
#define SRC_SUPLA_SUPLET_DEFINITION_H_

#include <stdint.h>
#include <supla-common/proto.h>
#include <supla/suplet/channel_map.h>

namespace Supla {
class Element;

namespace Suplet {

struct Definition;
struct InstanceRecord;

class RuntimeHandler {
 public:
  virtual ~RuntimeHandler() = default;

  virtual bool createElements(const Definition &definition,
                              const InstanceRecord &instance,
                              Supla::Element **created,
                              uint8_t createdSize,
                              ChannelMap *createdChannelMap) = 0;
};

enum class Category : uint8_t {
  Unknown = 0,
  Virtual = 1,
  Aggregate = 2,
  Modbus = 3,
  HttpIntegration = 4,
};

enum class Kind : uint8_t {
  Unknown = 0,
  VirtualRelay = 1,
  VirtualBinarySensor = 2,
  ThermometerGroup = 3,
  ModbusRtu = 4,
  HttpInverter = 5,
};

enum class ChannelKind : uint8_t {
  Unknown = 0,
  VirtualRelay = 1,
  VirtualBinarySensor = 2,
  VirtualThermometer = 3,
};

enum class ParameterType : uint8_t {
  Unknown = 0,
  Bool = 1,
  UInt8 = 2,
  UInt16 = 3,
  Int16 = 4,
  String = 5,
  Secret = 6,
  Enum = 7,
  ChannelList = 8,
};

enum class ParameterLifecycle : uint8_t {
  Editable = 0,
  CreateOnly = 1,
  ReadOnly = 2,
  Secret = 3,
};

struct Capability {
  Category category = Category::Unknown;
  Kind kind = Kind::Unknown;
  uint32_t definitionId = 0;
  uint16_t minDefinitionVersion = 0;
  uint16_t maxDefinitionVersion = 0;
  uint8_t minSchemaVersion = 1;
  uint8_t maxSchemaVersion = 1;
  uint8_t handlerVersion = 1;
  uint8_t maxInstances = 1;
  uint8_t supportsDownloadedDefinition = 0;
};

struct ChannelDefinition {
  uint8_t channelId = kInvalidChannelId;
  ChannelKind kind = ChannelKind::Unknown;
  int32_t defaultFunction = 0;
  const char *caption = nullptr;
};

struct ParameterDefinition {
  const char *key = nullptr;
  ParameterType type = ParameterType::Unknown;
  ParameterLifecycle lifecycle = ParameterLifecycle::Editable;
  int32_t min = INT32_MIN;
  int32_t max = INT32_MAX;
  int32_t defaultNumber = 0;
  const char *defaultText = nullptr;
  const char *enumValues = nullptr;
  uint8_t required = 0;
  uint8_t hasDefault = 0;
  uint8_t affectsTopology = 0;
};

constexpr uint32_t kInvalidDefinitionId = 0;
constexpr uint32_t kServerDefinitionIdMin = 1000;

inline bool isValidDefinitionId(uint32_t definitionId) {
  return definitionId != kInvalidDefinitionId;
}

inline bool isServerDefinitionId(uint32_t definitionId) {
  return definitionId >= kServerDefinitionIdMin;
}

inline bool isFirmwarePrivateDefinitionId(uint32_t definitionId) {
  return definitionId > kInvalidDefinitionId &&
         definitionId < kServerDefinitionIdMin;
}

struct Definition {
  Category category = Category::Unknown;
  Kind kind = Kind::Unknown;
  uint8_t schemaVersion = 1;
  uint8_t handlerVersion = 1;
  uint32_t definitionId = 0;
  uint16_t definitionVersion = 0;
  uint8_t maxInstances = 1;
  const char *name = nullptr;
  const ChannelDefinition *channels = nullptr;
  uint8_t channelCount = 0;
  const ParameterDefinition *parameters = nullptr;
  uint8_t parameterCount = 0;
  RuntimeHandler *runtimeHandler = nullptr;
  const char *definitionJson = nullptr;
  uint16_t definitionJsonSize = 0;
};

bool getDefinitionChannelIds(const Definition &definition,
                             uint8_t *output,
                             uint8_t outputSize);

}  // namespace Suplet
}  // namespace Supla

#endif  // SRC_SUPLA_SUPLET_DEFINITION_H_

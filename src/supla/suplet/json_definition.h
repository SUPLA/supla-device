// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_SUPLET_JSON_DEFINITION_H_
#define SRC_SUPLA_SUPLET_JSON_DEFINITION_H_

#include <stdint.h>
#include <supla/suplet/definition.h>

#ifndef SUPLA_SUPLET_MAX_NAME_SIZE
#define SUPLA_SUPLET_MAX_NAME_SIZE 48
#endif

#ifndef SUPLA_SUPLET_MAX_CAPTION_SIZE
#define SUPLA_SUPLET_MAX_CAPTION_SIZE 48
#endif

#ifndef SUPLA_SUPLET_MAX_PARAMETERS
#define SUPLA_SUPLET_MAX_PARAMETERS 16
#endif

#ifndef SUPLA_SUPLET_MAX_PARAMETER_KEY_SIZE
#define SUPLA_SUPLET_MAX_PARAMETER_KEY_SIZE 32
#endif

#ifndef SUPLA_SUPLET_MAX_PARAMETER_TEXT_SIZE
#define SUPLA_SUPLET_MAX_PARAMETER_TEXT_SIZE 64
#endif

namespace Supla {
namespace Suplet {

class JsonDefinition {
 public:
  JsonDefinition();

  void clear();
  Definition *getDefinition();
  const Definition *getDefinition() const;
  ChannelDefinition *getChannel(uint8_t index);
  ParameterDefinition *getParameter(uint8_t index);
  char *getNameBuffer();
  char *getCaptionBuffer(uint8_t index);
  char *getParameterKeyBuffer(uint8_t index);
  char *getParameterDefaultTextBuffer(uint8_t index);
  char *getParameterEnumValuesBuffer(uint8_t index);

 private:
  Definition definition;
  ChannelDefinition channels[SUPLA_SUPLET_MAX_CHANNELS_PER_INSTANCE];
  ParameterDefinition parameters[SUPLA_SUPLET_MAX_PARAMETERS];
  char name[SUPLA_SUPLET_MAX_NAME_SIZE];
  char captions[SUPLA_SUPLET_MAX_CHANNELS_PER_INSTANCE]
               [SUPLA_SUPLET_MAX_CAPTION_SIZE];
  char parameterKeys[SUPLA_SUPLET_MAX_PARAMETERS]
                    [SUPLA_SUPLET_MAX_PARAMETER_KEY_SIZE];
  char parameterDefaultText[SUPLA_SUPLET_MAX_PARAMETERS]
                           [SUPLA_SUPLET_MAX_PARAMETER_TEXT_SIZE];
  char parameterEnumValues[SUPLA_SUPLET_MAX_PARAMETERS]
                          [SUPLA_SUPLET_MAX_PARAMETER_TEXT_SIZE];
};

class JsonDefinitionParser {
 public:
  static bool parse(const char *json, JsonDefinition *output);
  static bool parseCategory(const char *value, Category *category);
  static bool parseKind(const char *value, Kind *kind);
  static bool parseChannelKind(const char *value, ChannelKind *kind);
  static bool parseParameterType(const char *value, ParameterType *type);
  static bool parseParameterLifecycle(const char *value,
                                      ParameterLifecycle *lifecycle);
  static bool parseDefaultFunction(const char *value, int32_t *function);
};

}  // namespace Suplet
}  // namespace Supla

#endif  // SRC_SUPLA_SUPLET_JSON_DEFINITION_H_

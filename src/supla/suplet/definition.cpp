// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <supla/suplet/config.h>

#if SUPLA_SUPLET_ENABLED

#include <supla/suplet/definition.h>

namespace Supla {
namespace Suplet {

bool getDefinitionChannelIds(const Definition &definition,
                             uint8_t *output,
                             uint8_t outputSize) {
  if (definition.channelCount > 0 &&
      (definition.channels == nullptr || output == nullptr)) {
    return false;
  }
  if (definition.channelCount > outputSize) {
    return false;
  }

  for (uint8_t i = 0; i < definition.channelCount; i++) {
    if (definition.channels[i].channelId == kInvalidChannelId) {
      return false;
    }
    for (uint8_t j = i + 1; j < definition.channelCount; j++) {
      if (definition.channels[i].channelId ==
          definition.channels[j].channelId) {
        return false;
      }
    }
    output[i] = definition.channels[i].channelId;
  }

  return true;
}

}  // namespace Suplet
}  // namespace Supla

#endif  // SUPLA_SUPLET_ENABLED

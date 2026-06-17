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

#include <supla/suplet/definition.h>

namespace Supla {
namespace Suplet {

uint32_t channelKeyFromString(const char *value) {
  if (value == nullptr || value[0] == 0) {
    return kInvalidChannelKey;
  }

  uint32_t hash = 2166136261u;
  while (*value) {
    hash ^= static_cast<uint8_t>(*value);
    hash *= 16777619u;
    value++;
  }

  return hash == kInvalidChannelKey ? 1 : hash;
}

bool getRequiredChannelKeys(const Definition &definition,
                            uint32_t *output,
                            uint8_t outputSize) {
  if (definition.channelCount > 0 &&
      (definition.channels == nullptr || output == nullptr)) {
    return false;
  }
  if (definition.channelCount > outputSize) {
    return false;
  }

  for (uint8_t i = 0; i < definition.channelCount; i++) {
    if (definition.channels[i].channelKey == kInvalidChannelKey) {
      return false;
    }
    for (uint8_t j = i + 1; j < definition.channelCount; j++) {
      if (definition.channels[i].channelKey ==
          definition.channels[j].channelKey) {
        return false;
      }
    }
    output[i] = definition.channels[i].channelKey;
  }

  return true;
}

}  // namespace Suplet
}  // namespace Supla

#endif  // SUPLA_SUPLET_ENABLED

// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <supla/suplet/config.h>

#if SUPLA_SUPLET_ENABLED

#include <supla/suplet/channel_map.h>

namespace Supla {
namespace Suplet {

uint8_t ChannelMap::getCount() const {
  return count;
}

void ChannelMap::clear() {
  for (uint8_t i = 0; i < count; i++) {
    mappings[i] = {};
  }
  count = 0;
}

bool ChannelMap::add(uint8_t channelId, int channelNumber) {
  if (channelId == kInvalidChannelId ||
      channelNumber < 0 ||
      channelNumber >= SUPLA_CHANNELMAXCOUNT ||
      count >= SUPLA_SUPLET_MAX_CHANNELS_PER_INSTANCE ||
      containsId(channelId)) {
    return false;
  }

  for (uint8_t i = 0; i < count; i++) {
    if (mappings[i].channelNumber == channelNumber) {
      return false;
    }
  }

  mappings[count].channelId = channelId;
  mappings[count].channelNumber = channelNumber;
  count++;
  return true;
}

bool ChannelMap::containsId(uint8_t channelId) const {
  return getChannelNumber(channelId) != kInvalidChannelNumber;
}

int ChannelMap::getChannelNumber(uint8_t channelId) const {
  for (uint8_t i = 0; i < count; i++) {
    if (mappings[i].channelId == channelId) {
      return mappings[i].channelNumber;
    }
  }
  return kInvalidChannelNumber;
}

const ChannelMapping *ChannelMap::getMapping(uint8_t index) const {
  if (index >= count) {
    return nullptr;
  }
  return &mappings[index];
}

}  // namespace Suplet
}  // namespace Supla

#endif  // SUPLA_SUPLET_ENABLED

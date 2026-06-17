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

#include <supla/suplet/channel_allocator.h>

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

bool ChannelMap::add(uint32_t channelKey, int channelNumber) {
  if (channelKey == kInvalidChannelKey ||
      channelNumber < 0 ||
      channelNumber >= SUPLA_CHANNELMAXCOUNT ||
      count >= SUPLA_SUPLET_MAX_CHANNELS_PER_INSTANCE ||
      containsKey(channelKey) ||
      containsChannelNumber(channelNumber)) {
    return false;
  }

  mappings[count].channelKey = channelKey;
  mappings[count].channelNumber = channelNumber;
  count++;
  return true;
}

bool ChannelMap::remove(uint32_t channelKey) {
  for (uint8_t i = 0; i < count; i++) {
    if (mappings[i].channelKey == channelKey) {
      for (uint8_t j = i; j + 1 < count; j++) {
        mappings[j] = mappings[j + 1];
      }
      count--;
      mappings[count] = {};
      return true;
    }
  }
  return false;
}

bool ChannelMap::containsKey(uint32_t channelKey) const {
  return getChannelNumber(channelKey) != kInvalidChannelNumber;
}

bool ChannelMap::containsChannelNumber(int channelNumber) const {
  for (uint8_t i = 0; i < count; i++) {
    if (mappings[i].channelNumber == channelNumber) {
      return true;
    }
  }
  return false;
}

int ChannelMap::getChannelNumber(uint32_t channelKey) const {
  for (uint8_t i = 0; i < count; i++) {
    if (mappings[i].channelKey == channelKey) {
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

bool ChannelAllocator::markOccupied(int channelNumber) {
  if (channelNumber < 0 || channelNumber >= SUPLA_CHANNELMAXCOUNT) {
    return false;
  }
  occupied[channelNumber] = true;
  return true;
}

bool ChannelAllocator::markFromMap(const ChannelMap &map) {
  for (uint8_t i = 0; i < map.getCount(); i++) {
    auto mapping = map.getMapping(i);
    if (mapping == nullptr || !markOccupied(mapping->channelNumber)) {
      return false;
    }
  }
  return true;
}

bool ChannelAllocator::isOccupied(int channelNumber) const {
  if (channelNumber < 0 || channelNumber >= SUPLA_CHANNELMAXCOUNT) {
    return false;
  }
  return occupied[channelNumber];
}

uint8_t ChannelAllocator::getFreeChannelCount() const {
  uint8_t result = 0;
  for (int i = 0; i < SUPLA_CHANNELMAXCOUNT; i++) {
    if (!occupied[i]) {
      result++;
    }
  }
  return result;
}

void ChannelAllocator::clearOccupied() {
  for (int i = 0; i < SUPLA_CHANNELMAXCOUNT; i++) {
    occupied[i] = false;
  }
}

bool ChannelAllocator::allocateMissing(ChannelMap *map,
                                       const uint32_t *requiredChannelKeys,
                                       uint8_t requiredChannelKeyCount) {
  if (map == nullptr ||
      (requiredChannelKeyCount > 0 && requiredChannelKeys == nullptr)) {
    return false;
  }

  for (uint8_t i = 0; i < requiredChannelKeyCount; i++) {
    if (requiredChannelKeys[i] == kInvalidChannelKey) {
      return false;
    }
    for (uint8_t j = i + 1; j < requiredChannelKeyCount; j++) {
      if (requiredChannelKeys[i] == requiredChannelKeys[j]) {
        return false;
      }
    }
  }

  ChannelMap result = *map;
  ChannelAllocator allocationState = *this;
  if (!allocationState.markFromMap(result)) {
    return false;
  }

  for (uint8_t i = 0; i < requiredChannelKeyCount; i++) {
    uint32_t channelKey = requiredChannelKeys[i];
    if (result.containsKey(channelKey)) {
      continue;
    }

    int channelNumber = allocationState.findFirstFreeChannel();
    if (channelNumber == kInvalidChannelNumber ||
        !result.add(channelKey, channelNumber) ||
        !allocationState.markOccupied(channelNumber)) {
      return false;
    }
  }

  *map = result;
  return true;
}

int ChannelAllocator::findFirstFreeChannel() const {
  for (int i = 0; i < SUPLA_CHANNELMAXCOUNT; i++) {
    if (!occupied[i]) {
      return i;
    }
  }
  return kInvalidChannelNumber;
}

}  // namespace Suplet
}  // namespace Supla

#endif  // SUPLA_SUPLET_ENABLED

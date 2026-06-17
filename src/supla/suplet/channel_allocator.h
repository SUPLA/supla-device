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

#ifndef SRC_SUPLA_SUPLET_CHANNEL_ALLOCATOR_H_
#define SRC_SUPLA_SUPLET_CHANNEL_ALLOCATOR_H_

#include <stdint.h>
#include <supla-common/proto.h>

#ifndef SUPLA_SUPLET_MAX_CHANNELS_PER_INSTANCE
#define SUPLA_SUPLET_MAX_CHANNELS_PER_INSTANCE 32
#endif

namespace Supla {
namespace Suplet {

constexpr int kInvalidChannelNumber = -1;
constexpr uint32_t kInvalidChannelKey = 0;

struct ChannelMapping {
  uint32_t channelKey = kInvalidChannelKey;
  int16_t channelNumber = kInvalidChannelNumber;
};

class ChannelMap {
 public:
  uint8_t getCount() const;
  void clear();

  bool add(uint32_t channelKey, int channelNumber);
  bool remove(uint32_t channelKey);
  bool containsKey(uint32_t channelKey) const;
  bool containsChannelNumber(int channelNumber) const;
  int getChannelNumber(uint32_t channelKey) const;
  const ChannelMapping *getMapping(uint8_t index) const;

 private:
  ChannelMapping mappings[SUPLA_SUPLET_MAX_CHANNELS_PER_INSTANCE] = {};
  uint8_t count = 0;
};

class ChannelAllocator {
 public:
  bool markOccupied(int channelNumber);
  bool markFromMap(const ChannelMap &map);
  bool isOccupied(int channelNumber) const;
  uint8_t getFreeChannelCount() const;
  void clearOccupied();

  bool allocateMissing(ChannelMap *map,
                       const uint32_t *requiredChannelKeys,
                       uint8_t requiredChannelKeyCount);

 private:
  int findFirstFreeChannel() const;
  bool occupied[SUPLA_CHANNELMAXCOUNT] = {};
};

}  // namespace Suplet
}  // namespace Supla

#endif  // SRC_SUPLA_SUPLET_CHANNEL_ALLOCATOR_H_

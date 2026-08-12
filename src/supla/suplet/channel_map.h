// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_SUPLET_CHANNEL_MAP_H_
#define SRC_SUPLA_SUPLET_CHANNEL_MAP_H_

#include <stdint.h>
#include <supla-common/proto.h>

#ifndef SUPLA_SUPLET_MAX_CHANNELS_PER_INSTANCE
#define SUPLA_SUPLET_MAX_CHANNELS_PER_INSTANCE 32
#endif

namespace Supla {
namespace Suplet {

constexpr int kInvalidChannelNumber = -1;
constexpr uint8_t kInvalidChannelId = 0;

struct ChannelMapping {
  uint8_t channelId = kInvalidChannelId;
  int16_t channelNumber = kInvalidChannelNumber;
};

class ChannelMap {
 public:
  uint8_t getCount() const;
  void clear();

  bool add(uint8_t channelId, int channelNumber);
  bool containsId(uint8_t channelId) const;
  int getChannelNumber(uint8_t channelId) const;
  const ChannelMapping *getMapping(uint8_t index) const;

 private:
  ChannelMapping mappings[SUPLA_SUPLET_MAX_CHANNELS_PER_INSTANCE] = {};
  uint8_t count = 0;
};

}  // namespace Suplet
}  // namespace Supla

#endif  // SRC_SUPLA_SUPLET_CHANNEL_MAP_H_

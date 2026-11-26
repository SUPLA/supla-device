// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_CHANNELS_IMPULSE_COUNTER_CHANNEL_H_
#define SRC_SUPLA_CHANNELS_IMPULSE_COUNTER_CHANNEL_H_

#include "channel.h"

namespace Supla {
namespace Sensor {
class VirtualImpulseCounter;
}  // namespace Sensor

class ImpulseCounterChannel : public Channel {
 public:
  bool getImpulseCounterValue(double *value, const char **unit) override;
  double calculate(uint64_t count) const;

 private:
  friend class Sensor::VirtualImpulseCounter;

  TChannelConfig_ImpulseCounter config = {};
};

}  // namespace Supla

#endif  // SRC_SUPLA_CHANNELS_IMPULSE_COUNTER_CHANNEL_H_

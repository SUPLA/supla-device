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

#ifndef SRC_SUPLA_SENSOR_WEIGHT_H_
#define SRC_SUPLA_SENSOR_WEIGHT_H_

#include "supla/channel_element.h"
#include "supla/element.h"

#define WEIGHT_NOT_AVAILABLE -1

namespace Supla {
namespace Sensor {
class Weight : public ChannelElement {
 public:
  Weight() {
    channel.setType(SUPLA_CHANNELTYPE_WEIGHTSENSOR);
    channel.setDefault(SUPLA_CHANNELFNC_WEIGHTSENSOR);
    channel.setNewValue(WEIGHT_NOT_AVAILABLE);
  }

  virtual double getValue() {
    return WEIGHT_NOT_AVAILABLE;
  }

  void iterateAlways() {
    if (lastReadTime + 10000 < millis()) {
      lastReadTime = millis();
      channel.setNewValue(getValue());
    }
  }

 protected:
  uint64_t lastReadTime = 0;
};

};  // namespace Sensor
};  // namespace Supla

#endif  // SRC_SUPLA_SENSOR_WEIGHT_H_

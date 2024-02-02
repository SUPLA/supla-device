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

#ifndef SRC_SUPLA_SENSOR_VIRTUAL_IMPULSE_COUNTER_H_
#define SRC_SUPLA_SENSOR_VIRTUAL_IMPULSE_COUNTER_H_

#include <supla-common/proto.h>
#include <supla/action_handler.h>
#include <supla/channel_element.h>

namespace Supla {

namespace Sensor {
class VirtualImpulseCounter : public ChannelElement, public ActionHandler {
 public:
  VirtualImpulseCounter();

  void onInit() override;
  void onLoadState() override;
  void onSaveState() override;
  void handleAction(int event, int action) override;

  // Returns value of a counter at given Supla channel
  uint64_t getCounter() const;

  // Set counter to a given value
  void setCounter(uint64_t value);

  // Increment the counter by 1
  void incCounter();

 protected:
  uint64_t counter = 0;  // Actual count of impulses
};

}  // namespace Sensor
}  // namespace Supla

#endif  // SRC_SUPLA_SENSOR_VIRTUAL_IMPULSE_COUNTER_H_

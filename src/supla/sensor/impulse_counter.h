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

#ifndef SRC_SUPLA_SENSOR_IMPULSE_COUNTER_H_
#define SRC_SUPLA_SENSOR_IMPULSE_COUNTER_H_

#include <supla-common/proto.h>
#include <supla/sensor/virtual_impulse_counter.h>

namespace Supla {

class Io;

namespace Sensor {
class ImpulseCounter : public VirtualImpulseCounter {
 public:
  ImpulseCounter(Supla::Io *io,
                 int _impulsePin,
                 bool _detectLowToHigh = false,
                 bool inputPullup = true,
                 unsigned int _debounceDelay = 10);
  ImpulseCounter(int _impulsePin,
                 bool _detectLowToHigh = false,
                 bool inputPullup = true,
                 unsigned int _debounceDelay = 10);

  void onInit() override;
  void onFastTimer() override;

 protected:
  Supla::Io *io = nullptr;
  uint32_t lastImpulseMillis =
      0;  // Stores timestamp of last impulse (used to ignore
          // changes of state during 10 ms timeframe)

  int16_t impulsePin = -1;  // Pin where impulses are counted
  uint16_t debounceDelay = 10;

  bool detectLowToHigh = false;  // defines if we count raining (LOW to HIGH) or
                                 // falling (HIGH to LOW) edge
  bool inputPullup = true;
  int8_t prevState = 0;  // Store previous state of pin (LOW/HIGH). It is used
                         // to track changes on pin state.
};

}  // namespace Sensor
}  // namespace Supla

#endif  // SRC_SUPLA_SENSOR_IMPULSE_COUNTER_H_

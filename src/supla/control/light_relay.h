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

#ifndef SRC_SUPLA_CONTROL_LIGHT_RELAY_H_
#define SRC_SUPLA_CONTROL_LIGHT_RELAY_H_

#include "relay.h"

namespace Supla {
namespace Control {
class LightRelay : public Relay {
 public:
  explicit LightRelay(int pin, bool highIsOn = true);
  void handleGetChannelState(TDSC_ChannelState *channelState);
  int handleCalcfgFromServer(TSD_DeviceCalCfgRequest *request);
  void onLoadState();
  void onSaveState();
  void turnOn(_supla_int_t duration = 0);
  void iterateAlways();

 protected:
  uint16_t lifespan;
  _supla_int_t turnOnSecondsCumulative;
  uint32_t turnOnTimestamp;
};

};  // namespace Control
};  // namespace Supla

#endif  // SRC_SUPLA_CONTROL_LIGHT_RELAY_H_

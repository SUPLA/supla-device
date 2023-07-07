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

/* BistableRelay
 * This class can be used to controll bistable relay.
 * Supla device will send short impulses (<0.5 s) on GPIO to toggle bistable
 * relay state.
 * Device does not have knowledge about the status of bistable relay, so it
 * has to be read on a different GPIO (statusPin)
 * This class can work without statusPin information, but Supla will lose
 * information about status of bistable relay.
 */

#ifndef SRC_SUPLA_CONTROL_BISTABLE_RELAY_H_
#define SRC_SUPLA_CONTROL_BISTABLE_RELAY_H_

#include "relay.h"

namespace Supla {
namespace Control {
class BistableRelay : public Relay {
 public:
  BistableRelay(Supla::Io *io,
                int pin,
                int statusPin = -1,
                bool statusPullUp = true,
                bool statusHighIsOn = true,
                bool highIsOn = true,
                _supla_int_t functions =
                    (0xFF ^ SUPLA_BIT_FUNC_CONTROLLINGTHEROLLERSHUTTER));
  BistableRelay(int pin,
                int statusPin = -1,
                bool statusPullUp = true,
                bool statusHighIsOn = true,
                bool highIsOn = true,
                _supla_int_t functions =
                    (0xFF ^ SUPLA_BIT_FUNC_CONTROLLINGTHEROLLERSHUTTER));
  void onInit() override;
  void iterateAlways() override;
  int handleNewValueFromServer(TSD_SuplaChannelNewValue *newValue) override;
  void turnOn(_supla_int_t duration = 0) override;
  void turnOff(_supla_int_t duration = 0) override;
  void toggle(_supla_int_t duration = 0) override;

  bool isOn() override;
  bool isStatusUnknown();

 protected:
  void internalToggle();

  int statusPin = -1;
  bool statusPullUp = true;
  bool statusHighIsOn = true;
  uint32_t disarmTimeMs = 0;
  uint32_t lastReadTime = 0;
  bool busy = false;
};

};  // namespace Control
};  // namespace Supla

#endif  // SRC_SUPLA_CONTROL_BISTABLE_RELAY_H_

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

/*
 * This class allows to control roller shutters with 3 buttons: up, down, stop
 */

#ifndef SRC_SUPLA_CONTROL_TRIPPLE_BUTTON_ROLLER_SHUTTER_H_
#define SRC_SUPLA_CONTROL_TRIPPLE_BUTTON_ROLLER_SHUTTER_H_

#include "bistable_roller_shutter.h"

namespace Supla {
namespace Control {
class TrippleButtonRollerShutter : public BistableRollerShutter {
 public:
  TrippleButtonRollerShutter(
      Supla::Io *io, int pinUp, int pinDown, int pinStop, bool highIsOn = true);
  TrippleButtonRollerShutter(int pinUp,
                             int pinDown,
                             int pinStop,
                             bool highIsOn = true);
  virtual ~TrippleButtonRollerShutter();

  void onInit() override;

 protected:
  void stopMovement() override;
  void switchOffRelays() override;
  bool inMove() override;
  virtual void relayStopOn();
  virtual void relayStopOff();

  int pinStop = 0;
};

};  // namespace Control
};  // namespace Supla

#endif  // SRC_SUPLA_CONTROL_TRIPPLE_BUTTON_ROLLER_SHUTTER_H_

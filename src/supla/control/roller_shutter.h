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

#ifndef SRC_SUPLA_CONTROL_ROLLER_SHUTTER_H_
#define SRC_SUPLA_CONTROL_ROLLER_SHUTTER_H_

#include "roller_shutter_interface.h"

namespace Supla {

class Io;

namespace Control {

class RollerShutter : public RollerShutterInterface {
 public:
  RollerShutter(Supla::Io *io, int pinUp, int pinDown, bool highIsOn = true);
  RollerShutter(int pinUp, int pinDown, bool highIsOn = true);

  void onInit() override;
  void onTimer() override;

 protected:
  virtual void stopMovement();
  virtual void relayDownOn();
  virtual void relayUpOn();
  virtual void relayDownOff();
  virtual void relayUpOff();
  virtual void startClosing();
  virtual void startOpening();
  virtual void switchOffRelays();

  int16_t pinUp = -1;
  int16_t pinDown = -1;

  uint32_t lastMovementStartTime = 0;
  uint32_t doNothingTime = 0;
  Supla::Io *io = nullptr;

  uint32_t operationTimeoutMs = 0;

  bool highIsOn = true;
};

}  // namespace Control
}  // namespace Supla

#endif  // SRC_SUPLA_CONTROL_ROLLER_SHUTTER_H_

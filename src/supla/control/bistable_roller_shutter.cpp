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

#include "bistable_roller_shutter.h"

#include <supla/storage/storage.h>
#include <supla/time.h>
#include <supla/io.h>

namespace Supla {
namespace Control {

BistableRollerShutter::BistableRollerShutter(Supla::Io *io,
                                             int pinUp,
                                             int pinDown,
                                             bool highIsOn)
    : RollerShutter(io, pinUp, pinDown, highIsOn) {
}

BistableRollerShutter::BistableRollerShutter(int pinUp,
                                             int pinDown,
                                             bool highIsOn)
    : RollerShutter(pinUp, pinDown, highIsOn) {
}

void BistableRollerShutter::stopMovement() {
  if (currentDirection == UP_DIR) {
    relayUpOn();
  } else if (currentDirection == DOWN_DIR) {
    relayDownOn();
  }
  currentDirection = STOP_DIR;
  doNothingTime = millis();
  // Schedule save in 5 s after stop movement of roller shutter
  Supla::Storage::ScheduleSave(5000);
}

void BistableRollerShutter::relayDownOn() {
  activeBiRelay = true;
  toggleTime = millis();
  Supla::Io::digitalWrite(
      channel.getChannelNumber(), pinDown, highIsOn ? HIGH : LOW, io);
}

void BistableRollerShutter::relayUpOn() {
  activeBiRelay = true;
  toggleTime = millis();
  Supla::Io::digitalWrite(
      channel.getChannelNumber(), pinUp, highIsOn ? HIGH : LOW, io);
}

void BistableRollerShutter::relayDownOff() {
  activeBiRelay = false;
  Supla::Io::digitalWrite(
      channel.getChannelNumber(), pinDown, highIsOn ? LOW : HIGH, io);
}

void BistableRollerShutter::relayUpOff() {
  activeBiRelay = false;
  Supla::Io::digitalWrite(
      channel.getChannelNumber(), pinUp, highIsOn ? LOW : HIGH, io);
}

void BistableRollerShutter::onTimer() {
  if (activeBiRelay && millis() - toggleTime > 200) {
    switchOffRelays();
    doNothingTime = millis();
  }
  if (activeBiRelay) {
    return;
  }

  RollerShutter::onTimer();
}

};  // namespace Control
};  // namespace Supla

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

#include "tripple_button_roller_shutter.h"

#include <supla/storage/storage.h>
#include <supla/time.h>
#include <supla/io.h>

namespace Supla {
namespace Control {

TrippleButtonRollerShutter::TrippleButtonRollerShutter(
    Supla::Io *io, int pinUp, int pinDown, int pinStop, bool highIsOn)
    : BistableRollerShutter(io, pinUp, pinDown, highIsOn), pinStop(pinStop) {
}

TrippleButtonRollerShutter::TrippleButtonRollerShutter(int pinUp,
                                                       int pinDown,
                                                       int pinStop,
                                                       bool highIsOn)
    : BistableRollerShutter(pinUp, pinDown, highIsOn), pinStop(pinStop) {
}

TrippleButtonRollerShutter::~TrippleButtonRollerShutter() {
}

void TrippleButtonRollerShutter::onInit() {
  Supla::Io::digitalWrite(
      channel.getChannelNumber(), pinStop, highIsOn ? LOW : HIGH, io);
  Supla::Io::pinMode(channel.getChannelNumber(), pinStop, OUTPUT, io);

  BistableRollerShutter::onInit();
}

void TrippleButtonRollerShutter::stopMovement() {
  relayStopOn();
  currentDirection = STOP_DIR;
  doNothingTime = millis();
  // Schedule save in 5 s after stop movement of roller shutter
  Supla::Storage::ScheduleSave(5000);
}

void TrippleButtonRollerShutter::relayStopOn() {
  activeBiRelay = true;
  toggleTime = millis();
  Supla::Io::digitalWrite(
      channel.getChannelNumber(), pinStop, highIsOn ? HIGH : LOW, io);
}

void TrippleButtonRollerShutter::relayStopOff() {
  activeBiRelay = false;
  Supla::Io::digitalWrite(
      channel.getChannelNumber(), pinStop, highIsOn ? LOW : HIGH, io);
}

void TrippleButtonRollerShutter::switchOffRelays() {
  relayUpOff();
  relayDownOff();
  relayStopOff();
}

bool TrippleButtonRollerShutter::inMove() {
  bool result = false;
  if (newTargetPositionAvailable && targetPosition == STOP_POSITION) {
    result = true;
    newTargetPositionAvailable = false;
  }
  return result || currentDirection != STOP_DIR;
}

};  // namespace Control
};  // namespace Supla

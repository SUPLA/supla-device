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

namespace {

Supla::Io::IoPin MakeOutputPin(Supla::Io::Base *io,
                               int pin,
                               bool highIsOn) {
  Supla::Io::IoPin result(pin, io);
  result.setActiveHigh(highIsOn);
  result.setMode(OUTPUT);
  return result;
}

}  // namespace

TrippleButtonRollerShutter::TrippleButtonRollerShutter(
    Supla::Io::Base  *io, int pinUp, int pinDown, int pinStop, bool highIsOn)
    : BistableRollerShutter(io, pinUp, pinDown, highIsOn),
      pinStop(MakeOutputPin(io, pinStop, highIsOn)) {
}

TrippleButtonRollerShutter::TrippleButtonRollerShutter(int pinUp,
                                                       int pinDown,
                                                       int pinStop,
                                                       bool highIsOn)
    : BistableRollerShutter(pinUp, pinDown, highIsOn),
      pinStop(MakeOutputPin(nullptr, pinStop, highIsOn)) {
}

TrippleButtonRollerShutter::TrippleButtonRollerShutter(
    Supla::Io::IoPin pinUp,
    Supla::Io::IoPin pinDown,
    Supla::Io::IoPin pinStop)
    : BistableRollerShutter(pinUp, pinDown), pinStop(pinStop) {
}

TrippleButtonRollerShutter::~TrippleButtonRollerShutter() {
}

void TrippleButtonRollerShutter::onInit() {
  pinStop.writeInactive(channel.getChannelNumber());
  pinStop.pinMode(channel.getChannelNumber());

  BistableRollerShutter::onInit();
}

void TrippleButtonRollerShutter::stopMovement() {
  relayStopOn();
  currentDirection = Directions::STOP_DIR;
  doNothingTime = millis();
  // Schedule save in 5 s after stop movement of roller shutter
  Supla::Storage::ScheduleSave(5000, 1000);
}

void TrippleButtonRollerShutter::relayStopOn() {
  activeBiRelay = true;
  toggleTime = millis();
  pinStop.writeActive(channel.getChannelNumber());
}

void TrippleButtonRollerShutter::relayStopOff() {
  activeBiRelay = false;
  pinStop.writeInactive(channel.getChannelNumber());
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
  return result || currentDirection != Directions::STOP_DIR;
}

};  // namespace Control
};  // namespace Supla

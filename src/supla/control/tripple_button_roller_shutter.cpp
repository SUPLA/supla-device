// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

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
  this->pinStop.setMode(OUTPUT);
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
  operationTimeoutMs = 0;
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

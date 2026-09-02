// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "bistable_roller_shutter.h"

#include <supla/storage/storage.h>
#include <supla/time.h>
#include <supla/io.h>

namespace Supla {
namespace Control {

BistableRollerShutter::BistableRollerShutter(Supla::Io::IoPin pinUp,
                                             Supla::Io::IoPin pinDown)
    : RollerShutter(pinUp, pinDown) {
}

BistableRollerShutter::BistableRollerShutter(Supla::Io::Base *io,
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
  if (currentDirection == Directions::UP_DIR) {
    relayUpOn();
  } else if (currentDirection == Directions::DOWN_DIR) {
    relayDownOn();
  }
  currentDirection = Directions::STOP_DIR;
  operationTimeoutMs = 0;
  doNothingTime = millis();
  // Schedule save in 5 s after stop movement of roller shutter
  Supla::Storage::ScheduleSave(5000, 2000);
}

void BistableRollerShutter::relayDownOn() {
  activeBiRelay = true;
  toggleTime = millis();
  pinDown.writeActive(channel.getChannelNumber());
}

void BistableRollerShutter::relayUpOn() {
  activeBiRelay = true;
  toggleTime = millis();
  pinUp.writeActive(channel.getChannelNumber());
}

void BistableRollerShutter::relayDownOff() {
  activeBiRelay = false;
  pinDown.writeInactive(channel.getChannelNumber());
}

void BistableRollerShutter::relayUpOff() {
  activeBiRelay = false;
  pinUp.writeInactive(channel.getChannelNumber());
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

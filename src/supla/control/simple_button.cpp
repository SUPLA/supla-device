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

#include "supla/control/simple_button.h"

#include <supla/io.h>
#include <supla/time.h>
#include <supla/log_wrapper.h>

using Supla::Control::SimpleButton;
using Supla::Control::ButtonState;

ButtonState::ButtonState(Supla::Io *io,
                                         int pin,
                                         bool pullUp,
                                         bool invertLogic)
    : ButtonState(pin, pullUp, invertLogic) {
  this->io = io;
}

ButtonState::ButtonState(int pin, bool pullUp, bool invertLogic)
    : pin(pin), pullUp(pullUp), invertLogic(invertLogic) {
}

enum Supla::Control::StateResults ButtonState::update() {
  uint32_t curMillis = millis();
  if (debounceDelayMs == 0 ||
      curMillis - debounceTimestampMs > debounceDelayMs) {
    int currentState = Supla::Io::digitalRead(pin, io);
    if (currentState != prevState) {
      // If status is changed, then make sure that it will be kept at
      // least swNoiseFilterDelayMs ms to avoid noise
      if (swNoiseFilterDelayMs != 0 && currentState != newStatusCandidate) {
        newStatusCandidate = currentState;
        filterTimestampMs = curMillis;
      } else if (curMillis - filterTimestampMs > swNoiseFilterDelayMs) {
        // If new status is kept at least swNoiseFilterDelayMs ms, then apply
        // change of status
        debounceTimestampMs = curMillis;
        prevState = currentState;
        if (currentState == valueOnPress()) {
          return TO_PRESSED;
        } else {
          return TO_RELEASED;
        }
      }
    } else {
      // If current status is the same as prevState, then reset
      // new status candidate
      newStatusCandidate = prevState;
    }
  }
  if (prevState == valueOnPress()) {
    return PRESSED;
  } else {
    return RELEASED;
  }
}

enum Supla::Control::StateResults ButtonState::getLastState()
    const {
  if (prevState == valueOnPress()) {
    return PRESSED;
  } else {
    return RELEASED;
  }
}

SimpleButton::SimpleButton(Supla::Io *io,
                                           int pin,
                                           bool pullUp,
                                           bool invertLogic)
    : state(io, pin, pullUp, invertLogic) {
}

SimpleButton::SimpleButton(int pin,
                                           bool pullUp,
                                           bool invertLogic)
    : state(pin, pullUp, invertLogic) {
}

void SimpleButton::onTimer() {
  enum Supla::Control::StateResults stateResult = state.update();
  if (stateResult == TO_PRESSED) {
    runAction(ON_PRESS);
    runAction(ON_CHANGE);
  } else if (stateResult == TO_RELEASED) {
    runAction(ON_RELEASE);
    runAction(ON_CHANGE);
  }
}

void SimpleButton::onInit() {
  state.init(getButtonNumber());
}

void ButtonState::init(int buttonNumber) {
  if (prevState == -1) {
    Supla::Io::pinMode(pin, pullUp ? INPUT_PULLUP : INPUT, io);
    prevState = Supla::Io::digitalRead(pin, io);
    newStatusCandidate = prevState;
    SUPLA_LOG_DEBUG(
        "Button[%d]: Initialized: pin %d, pullUp %d, invertLogic %d, state %d",
        buttonNumber,
        pin,
        pullUp,
        invertLogic,
        prevState);
  }
}

int ButtonState::valueOnPress() const {
  return invertLogic ? LOW : HIGH;
}

void SimpleButton::setSwNoiseFilterDelay(
    unsigned int newDelayMs) {
  state.setSwNoiseFilterDelay(newDelayMs);
}
void ButtonState::setSwNoiseFilterDelay(
    unsigned int newDelayMs) {
  swNoiseFilterDelayMs = newDelayMs;
}

void SimpleButton::setDebounceDelay(unsigned int newDelayMs) {
  state.setDebounceDelay(newDelayMs);
}

void ButtonState::setDebounceDelay(unsigned int newDelayMs) {
  debounceDelayMs = newDelayMs;
}

int8_t SimpleButton::getButtonNumber() const {
  return state.getGpio();
}

int ButtonState::getGpio() const {
  return pin;
}

enum Supla::Control::StateResults SimpleButton::getLastState() const {
  return state.getLastState();
}


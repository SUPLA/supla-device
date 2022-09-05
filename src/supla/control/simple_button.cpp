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

#include "../io.h"
#include "../time.h"
#include "button.h"
#include "supla/control/simple_button.h"
#include "supla/storage/config.h"

Supla::Control::ButtonState::ButtonState(int pin, bool pullUp, bool invertLogic)
    : debounceTimeMs(0),
      filterTimeMs(0),
      debounceDelayMs(50),
      swNoiseFilterDelayMs(20),
      pin(pin),
      newStatusCandidate(LOW),
      prevState(LOW),
      pullUp(pullUp),
      invertLogic(invertLogic) {
}

int Supla::Control::ButtonState::update() {
  uint64_t curMillis = millis();
  if (debounceDelayMs == 0 || curMillis - debounceTimeMs > debounceDelayMs) {
    int currentState = Supla::Io::digitalRead(pin);
    if (currentState != prevState) {
      // If status is changed, then make sure that it will be kept at
      // least swNoiseFilterDelayMs ms to avoid noise
      if (swNoiseFilterDelayMs != 0 && currentState != newStatusCandidate) {
        newStatusCandidate = currentState;
        filterTimeMs = curMillis;
      } else if (curMillis - filterTimeMs > swNoiseFilterDelayMs) {
        // If new status is kept at least swNoiseFilterDelayMs ms, then apply
        // change of status
        debounceTimeMs = curMillis;
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

Supla::Control::SimpleButton::SimpleButton(int pin,
                                           bool pullUp,
                                           bool invertLogic)
    : state(pin, pullUp, invertLogic) {
}

void Supla::Control::SimpleButton::onTimer() {
  int stateResult = state.update();
  if (stateResult == TO_PRESSED) {
    runAction(ON_PRESS);
    runAction(ON_CHANGE);
  } else if (stateResult == TO_RELEASED) {
    runAction(ON_RELEASE);
    runAction(ON_CHANGE);
  }
}

void Supla::Control::SimpleButton::onInit() {
  state.init();
}

void Supla::Control::ButtonState::init() {
  Supla::Io::pinMode(pin, pullUp ? INPUT_PULLUP : INPUT);
  prevState = Supla::Io::digitalRead(pin);
  newStatusCandidate = prevState;
}

int Supla::Control::ButtonState::valueOnPress() {
  return invertLogic ? LOW : HIGH;
}

void Supla::Control::SimpleButton::setSwNoiseFilterDelay(
    unsigned int newDelayMs) {
  state.setSwNoiseFilterDelay(newDelayMs);
}
void Supla::Control::ButtonState::setSwNoiseFilterDelay(
    unsigned int newDelayMs) {
  swNoiseFilterDelayMs = newDelayMs;
}

void Supla::Control::SimpleButton::setDebounceDelay(unsigned int newDelayMs) {
  state.setDebounceDelay(newDelayMs);
}

void Supla::Control::ButtonState::setDebounceDelay(unsigned int newDelayMs) {
  debounceDelayMs = newDelayMs;
}


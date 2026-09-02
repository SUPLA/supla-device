// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "supla/control/simple_button.h"

#include <supla/io.h>
#include <supla/time.h>
#include <supla/log_wrapper.h>

using Supla::Control::SimpleButton;
using Supla::Control::ButtonState;

ButtonState::ButtonState(Supla::Io::IoPin inputPin)
    : inputPin(inputPin) {
  this->inputPin.setMode(INPUT);
}

ButtonState::ButtonState(Supla::Io::Base *io,
                         int pin,
                         bool pullUp,
                         bool invertLogic)
    : ButtonState(Supla::Io::IoPin(pin, io)) {
  this->inputPin.setPullUp(pullUp);
  this->inputPin.setActiveHigh(!invertLogic);
}

ButtonState::ButtonState(int pin, bool pullUp, bool invertLogic)
    : ButtonState(Supla::Io::IoPin(pin)) {
  this->inputPin.setPullUp(pullUp);
  this->inputPin.setActiveHigh(!invertLogic);
}

SimpleButton::SimpleButton(Supla::Io::IoPin inputPin)
    : state(inputPin) {
}

enum Supla::Control::StateResults ButtonState::update() {
  if (!inputPin.isSet()) {
    return RELEASED;
  }
  uint32_t curMillis = millis();
  if (debounceDelayMs == 0 ||
      curMillis - debounceTimestampMs > debounceDelayMs) {
    int currentState = inputPin.digitalRead();
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

bool ButtonState::isReady() const {
  if (inputPin.io && !inputPin.io->isReady()) {
    return false;
  }
  return true;
}

SimpleButton::SimpleButton(Supla::Io::Base *io,
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
  if (!state.isReady()) {
    return;
  }
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
    if (inputPin.isSet()) {
      inputPin.pinMode(buttonNumber);
      prevState = inputPin.digitalRead(buttonNumber);
    }
    newStatusCandidate = prevState;
    SUPLA_LOG_DEBUG(
        "Button[%d]: Initialized: pin %d, pullUp %d, activeHigh %d, state %d",
        buttonNumber,
        inputPin.getPin(),
        inputPin.isPullUp(),
        inputPin.isActiveHigh(),
        prevState);
  }
}

int ButtonState::valueOnPress() const {
  return inputPin.isActiveHigh() ? HIGH : LOW;
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
  return inputPin.getPin();
}

enum Supla::Control::StateResults SimpleButton::getLastState() const {
  return state.getLastState();
}

bool SimpleButton::isReady() const {
  return state.isReady();
}

// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "internal_pin_output.h"

#include <supla/actions.h>
#include <supla/io.h>
#include <supla/time.h>

#include "../events.h"

Supla::Control::InternalPinOutput::InternalPinOutput(Supla::Io::IoPin outPin)
    : outPin(outPin) {
  this->outPin.setMode(OUTPUT);
}

Supla::Control::InternalPinOutput::InternalPinOutput(Supla::Io::IoPin outPin,
                                                     bool highIsOn)
    : outPin(outPin) {
  this->outPin.setMode(OUTPUT);
  this->outPin.setActiveHigh(highIsOn);
}

Supla::Control::InternalPinOutput::InternalPinOutput(Supla::Io::Base *io,
                                                     int pin,
                                                     bool highIsOn)
    : InternalPinOutput(Supla::Io::IoPin(pin, io), highIsOn) {
}

Supla::Control::InternalPinOutput::InternalPinOutput(int pin, bool highIsOn)
    : InternalPinOutput(Supla::Io::IoPin(pin), highIsOn) {
}

Supla::Control::InternalPinOutput &
Supla::Control::InternalPinOutput::setDefaultStateOn() {
  stateOnInit = STATE_ON_INIT_ON;
  return *this;
}
Supla::Control::InternalPinOutput &
Supla::Control::InternalPinOutput::setDefaultStateOff() {
  stateOnInit = STATE_ON_INIT_OFF;
  return *this;
}

uint8_t Supla::Control::InternalPinOutput::pinOnValue() {
  return outPin.isActiveHigh() ? HIGH : LOW;
}

uint8_t Supla::Control::InternalPinOutput::pinOffValue() {
  return outPin.isActiveHigh() ? LOW : HIGH;
}

void Supla::Control::InternalPinOutput::turnOn(_supla_int_t duration) {
  durationMs = duration;
  durationTimestamp = millis();
  if (storedTurnOnDurationMs) {
    durationMs = storedTurnOnDurationMs;
  }

  runAction(Supla::ON_TURN_ON);
  runAction(Supla::ON_CHANGE);

  outPin.writeActive();
}

void Supla::Control::InternalPinOutput::turnOff(_supla_int_t duration) {
  durationMs = duration;
  durationTimestamp = millis();

  runAction(Supla::ON_TURN_OFF);
  runAction(Supla::ON_CHANGE);

  outPin.writeInactive();
}

bool Supla::Control::InternalPinOutput::isOn() {
  return outPin.readActive();
}

void Supla::Control::InternalPinOutput::toggle(_supla_int_t duration) {
  if (isOn()) {
    turnOff(duration);
  } else {
    turnOn(duration);
  }
}

void Supla::Control::InternalPinOutput::handleAction(int event, int action) {
  (void)(event);
  switch (action) {
    case TURN_ON: {
      turnOn();
      break;
    }
    case TURN_OFF: {
      turnOff();
      break;
    }
    case TOGGLE: {
      toggle();
      break;
    }
  }
}

void Supla::Control::InternalPinOutput::onInit() {
  if (stateOnInit == STATE_ON_INIT_ON) {
    turnOn();
  } else {
    turnOff();
  }

  // pin mode is set after setting pin value in order to avoid problems with LOW
  // trigger relays
  outPin.pinMode();
  if (stateOnInit == STATE_ON_INIT_ON) {
    turnOn();
  } else {
    turnOff();
  }
}
void Supla::Control::InternalPinOutput::iterateAlways() {
  if (durationMs && millis() - durationTimestamp > durationMs) {
    toggle();
  }
}

Supla::Control::InternalPinOutput &
Supla::Control::InternalPinOutput::setDurationMs(_supla_int_t duration) {
  storedTurnOnDurationMs = duration;
  return *this;
}

int Supla::Control::InternalPinOutput::getOutputValue() const {
  return lastOutputValue;
}

void Supla::Control::InternalPinOutput::setOutputValue(int value) {
  if (value != lastOutputValue) {
    lastOutputValue = value;
    if (value != 0) {
      turnOn();
    } else {
      turnOff();
    }
  }
}

bool Supla::Control::InternalPinOutput::isOnOffOnly() const {
  return true;
}

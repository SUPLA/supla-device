// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "binary.h"

#include <supla/time.h>

Supla::Sensor::Binary::Binary(Supla::Io::IoPin inputPin)
    : inputPin(inputPin) {
  this->inputPin.setMode(INPUT);
}

Supla::Sensor::Binary::Binary(Supla::Io::Base *io,
                              int pin,
                              bool pullUp,
                              bool invertLogic)
    : Binary(Supla::Io::IoPin(pin, io)) {
  inputPin.setPullUp(pullUp);
  inputPin.setActiveHigh(!invertLogic);
}

Supla::Sensor::Binary::Binary(int pin, bool pullUp, bool invertLogic)
    : Binary(Supla::Io::IoPin(pin)) {
  inputPin.setPullUp(pullUp);
  inputPin.setActiveHigh(!invertLogic);
}

bool Supla::Sensor::Binary::getValue() {
  auto value = inputPin.readActive(channel.getChannelNumber());

  if (config.filteringTimeMs > 0) {
    if (value != newStateCandidateValue) {
      newStateCandidateValue = value;
      lastStateChangeMs = millis();
      notifyInputStateChangeCandidate();
      return prevValue;
    } else if (millis() - lastStateChangeMs > config.filteringTimeMs) {
      value = newStateCandidateValue;
      prevValue = value;
      return value;
    } else {
      return prevValue;
    }
  }

  return value;
}

void Supla::Sensor::Binary::onInit() {
  inputPin.pinMode(channel.getChannelNumber());
  beginInitialChannelValueRead();
  setInitialChannelValue(getValue());
}

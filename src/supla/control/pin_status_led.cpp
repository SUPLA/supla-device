// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "pin_status_led.h"

#include <supla/io.h>

Supla::Control::PinStatusLed::PinStatusLed(Supla::Io::IoPin srcPin,
                                           Supla::Io::IoPin outPin)
    : srcPin(srcPin), outPin(outPin) {
  this->outPin.setMode(OUTPUT);
}

Supla::Control::PinStatusLed::PinStatusLed(Supla::Io::IoPin srcPin,
                                           Supla::Io::IoPin outPin,
                                           bool invert)
    : srcPin(srcPin), outPin(outPin) {
  this->outPin.setMode(OUTPUT);
  this->srcPin.setActiveHigh(!invert);
}

Supla::Control::PinStatusLed::PinStatusLed(Supla::Io::Base *ioSrc,
                                           Supla::Io::Base *ioOut,
                                           uint8_t srcPin,
                                           uint8_t outPin,
                                           bool invert)
    : PinStatusLed(Supla::Io::IoPin(srcPin, ioSrc),
                   Supla::Io::IoPin(outPin, ioOut),
                   invert) {
}

Supla::Control::PinStatusLed::PinStatusLed(uint8_t srcPin,
                                           uint8_t outPin,
                                           bool invert)
    : PinStatusLed(Supla::Io::IoPin(srcPin), Supla::Io::IoPin(outPin), invert) {
}

void Supla::Control::PinStatusLed::onInit() {
  updatePin();
  outPin.pinMode();
}

void Supla::Control::PinStatusLed::iterateAlways() {
  if (!workOnTimer) {
    updatePin();
  }
}

void Supla::Control::PinStatusLed::onTimer() {
  if (workOnTimer) {
    updatePin();
  }
}

void Supla::Control::PinStatusLed::setInvertedLogic(bool invertedLogic) {
  srcPin.setActiveHigh(!invertedLogic);
  updatePin();
}

void Supla::Control::PinStatusLed::updatePin() {
  bool value = srcPin.readActive();
  if (value != outPin.readActive()) {
    if (value) {
      outPin.writeActive();
    } else {
      outPin.writeInactive();
    }
  }
}

void Supla::Control::PinStatusLed::setWorkOnTimer(bool workOnTimer) {
  this->workOnTimer = workOnTimer;
}

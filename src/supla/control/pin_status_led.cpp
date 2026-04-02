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

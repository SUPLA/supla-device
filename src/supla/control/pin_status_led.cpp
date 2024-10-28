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

Supla::Control::PinStatusLed::PinStatusLed(Supla::Io *ioSrc,
                                           Supla::Io *ioOut,
                                           uint8_t srcPin,
                                           uint8_t outPin,
                                           bool invert)
    : PinStatusLed(srcPin, outPin, invert) {
  this->ioSrc = ioSrc;
  this->ioOut = ioOut;
}

Supla::Control::PinStatusLed::PinStatusLed(uint8_t srcPin,
                                           uint8_t outPin,
                                           bool invert)
    : srcPin(srcPin), outPin(outPin), invert(invert) {
}

void Supla::Control::PinStatusLed::onInit() {
  updatePin();
  Supla::Io::pinMode(outPin, OUTPUT, ioOut);
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
  invert = invertedLogic;
  updatePin();
}

void Supla::Control::PinStatusLed::updatePin() {
  int value = Supla::Io::digitalRead(srcPin, ioSrc);
  value = invert ? !value : value;
  if (value != Supla::Io::digitalRead(outPin, ioOut)) {
    Supla::Io::digitalWrite(outPin, value, ioOut);
  }
}

void Supla::Control::PinStatusLed::setWorkOnTimer(bool workOnTimer) {
  this->workOnTimer = workOnTimer;
}

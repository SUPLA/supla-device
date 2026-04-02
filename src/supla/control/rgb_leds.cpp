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

#include "rgb_leds.h"

namespace {
constexpr uint8_t LegacyAnalogWriteResolutionBits = 10;
constexpr uint32_t LegacyAnalogWriteFrequencyHz = 1000;

void ConfigureLegacyAnalogOutput(Supla::Io::IoPin &pin) {
  pin.setAnalogOutputResolutionBits(LegacyAnalogWriteResolutionBits);
  pin.setAnalogOutputFrequency(LegacyAnalogWriteFrequencyHz);
}
}  // namespace

Supla::Control::RGBLeds::RGBLeds(Supla::Io::Base *io,
                                 int redPin,
                                 int greenPin,
                                 int bluePin)
    : RGBLeds(Supla::Io::IoPin(redPin, io),
              Supla::Io::IoPin(greenPin, io),
              Supla::Io::IoPin(bluePin, io)) {}

Supla::Control::RGBLeds::RGBLeds(int redPin, int greenPin, int bluePin)
    : RGBLeds(Supla::Io::IoPin(redPin),
              Supla::Io::IoPin(greenPin),
              Supla::Io::IoPin(bluePin)) {}

Supla::Control::RGBLeds::RGBLeds(Supla::Io::IoPin redPin,
                                 Supla::Io::IoPin greenPin,
                                 Supla::Io::IoPin bluePin)
    : redPin(redPin), greenPin(greenPin), bluePin(bluePin) {
  this->redPin.setMode(OUTPUT);
  this->greenPin.setMode(OUTPUT);
  this->bluePin.setMode(OUTPUT);
}

void Supla::Control::RGBLeds::setRGBWValueOnDevice(uint32_t red,
                                                   uint32_t green,
                                                   uint32_t blue,
                                                   uint32_t brightness) {
  (void)(brightness);
  redPin.analogWrite(red);
  greenPin.analogWrite(green);
  bluePin.analogWrite(blue);
}

void Supla::Control::RGBLeds::onInit() {
  ConfigureLegacyAnalogOutput(redPin);
  ConfigureLegacyAnalogOutput(greenPin);
  ConfigureLegacyAnalogOutput(bluePin);
  redPin.configureAnalogOutput();
  greenPin.configureAnalogOutput();
  bluePin.configureAnalogOutput();
  redPin.pinMode();
  greenPin.pinMode();
  bluePin.pinMode();

  Supla::Control::RGBBase::onInit();
}

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

#include "rgbw_leds.h"

namespace {
constexpr uint8_t LegacyAnalogWriteResolutionBits = 10;
constexpr uint32_t LegacyAnalogWriteFrequencyHz = 1000;

Supla::Io::IoPin MakeOutputPin(Supla::Io::Base *io, int pin) {
  Supla::Io::IoPin result(pin, io);
  result.setMode(OUTPUT);
  return result;
}

void ConfigureLegacyAnalogOutput(Supla::Io::IoPin &pin) {
  pin.setAnalogOutputResolutionBits(LegacyAnalogWriteResolutionBits);
  pin.setAnalogOutputFrequency(LegacyAnalogWriteFrequencyHz);
}
}  // namespace

Supla::Control::RGBWLeds::RGBWLeds(Supla::Io::Base *io,
                                   int redPin,
                                   int greenPin,
                                   int bluePin,
                                   int brightnessPin)
    : RGBWLeds(MakeOutputPin(io, redPin),
               MakeOutputPin(io, greenPin),
               MakeOutputPin(io, bluePin),
               MakeOutputPin(io, brightnessPin)) {}

Supla::Control::RGBWLeds::RGBWLeds(int redPin,
                                   int greenPin,
                                   int bluePin,
                                   int brightnessPin)
    : RGBWLeds(MakeOutputPin(nullptr, redPin),
               MakeOutputPin(nullptr, greenPin),
               MakeOutputPin(nullptr, bluePin),
               MakeOutputPin(nullptr, brightnessPin)) {}

Supla::Control::RGBWLeds::RGBWLeds(Supla::Io::IoPin redPin,
                                   Supla::Io::IoPin greenPin,
                                   Supla::Io::IoPin bluePin,
                                   Supla::Io::IoPin brightnessPin)
    : redPin(redPin),
      greenPin(greenPin),
      bluePin(bluePin),
      brightnessPin(brightnessPin) {
  this->redPin.setMode(OUTPUT);
  this->greenPin.setMode(OUTPUT);
  this->bluePin.setMode(OUTPUT);
  this->brightnessPin.setMode(OUTPUT);
}

void Supla::Control::RGBWLeds::setRGBWValueOnDevice(uint32_t red,
                          uint32_t green,
                          uint32_t blue,
                          uint32_t brightness) {
  redPin.analogWrite(red);
  greenPin.analogWrite(green);
  bluePin.analogWrite(blue);
  brightnessPin.analogWrite(brightness);
}

void Supla::Control::RGBWLeds::onInit() {
  ConfigureLegacyAnalogOutput(redPin);
  ConfigureLegacyAnalogOutput(greenPin);
  ConfigureLegacyAnalogOutput(bluePin);
  ConfigureLegacyAnalogOutput(brightnessPin);
  redPin.configureAnalogOutput();
  greenPin.configureAnalogOutput();
  bluePin.configureAnalogOutput();
  brightnessPin.configureAnalogOutput();
  redPin.pinMode();
  greenPin.pinMode();
  bluePin.pinMode();
  brightnessPin.pinMode();

  Supla::Control::RGBWBase::onInit();
}

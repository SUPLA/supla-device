// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "rgb_leds.h"

namespace {
constexpr uint8_t LegacyAnalogWriteResolutionBits = 10;
constexpr uint32_t LegacyAnalogWriteFrequencyHz = 1000;

void ConfigureLegacyAnalogOutput(Supla::Control::RGBBase &lighting,
                                 Supla::Io::IoPin &pin) {
  lighting.setPwmResolutionBits(LegacyAnalogWriteResolutionBits);
  lighting.setPwmFrequency(LegacyAnalogWriteFrequencyHz);
  pin.setPwmResolutionBits(lighting.getPwmResolutionBits());
  pin.setPwmFrequency(lighting.getPwmFrequency());
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
  redPin.analogWrite(scalePwmValueForOutput(redPin, red));
  greenPin.analogWrite(scalePwmValueForOutput(greenPin, green));
  bluePin.analogWrite(scalePwmValueForOutput(bluePin, blue));
}

void Supla::Control::RGBLeds::onInit() {
  ConfigureLegacyAnalogOutput(*this, redPin);
  ConfigureLegacyAnalogOutput(*this, greenPin);
  ConfigureLegacyAnalogOutput(*this, bluePin);
  redPin.configureAnalogOutput();
  greenPin.configureAnalogOutput();
  bluePin.configureAnalogOutput();
  redPin.pinMode();
  greenPin.pinMode();
  bluePin.pinMode();

  Supla::Control::RGBBase::onInit();
}

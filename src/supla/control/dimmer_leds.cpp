// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dimmer_leds.h"

namespace {
constexpr uint8_t LegacyAnalogWriteResolutionBits = 10;
constexpr uint32_t LegacyAnalogWriteFrequencyHz = 1000;

void ConfigureLegacyAnalogOutput(Supla::Control::DimmerBase &lighting,
                                 Supla::Io::IoPin &pin) {
  lighting.setPwmResolutionBits(LegacyAnalogWriteResolutionBits);
  lighting.setPwmFrequency(LegacyAnalogWriteFrequencyHz);
  pin.setPwmResolutionBits(lighting.getPwmResolutionBits());
  pin.setPwmFrequency(lighting.getPwmFrequency());
}
}  // namespace

Supla::Control::DimmerLeds::DimmerLeds(Supla::Io::Base *io, int brightnessPin)
    : DimmerLeds(Supla::Io::IoPin(brightnessPin, io)) {}

Supla::Control::DimmerLeds::DimmerLeds(int brightnessPin)
    : DimmerLeds(Supla::Io::IoPin(brightnessPin)) {}

Supla::Control::DimmerLeds::DimmerLeds(Supla::Io::IoPin brightnessPin)
    : brightnessPin(brightnessPin) {
  this->brightnessPin.setMode(OUTPUT);
}

void Supla::Control::DimmerLeds::setRGBWValueOnDevice(uint32_t red,
                                                      uint32_t green,
                                                      uint32_t blue,
                                                      uint32_t brightness) {
  (void)(red);
  (void)(green);
  (void)(blue);
  brightnessPin.analogWrite(
      scalePwmValueForOutput(brightnessPin, brightness));
}

void Supla::Control::DimmerLeds::onInit() {
  ConfigureLegacyAnalogOutput(*this, brightnessPin);
  brightnessPin.configureAnalogOutput();
  brightnessPin.pinMode();

  Supla::Control::DimmerBase::onInit();
}

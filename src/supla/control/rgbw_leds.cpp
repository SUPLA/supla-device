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
#include <supla/io.h>
#include <supla/log_wrapper.h>

#ifdef ARDUINO_ARCH_ESP32
extern int esp32PwmChannelCounter;
#endif

Supla::Control::RGBWLeds::RGBWLeds(int redPin,
                                   int greenPin,
                                   int bluePin,
                                   int brightnessPin)
    : redPin(redPin),
      greenPin(greenPin),
      bluePin(bluePin),
      brightnessPin(brightnessPin) {
}

void Supla::Control::RGBWLeds::setRGBWValueOnDevice(uint32_t red,
                          uint32_t green,
                          uint32_t blue,
                          uint32_t colorBrightness,
                          uint32_t brightness) {
  uint32_t redAdj =   red   * colorBrightness / 1023;
  uint32_t greenAdj = green * colorBrightness / 1023;
  uint32_t blueAdj =  blue  * colorBrightness / 1023;
  uint32_t brightnessAdj = brightness;

#ifdef ARDUINO_ARCH_AVR
  redAdj = map(redAdj, 0, 1023, 0, 255);
  greenAdj = map(greenAdj, 0, 1023, 0, 255);
  blueAdj = map(blueAdj, 0, 1023, 0, 255);
  brightnessAdj = map(brightnessAdj, 0, 1023, 0, 255);
#endif

#ifdef ARDUINO_ARCH_ESP32
  ledcWrite(redPin, redAdj);
  ledcWrite(greenPin, greenAdj);
  ledcWrite(bluePin, blueAdj);
  ledcWrite(brightnessPin, brightnessAdj);
#else
  Supla::Io::analogWrite(redPin, redAdj);
  Supla::Io::analogWrite(greenPin, greenAdj);
  Supla::Io::analogWrite(bluePin, blueAdj);
  Supla::Io::analogWrite(brightnessPin, brightnessAdj);
#endif
}

void Supla::Control::RGBWLeds::onInit() {
#ifdef ARDUINO_ARCH_ESP32
  SUPLA_LOG_DEBUG("RGBW: attaching pin %d to PWM channel %d",
                  redPin, esp32PwmChannelCounter);

  ledcSetup(esp32PwmChannelCounter, 1000, 10);
  ledcAttachPin(redPin, esp32PwmChannelCounter);
  // on ESP32 we write to PWM channels instead of pins, so we copy channel
  // number as pin in order to reuse variable
  redPin = esp32PwmChannelCounter;
  esp32PwmChannelCounter++;

  ledcSetup(esp32PwmChannelCounter, 1000, 10);
  ledcAttachPin(greenPin, esp32PwmChannelCounter);
  greenPin = esp32PwmChannelCounter;
  esp32PwmChannelCounter++;

  ledcSetup(esp32PwmChannelCounter, 1000, 10);
  ledcAttachPin(bluePin, esp32PwmChannelCounter);
  bluePin = esp32PwmChannelCounter;
  esp32PwmChannelCounter++;

  ledcSetup(esp32PwmChannelCounter, 1000, 10);
  ledcAttachPin(brightnessPin, esp32PwmChannelCounter);
  brightnessPin = esp32PwmChannelCounter;
  esp32PwmChannelCounter++;

#else
  Supla::Io::pinMode(redPin, OUTPUT);
  Supla::Io::pinMode(greenPin, OUTPUT);
  Supla::Io::pinMode(bluePin, OUTPUT);
  Supla::Io::pinMode(brightnessPin, OUTPUT);

#ifdef ARDUINO_ARCH_ESP8266
  analogWriteRange(1024);
#endif
#endif

  Supla::Control::RGBWBase::onInit();
}

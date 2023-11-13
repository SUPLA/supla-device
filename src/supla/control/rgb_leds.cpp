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
#include <supla/io.h>
#include <supla/log_wrapper.h>

#ifdef ARDUINO_ARCH_ESP32
extern int esp32PwmChannelCounter;
#endif

Supla::Control::RGBLeds::RGBLeds(Supla::Io *io,
                                 int redPin,
                                 int greenPin,
                                 int bluePin)
    : RGBLeds(redPin, greenPin, bluePin) {
  this->io = io;
}

Supla::Control::RGBLeds::RGBLeds(int redPin, int greenPin, int bluePin)
    : redPin(redPin), greenPin(greenPin), bluePin(bluePin) {
}

void Supla::Control::RGBLeds::setRGBWValueOnDevice(uint32_t red,
                                                   uint32_t green,
                                                   uint32_t blue,
                                                   uint32_t colorBrightness,
                                                   uint32_t brightness) {
  (void)(brightness);
  uint32_t redAdj =   red   * colorBrightness / 1023;
  uint32_t greenAdj = green * colorBrightness / 1023;
  uint32_t blueAdj =  blue  * colorBrightness / 1023;

#ifdef ARDUINO_ARCH_AVR
  redAdj = map(redAdj, 0, 1023, 0, 255);
  greenAdj = map(greenAdj, 0, 1023, 0, 255);
  blueAdj = map(blueAdj, 0, 1023, 0, 255);
#endif

#ifdef ARDUINO_ARCH_ESP32
  if (io) {
    Supla::Io::analogWrite(redPin, redAdj, io);
    Supla::Io::analogWrite(greenPin, greenAdj, io);
    Supla::Io::analogWrite(bluePin, blueAdj, io);
  } else {
    // TODO(klew): move to IO for ESP32
    ledcWrite(redPin, redAdj);
    ledcWrite(greenPin, greenAdj);
    ledcWrite(bluePin, blueAdj);
  }
#else
  Supla::Io::analogWrite(redPin, redAdj, io);
  Supla::Io::analogWrite(greenPin, greenAdj, io);
  Supla::Io::analogWrite(bluePin, blueAdj, io);
#endif
}

void Supla::Control::RGBLeds::onInit() {
#ifdef ARDUINO_ARCH_ESP32
  if (io) {
    Supla::Io::pinMode(redPin, OUTPUT, io);
    Supla::Io::pinMode(greenPin, OUTPUT, io);
    Supla::Io::pinMode(bluePin, OUTPUT, io);
  } else {
    // TODO(klew): move to IO for ESP32
    SUPLA_LOG_DEBUG("RGB: attaching pin %d to PWM channel %d",
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
  }
#else
  Supla::Io::pinMode(redPin, OUTPUT, io);
  Supla::Io::pinMode(greenPin, OUTPUT, io);
  Supla::Io::pinMode(bluePin, OUTPUT, io);

#ifdef ARDUINO_ARCH_ESP8266
  analogWriteRange(1024);
#endif
#endif

  Supla::Control::RGBBase::onInit();
}

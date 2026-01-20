/*
   Copyright (C) malarz

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

// Dependencies:
// https://github.com/adafruit/Adafruit_NeoPixel

#ifndef SRC_SUPLA_CONTROL_ADDRESSABLELEDS_H_
#define SRC_SUPLA_CONTROL_ADDRESSABLELEDS_H_

#include <Adafruit_NeoPixel.h>
#include <supla/log_wrapper.h>
#include <supla/control/virtual_relay.h>
#include <supla/control/rgb_base.h>

namespace Supla {
namespace Control {

enum AddressableLEDsEffect : uint8_t {
  STILL,
  SWAP,
  FLOW,
  RAINBOWWHEEL,
  RAINBOW,
};

/**
 * Class for using addressable LEDs as ws2812b and many others
 *
 * It implements some simple effects for using with strips and rings
 * constructed with such leds
 */

class AddressableLEDs : public Supla::Element {
 public:
  /**
   * Constructor
   *
   * @param number - number of leds in strip/ring
   * @param pin - GPIO with connected data line of leds
   * @param type (optional) - type of sytip/ring,
   *                          see Adafruit Neopixel library
   *
   */
  AddressableLEDs(uint16_t number, int16_t pin,
         neoPixelType type = NEO_GRB + NEO_KHZ800) {
    numberOfLeds = number;
    pixels = new Adafruit_NeoPixel(numberOfLeds, pin, type);
  }

  /**
   * Setting effect on strip/ring
   *
   * @param neweffect - AddressableLEDsEffect, can be:
   * - AddressableLEDsEffect::STILL
   * - AddressableLEDsEffect::SWAP
   * - AddressableLEDsEffect::FLOW
   * - AddressableLEDsEffect::RAINBOWWHEEL
   * - AddressableLEDsEffect::RAINBOW
   * @param newStepTime - time (in ms) of one effect step
   * @param turnAllLEDsTime (optional) - time (in sec) of turning on all leds 
   *
   */
  void setEffect(AddressableLEDsEffect neweffect, uint16_t newStepTime,
      uint8_t turnAllLEDsTime = 0) {
    if (effect != neweffect) {
      effect = neweffect;
      counter = 0;
    }

    if (turnOnTime != turnAllLEDsTime) {
      // switching OFF all LEDs
      for (int i = 0; i < numberOfLeds; i++) {
        pixels->setPixelColor(i, 0);
      }
      pixels->show();
      lightedLeds = 0;
    }

    stepTime = newStepTime;
    turnOnTime = turnAllLEDsTime;
  }

  void onInit() override {
     pixels->begin();
  }

  AddressableLEDsEffect getEffect() {
    return effect;
  }
  uint16_t getStepTime() {
    return stepTime;
  }
  uint16_t getTurnOnTime() {
    return turnOnTime;
  }

  bool isOn() {
    return on;
  }
  void turnOn() {
    on = true;
  }
  void turnOff() {
    on = false;
  }

  void setColor(uint32_t color) {
    RGBcolor = color;
  }
  void setColor(uint8_t red, uint8_t green, uint8_t blue) {
    RGBcolor = pixels->Color(red, green, blue);
  }
  void setBrightness(uint8_t brightness) {
    pixels->setBrightness(brightness);
  }

  void iterateAlways() {
    // LEDs switching ON
    if (isOn() && lightedLeds < numberOfLeds
        && ((millis() - lastLEDTime >= (1000 * turnOnTime / numberOfLeds))
        || (millis() < lastLEDTime))) {
      lastLEDTime = millis();
      SUPLA_LOG_DEBUG("RGB strip: switching on LED %d", lightedLeds);
      lightedLeds++;
    }

    if (lightedLeds > 0 && ((millis()-lastTime >= stepTime)
        || (millis() < lastTime))) {
      lastTime = millis();
      counter++;
      // show effect
      switch (effect) {
        case STILL:
          iterate_Still();
          break;
        case SWAP:
          iterate_Swap();
          break;
        case FLOW:
          iterate_Flow();
          break;
        case RAINBOWWHEEL:
          iterate_RainbowWheel();
          break;
        case RAINBOW:
          iterate_Rainbow();
          break;
      }
    }

    // LEDs switching OFF
    if (!isOn() && lightedLeds > 0
        && ((millis() - lastLEDTime >= (1000 * turnOnTime / numberOfLeds))
        || (millis() < lastLEDTime))) {
      lastLEDTime = millis();
      lightedLeds--;
      SUPLA_LOG_DEBUG("RGB strip: switching off LED %d", lightedLeds);
      pixels->setPixelColor(lightedLeds, 0);
      pixels->show();
    }
  }

 protected:
  Adafruit_NeoPixel *pixels;
  bool on = false;
  uint16_t numberOfLeds;
  uint16_t lightedLeds = 0;
  uint32_t RGBcolor = 0x004400;  // color of SUPLA :-)
  uint32_t LastColor = 0;

  uint8_t turnOnTime = 0;
  uint32_t lastLEDTime = 0;

  uint16_t stepTime = 1000;
  uint32_t lastTime = 0;

  AddressableLEDsEffect effect = STILL;
  uint32_t counter = 0;

  void iterate_Still() {
    for (int i = 0; i < lightedLeds; i++) {
      pixels->setPixelColor(i, RGBcolor);
    }
    pixels->show();
  }

  void iterate_Swap() {
    for (int i = 0; i < lightedLeds; i++) {
      if ((i+counter)%2) {
        pixels->setPixelColor(i, RGBcolor);
      } else {
        pixels->setPixelColor(i, 0);
      }
    }
    pixels->show();
  }

  void iterate_Flow() {
    for (int i = 0; i < numberOfLeds; i++) {
      if ((i+counter)%4) {
        pixels->setPixelColor(i, RGBcolor);
      } else {
        pixels->setPixelColor(i, 0);
      }
    }
    pixels->show();
  }

  void iterate_RainbowWheel() {
    for (int i = 0; i < lightedLeds; i++) {
      if (counter > 255) {
        counter = 0;
      }
      pixels->setPixelColor(i, RainbowWheel((i*1+counter) & 255));
    }
    pixels->show();
  }

  void iterate_Rainbow() {
    for (int i = 0; i < lightedLeds; i++) {
      if (counter*256 > 5*65536) {
        counter = 0;
      }
      int pixelHue = 256*counter + (i * 65536L / numberOfLeds);
      pixels->setPixelColor(i, pixels->gamma32(pixels->ColorHSV(pixelHue)));
    }
    pixels->show();
  }

  uint32_t RainbowWheel(byte WheelPos) {
    if (WheelPos < 85) {
      return pixels->Color(WheelPos * 3, 255 - WheelPos * 3, 0);
    } else if (WheelPos < 170) {
      WheelPos -= 85;
      return pixels->Color(255 - WheelPos * 3, 0, WheelPos * 3);
    } else {
      WheelPos -= 170;
      return pixels->Color(0, WheelPos * 3, 255 - WheelPos * 3);
    }
  }
};

}  // namespace Control
}  // namespace Supla

#endif  // SRC_SUPLA_CONTROL_ADDRESSABLELEDS_H_


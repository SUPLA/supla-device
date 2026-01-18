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
  VEEROOS,
  SWAP,
  FLOW,
  RAINBOWWHEEL,
  RAINBOW,
  RAINBOWVEEROOS,
};

class AddressableLEDs : public Supla::Element {
 public:
  AddressableLEDs(uint16_t number, int16_t pin,
         neoPixelType type = NEO_GRB + NEO_KHZ800) {
    numberOfLeds = number;
    pixels = new Adafruit_NeoPixel(numberOfLeds, pin, type);

    pixels->begin();
  }

  void setEffect(AddressableLEDsEffect _effect, int time) {
    if (effect != _effect) {
      effect = _effect;
      counter = 0;

      // switching OFF all LEDs
      for (int i=0; i < numberOfLeds; i++) {
        pixels->setPixelColor(i, 0);
      }
      pixels->show();
      lightedLeds = 0;
    }
    if (OneLedTime != time) {
       OneLedTime = time;
    }
  }

  AddressableLEDsEffect getEffect() {
    return effect;
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
    switch (effect) {
      case VEEROOS:
        iterateAlways_Veeroos();
        break;
      case SWAP:
        iterateAlways_Swap();
        break;
      case FLOW:
        iterateAlways_Flow();
        break;
      case RAINBOWWHEEL:
        iterateAlways_RainbowWheel();
        break;
      case RAINBOW:
        lightedLeds = numberOfLeds;
        iterateAlways_Rainbow();
        break;
      case RAINBOWVEEROOS:
        iterateAlways_RainbowVeeroos();
        break;
    }
  }

 protected:
  Adafruit_NeoPixel *pixels;
  bool on = false;
  uint16_t numberOfLeds;
  uint16_t lightedLeds = 0;
  uint32_t RGBcolor = 0x004400;  // color of SUPLA :-)
  uint32_t LastColor = 0;

  int OneLedTime = 200;
  uint32_t lastTime = 0;

  AddressableLEDsEffect effect = VEEROOS;
  uint32_t counter = 0;

  void iterateAlways_Veeroos() {
    // LEDs switching ON
    if (isOn() && lightedLeds < numberOfLeds
        && (millis()-lastTime >= OneLedTime) || (millis() < lastTime)) {
      lastTime = millis();
      SUPLA_LOG_DEBUG("RGB strip: switching on LED %d with color 0x%06x",
        lightedLeds, RGBcolor);
      pixels->setPixelColor(lightedLeds, RGBcolor);
      pixels->show();
      lightedLeds++;
    }
    // LEDs changing colour
    if (LastColor != RGBcolor) {
      LastColor = RGBcolor;
      for (int i=0; i < lightedLeds; i++) {
        pixels->setPixelColor(i, RGBcolor);
      }
      pixels->show();
    }
    // LEDs switching OFF
    if (!isOn() && lightedLeds > 0 && (millis()-lastTime >= OneLedTime)
        || (millis() < lastTime)) {
      lastTime = millis();
      lightedLeds--;
      SUPLA_LOG_DEBUG("RGB strip: switching off LED %d with color 0x%06x",
        lightedLeds, RGBcolor);
      pixels->setPixelColor(lightedLeds, 0);
      pixels->show();
    }
  }

  void iterateAlways_Swap() {
    if (isOn() && ((millis()-lastTime >= OneLedTime)
        || (millis() < lastTime))) {
      lastTime = millis();
      counter++;
      for (int i=0; i < numberOfLeds; i++) {
        if ((i+counter)%2) {
          pixels->setPixelColor(i, RGBcolor);
        } else {
          pixels->setPixelColor(i, 0);
        }
      }
      pixels->show();
    }
    if (!isOn()) {
      for (int i=0; i < numberOfLeds; i++) {
        pixels->setPixelColor(i, 0);
      }
      pixels->show();
    }
  }

  void iterateAlways_Flow() {
    if (isOn() && ((millis()-lastTime >= OneLedTime)
        || (millis() < lastTime))) {
      lastTime = millis();
      counter++;
      for (int i=0; i < numberOfLeds; i++) {
        if ((i+counter)%4) {
          pixels->setPixelColor(i, RGBcolor);
        } else {
          pixels->setPixelColor(i, 0);
        }
      }
      pixels->show();
    }
    if (!isOn()) {
      for (int i=0; i < numberOfLeds; i++) {
        pixels->setPixelColor(i, 0);
      }
      pixels->show();
    }
  }

  void iterateAlways_RainbowWheel() {
    if (isOn() && ((millis()-lastTime >= OneLedTime)
        || (millis() < lastTime))) {
      lastTime = millis();
      counter++;
      for (int i=0; i < numberOfLeds; i++) {
        if (counter > 255) {
          counter = 0;
        }
        pixels->setPixelColor(i, RainbowWheel((i*1+counter) & 255));
      }
      pixels->show();
    }
    if (!isOn()) {
      for (int i=0; i < numberOfLeds; i++) {
        pixels->setPixelColor(i, 0);
      }
      pixels->show();
    }
  }

  void iterateAlways_Rainbow() {
    if (isOn() && ((millis()-lastTime >= OneLedTime)
        || (millis() < lastTime))) {
      lastTime = millis();
      counter++;
      for (int i=0; i < lightedLeds; i++) {
        if (counter*256 > 5*65536) {
          counter = 0;
        }
        int pixelHue = 256*counter + (i * 65536L / numberOfLeds);
        pixels->setPixelColor(i, pixels->gamma32(pixels->ColorHSV(pixelHue)));
      }
      pixels->show();
    }
    if (!isOn()) {
      for (int i=0; i < numberOfLeds; i++) {
        pixels->setPixelColor(i, 0);
      }
      pixels->show();
    }
  }

  void iterateAlways_RainbowVeeroos() {
    if ((millis()-lastTime >= OneLedTime) || (millis() < lastTime)) {
      lastTime = millis();
      counter++;

      // LEDs switching ON
      if (isOn() && lightedLeds < numberOfLeds && counter%20 == 0) {
        lightedLeds++;
      }
      // LEDs colour changing
      for (int i=0; i < lightedLeds; i++) {
        if (counter*256 > 5*65536) {
          counter = 0;
        }
        int pixelHue = 256 * counter + (i * 65536L / numberOfLeds);
        pixels->setPixelColor(i, pixels->gamma32(pixels->ColorHSV(pixelHue)));
      }
      pixels->show();
      // LEDs switching OFF
      if (!isOn() && lightedLeds > 0 && counter%20 == 0) {
        lightedLeds--;
        pixels->setPixelColor(lightedLeds, 0);
        pixels->show();
      }
    }
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

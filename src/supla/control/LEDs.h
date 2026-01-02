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

#ifndef SRC_SUPLA_CONTROL_LEDS_H_
#define SRC_SUPLA_CONTROL_LEDS_H_

#include <Adafruit_NeoPixel.h>
#include <supla/log_wrapper.h>
#include <supla/control/virtual_relay.h>
#include <supla/control/rgb_base.h>

namespace Supla {
namespace Control {
namespace LEDs {
  
enum StripEffect : uint8_t {
  VEEROOS,
  SWAP,
  FLOW,
  RAINBOWWHEEL,
  RAINBOW,
  RAINBOWVEEROOS,
};

class Strip : public Supla::Element {
  public:
    Strip(uint16_t number, int16_t pin, neoPixelType type = NEO_GRB + NEO_KHZ800);

    void setEffect(StripEffect _effect, int time);

    StripEffect getEffect();

    bool isOn();
    void turnOn();
    void turnOff();

    void setColor(uint32_t color);
    void setColor(uint8_t red, uint8_t green, uint8_t blue);
    void setBrightness(uint8_t brightness);

    void iterateAlways();

  protected:
    Adafruit_NeoPixel *pixels;
    bool on = false;
    uint16_t numberOfLeds;
    uint16_t lightedLeds = 0;
    uint32_t RGBcolor = 0x004400; // color of SUPLA :-)
    uint32_t LastColor = 0;

    int OneLedTime = 200;
    uint32_t lastTime = 0;

    StripEffect effect = VEEROOS;
    long counter = 0;

    void iterateAlways_Veeroos();
    void iterateAlways_Swap();
    void iterateAlways_Flow();
    void iterateAlways_RainbowWheel();
    void iterateAlways_Rainbow();
    void iterateAlways_RainbowVeeroos();
    uint32_t RainbowWheel(byte WheelPos);
};

class effectSwitch : public Supla::Control::VirtualRelay {
  public:
    effectSwitch(LEDs::Strip* leds, StripEffect effect, int time);

    void turnOn(_supla_int_t duration = 0);
    void turnOff(_supla_int_t duration = 0);

    void iterateAlways();

  protected:
    Strip* _leds;
    StripEffect _effect;
    int _time;
};

class colorSelector : public Supla::Control::RGBBase {
  public:
    colorSelector(LEDs::Strip* leds);

    void setRGBWValueOnDevice(uint32_t red,
                              uint32_t green,
                              uint32_t blue,
                              uint32_t colorBrightness,
                              uint32_t brightness);

    void turnOn();
    void turnOff();

  protected:
    Strip* _leds;
};

}  // namespace LEDs
}  // namespace Control
}  // namespace Supla

#endif  // SRC_SUPLA_CONTROL_LEDS_H_
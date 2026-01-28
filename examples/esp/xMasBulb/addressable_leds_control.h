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

#ifndef SRC_ADDRESSABLE_LEDS_CONTROL_H_
#define SRC_ADDRESSABLE_LEDS_CONTROL_H_

#include <supla/control/addressable_leds.h>

class AddressableLEDsEffectSwitch : public Supla::Control::VirtualRelay {
  public:
    AddressableLEDsEffectSwitch(Supla::Control::AddressableLEDs* newLeds, Supla::Control::AddressableLEDsEffect newEffect, uint16_t switchTime, uint8_t turnTime = 0) {
      leds = newLeds;
      effect = newEffect;
      time = switchTime;
      turnOnTume = turnTime;
      getChannel()->setDefault(SUPLA_CHANNELFNC_LIGHTSWITCH);
    }

    void turnOn(_supla_int_t duration = 0) {
      leds->setEffect(effect, time, turnOnTume);
      if (!leds->isOn()) {
        leds->turnOn();
      }
      channel.setNewValue(true);
    }

    void turnOff(_supla_int_t duration = 0) {
      if (leds->getEffect() == effect) {
        leds->turnOff();
        channel.setNewValue(false);
      }
    }

    void iterateAlways() {
      if (leds->getEffect() != effect) {
        channel.setNewValue(false);
      }
      if (leds->getStepTime() != time) {
        channel.setNewValue(false);
      }
      if (leds->getTurnOnTime() != turnOnTume) {
        channel.setNewValue(false);
      }
    }

  protected:
    Supla::Control::AddressableLEDs* leds;
    Supla::Control::AddressableLEDsEffect effect;
    uint16_t time;
    uint8_t turnOnTume;
};

class AddressableLEDsColorSelector : public Supla::Control::RGBBase {
  public:
    AddressableLEDsColorSelector(Supla::Control::AddressableLEDs* newLeds) {
      leds = newLeds;
      setDefaultDimmedBrightness(50);
      setDefaultStateOn();
    }

    void setRGBWValueOnDevice(uint32_t red,
                              uint32_t green,
                              uint32_t blue,
                              uint32_t colorBrightness,
                              uint32_t brightness) {

      leds->setColor(red/4, green/4, blue/4);

      // effects can have diffrent way to switch on/off
      if (leds->isOn()) {
        leds->setBrightness(colorBrightness/4);
      }
    }

    void turnOn() {
      leds->turnOn();
    }

    void turnOff() {
      leds->turnOff();
    }

  protected:
    Supla::Control::AddressableLEDs* leds;
};

#endif  // SRC_ADDRESSABLE_LEDS_CONTROL_H_

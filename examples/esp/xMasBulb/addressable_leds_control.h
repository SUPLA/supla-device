// SPDX-FileCopyrightText: malarz
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXAMPLES_ESP_XMASBULB_ADDRESSABLE_LEDS_CONTROL_H_
#define EXAMPLES_ESP_XMASBULB_ADDRESSABLE_LEDS_CONTROL_H_

#include <supla/control/addressable_leds.h>

class AddressableLEDsEffectSwitch : public Supla::Control::VirtualRelay {
 public:
  AddressableLEDsEffectSwitch(
      Supla::Control::AddressableLEDs* newLeds,
      Supla::Control::AddressableLEDsEffect newEffect, uint16_t switchTime,
      uint8_t turnTime = 0) {
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
  explicit AddressableLEDsColorSelector(
      Supla::Control::AddressableLEDs* newLeds) {
    leds = newLeds;
    setDefaultDimmedBrightness(50);
    setDefaultStateOn();
  }

  void setRGBWValueOnDevice(uint32_t red, uint32_t green, uint32_t blue,
                            uint32_t brightness) {
    leds->setColor(red / 4, green / 4, blue / 4);
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

#endif  // EXAMPLES_ESP_XMASBULB_ADDRESSABLE_LEDS_CONTROL_H_

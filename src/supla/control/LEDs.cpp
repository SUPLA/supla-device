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

#include "LEDs.h"

Supla::Control::LEDs::Strip::Strip(uint16_t number, int16_t pin, neoPixelType type) {
      numberOfLeds = number;
      pixels = new Adafruit_NeoPixel(numberOfLeds, pin, type);

      pixels->begin();
}

void Supla::Control::LEDs::Strip::setEffect(StripEffect _effect, int time) {
  if (effect != _effect) {
    effect = _effect;
    counter = 0;

    // switching OFF all LEDs
    for (int i=0; i<numberOfLeds; i++) {
      pixels->setPixelColor(i, 0);
    }
    pixels->show();
    lightedLeds = 0;
  }
  if (OneLedTime != time) { 
    OneLedTime = time;
  }
}

Supla::Control::LEDs::StripEffect Supla::Control::LEDs::Strip::getEffect() {
  return effect;
}

bool Supla::Control::LEDs::Strip::isOn() {
  return on;
}

void Supla::Control::LEDs::Strip::turnOn() {
  on = true;
}

void Supla::Control::LEDs::Strip::turnOff() {
  on = false;
}

void Supla::Control::LEDs::Strip::setColor(uint32_t color) {
  RGBcolor = color;
}

void Supla::Control::LEDs::Strip::setColor(uint8_t red, uint8_t green, uint8_t blue) {
  RGBcolor = pixels->Color(red, green, blue);
}

void Supla::Control::LEDs::Strip::setBrightness(uint8_t brightness) {
  pixels->setBrightness(brightness);
}

void Supla::Control::LEDs::Strip::iterateAlways() {
  switch(effect) {
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

void Supla::Control::LEDs::Strip::iterateAlways_Veeroos() {
  // LEDs switching ON
  if (isOn() && lightedLeds<numberOfLeds && (millis()-lastTime >= OneLedTime) || (millis() < lastTime)) {
    lastTime = millis();
    SUPLA_LOG_DEBUG("RGB strip: switching on LED %d with color 0x%06x", lightedLeds, RGBcolor);
    pixels->setPixelColor(lightedLeds, RGBcolor);
    pixels->show();
    lightedLeds++;
  }
  // LEDs changing colour
  if (LastColor != RGBcolor) {
    LastColor = RGBcolor;
    for (int i=0; i<lightedLeds; i++) {
      pixels->setPixelColor(i, RGBcolor);
    }
    pixels->show();
  }
  // LEDs switching OFF
  if (!isOn() && lightedLeds>0 && (millis()-lastTime >= OneLedTime) || (millis() < lastTime)) {
    lastTime = millis();
    lightedLeds--;
    SUPLA_LOG_DEBUG("RGB strip: switching off LED %d with color 0x%06x", lightedLeds, RGBcolor);
    pixels->setPixelColor(lightedLeds, 0);
    pixels->show();
  }
}

void Supla::Control::LEDs::Strip::iterateAlways_Swap() {
  if (isOn() && ((millis()-lastTime >= OneLedTime) || (millis() < lastTime))) {
    lastTime = millis();
    counter++;
    for (int i=0; i<numberOfLeds; i++) {
      if ((i+counter)%2) {
        pixels->setPixelColor(i, RGBcolor);
      } else {
        pixels->setPixelColor(i, 0);
      }
    }
    pixels->show();
  }
  if (!isOn()) {
    for (int i=0; i<numberOfLeds; i++) {
      pixels->setPixelColor(i, 0);
    }
    pixels->show();
  }
} 

void Supla::Control::LEDs::Strip::iterateAlways_Flow() {
  if (isOn() && ((millis()-lastTime >= OneLedTime) || (millis() < lastTime))) {
    lastTime = millis();
    counter++;
    for (int i=0; i<numberOfLeds; i++) {
      if ((i+counter)%4) {
        pixels->setPixelColor(i, RGBcolor);
      } else {
        pixels->setPixelColor(i, 0);
      }
    }
    pixels->show();
  }
  if (!isOn()) {
    for (int i=0; i<numberOfLeds; i++) {
      pixels->setPixelColor(i, 0);
    }
    pixels->show();
  }
} 

void Supla::Control::LEDs::Strip::iterateAlways_RainbowWheel() {
  if (isOn() && ((millis()-lastTime >= OneLedTime) || (millis() < lastTime))) {
    lastTime = millis();
    counter++;
    for (int i=0; i<numberOfLeds; i++) {
      if (counter>255) {
        counter = 0;
      }
      pixels->setPixelColor(i, RainbowWheel((i*1+counter) & 255));
    }
    pixels->show();
  }
  if (!isOn()) {
    for (int i=0; i<numberOfLeds; i++) {
      pixels->setPixelColor(i, 0);
    }
    pixels->show();
  }
} 

void Supla::Control::LEDs::Strip::iterateAlways_Rainbow() {
  if (isOn() && ((millis()-lastTime >= OneLedTime) || (millis() < lastTime))) {
    lastTime = millis();
    counter++;
    for (int i=0; i<lightedLeds; i++) {
      if (counter*256 > 5*65536) {
        counter = 0;
      }
      int pixelHue = 256*counter + (i * 65536L / numberOfLeds);
      pixels->setPixelColor(i, pixels->gamma32(pixels->ColorHSV(pixelHue)));
    }
    pixels->show();
  }
  if (!isOn()) {
    for (int i=0; i<numberOfLeds; i++) {
      pixels->setPixelColor(i, 0);
    }
    pixels->show();
  }
} 

void Supla::Control::LEDs::Strip::iterateAlways_RainbowVeeroos() {
  if ((millis()-lastTime >= OneLedTime) || (millis() < lastTime)) {
    lastTime = millis();
    counter++;

    // LEDs switching ON
    if (isOn() && lightedLeds<numberOfLeds && counter%20==0) {
      lightedLeds++;
    }
    // LEDs colour changing
    for (int i=0; i<lightedLeds; i++) {
      if (counter*256 > 5*65536) {
        counter = 0;
      }
      int pixelHue = 256*counter + (i * 65536L / numberOfLeds);
      pixels->setPixelColor(i, pixels->gamma32(pixels->ColorHSV(pixelHue)));
    }
    pixels->show();
    // LEDs switching OFF
    if (!isOn() && lightedLeds>0 && counter%20==0) {
      lightedLeds--;
      pixels->setPixelColor(lightedLeds, 0);
      pixels->show();
    }
  }
}

uint32_t Supla::Control::LEDs::Strip::RainbowWheel(byte WheelPos) {
  if(WheelPos < 85) {
    return pixels->Color(WheelPos * 3, 255 - WheelPos * 3, 0);
  } 
  else if(WheelPos < 170) {
    WheelPos -= 85;
    return pixels->Color(255 - WheelPos * 3, 0, WheelPos * 3);
  } 
  else {
    WheelPos -= 170;
    return pixels->Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
}

Supla::Control::LEDs::effectSwitch::effectSwitch(Supla::Control::LEDs::Strip* leds, Supla::Control::LEDs::StripEffect effect, int time) {
  _leds = leds;
  _effect = effect;
  _time = time;
  getChannel()->setDefault(SUPLA_CHANNELFNC_LIGHTSWITCH);
}

void Supla::Control::LEDs::effectSwitch::turnOn(_supla_int_t duration) {
  _leds->setEffect(_effect, _time);
  if (!_leds->isOn()) {
    _leds->turnOn();
  }
  channel.setNewValue(true);
}

void Supla::Control::LEDs::effectSwitch::turnOff(_supla_int_t duration) {
  if (_leds->getEffect() == _effect) {
    _leds->turnOff();
    channel.setNewValue(false);
  }
}

void Supla::Control::LEDs::effectSwitch::iterateAlways() {
  if (_leds->getEffect() != _effect) {
    channel.setNewValue(false);
  }
}

Supla::Control::LEDs::colorSelector::colorSelector(Supla::Control::LEDs::Strip* leds) {
  _leds = leds;
  setDefaultDimmedBrightness(50);
  setDefaultStateOn();
}

void Supla::Control::LEDs::colorSelector::setRGBWValueOnDevice(uint32_t red,
                          uint32_t green,
                          uint32_t blue,
                          uint32_t colorBrightness,
                          uint32_t brightness) {

  _leds->setColor(red/4, green/4, blue/4);

  // effects can have diffrent way to switch on/off
  if (_leds->isOn()) {
    _leds->setBrightness(colorBrightness/4);
  }
}

void Supla::Control::LEDs::colorSelector::turnOn() {
  _leds->turnOn();
}

void Supla::Control::LEDs::colorSelector::turnOff() {
  _leds->turnOff();
}

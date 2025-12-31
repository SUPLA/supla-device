/*
   Copyright (C) AC SOFTWARE SP. Z O.O

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

#include "rgbcct_parsed.h"

#include <supla/control/rgb_base.h>
#include <supla/log_wrapper.h>
#include <supla/time.h>

#include <cstdio>

Supla::Control::RgbCctParsed::RgbCctParsed(Supla::Parser::Parser *parser)
    : Supla::Sensor::SensorParsed<Supla::Control::RGBCCTBase>(parser) {
}

void Supla::Control::RgbCctParsed::iterateAlways() {
  Supla::Control::RGBCCTBase::iterateAlways();

  if (parser && (millis() - lastReadTime > 100)) {
    refreshParserSource();
    lastReadTime = millis();
    if (isOffline()) {
      channel.setStateOffline();
    } else {
      channel.setStateOnline();
    }
  }
}

bool Supla::Control::RgbCctParsed::isOffline() {
  if (useOfflineOnInvalidState && parser) {
    if (getStateValue() == -1) {
      return true;
    }
  }
  return false;
}

void Supla::Control::RgbCctParsed::setUseOfflineOnInvalidState(
    bool useOfflineOnInvalidState) {
  this->useOfflineOnInvalidState = useOfflineOnInvalidState;
  SUPLA_LOG_INFO("useOfflineOnInvalidState = %d", useOfflineOnInvalidState);
}

void Supla::Control::RgbCctParsed::setRGBCCTValueOnDevice(
    uint32_t red,
    uint32_t green,
    uint32_t blue,
    uint32_t colorBrightness,
    uint32_t white1Brightness,
    uint32_t white2Brightness) {
  (void)red;
  (void)green;
  (void)blue;
  (void)colorBrightness;
  (void)white1Brightness;
  (void)white2Brightness;

  //  SUPLA_LOG_DEBUG("RGBCCT[%d]: R %d G %d B %d C %d W1 %d W2 %d",
  //                  getChannelNumber(),
  //                  red,
  //                  green,
  //                  blue,
  //                  colorBrightness,
  //                  white1Brightness,
  //                  white2Brightness);
}

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
    : Supla::Sensor::SensorParsed<Supla::Control::LightingPwmBase>(parser) {
}

void Supla::Control::RgbCctParsed::iterateAlways() {
  Supla::Control::LightingPwmBase::iterateAlways();

  if (parser && (millis() - lastReadTime > 100)) {
    if (setOfflineIfSourceDisconnected()) {
      lastReadTime = millis();
      return;
    }
    refreshParserSource(false);
    lastReadTime = millis();
    setChannelStateOnline(!isOffline());
  }
}

void Supla::Control::RgbCctParsed::setRGBCCTValueOnDevice(uint32_t output[5],
                                                          int usedOutputs) {
  (void)usedOutputs;
  (void)output;

  //  SUPLA_LOG_DEBUG("RGBCCT[%d]: R %d G %d B %d C %d W1 %d W2 %d",
  //                  getChannelNumber(),
  //                  red,
  //                  green,
  //                  blue,
  //                  colorBrightness,
  //                  white1Brightness,
  //                  white2Brightness);
}

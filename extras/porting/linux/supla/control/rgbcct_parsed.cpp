// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

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

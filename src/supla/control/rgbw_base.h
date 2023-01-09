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

#ifndef SRC_SUPLA_CONTROL_RGBW_BASE_H_
#define SRC_SUPLA_CONTROL_RGBW_BASE_H_

#include <stdint.h>

#include "../action_handler.h"
#include "../actions.h"
#include "../channel_element.h"

namespace Supla {
namespace Control {
class RGBWBase : public ChannelElement, public ActionHandler {
 public:
  RGBWBase();

  virtual void setRGBWValueOnDevice(uint32_t red,
                                    uint32_t green,
                                    uint32_t blue,
                                    uint32_t colorBrightness,
                                    uint32_t brightness) = 0;

  virtual void setRGBW(int red,
                       int green,
                       int blue,
                       int colorBrightness,
                       int brightness,
                       bool toggle = false);

  int handleNewValueFromServer(TSD_SuplaChannelNewValue *newValue);
  virtual void turnOn();
  virtual void turnOff();
  virtual void toggle();
  void handleAction(int event, int action);
  void setStep(int step);
  void setDefaultDimmedBrightness(int dimmedBrightness);
  void setFadeEffectTime(int timeMs);
  void setMinIterationBrightness(uint8_t minBright);
  void setMinMaxIterationDelay(uint16_t delayMs);

  void onInit();
  void iterateAlways();
  void onTimer();
  void onLoadState();
  void onSaveState();

  virtual RGBWBase &setDefaultStateOn();
  virtual RGBWBase &setDefaultStateOff();
  virtual RGBWBase &setDefaultStateRestore();
  // Set mapping between interface setting of brightness and actual value
  // set on device. Values should be between 0 and 1023 (min, max).
  // I.e. if limit is set to (100, 800), then values from Supla in range
  // 0-100% are mapped to PWM values in range 100 and 800.
  virtual RGBWBase &setBrightnessLimits(int min, int max);
  // Set mapping between interface setting of color brightness and actual value
  // set on device. Values should be between 0 and 1023 (min, max).
  virtual RGBWBase &setColorBrightnessLimits(int min, int max);

 protected:
  uint8_t addWithLimit(int value, int addition, int limit = 255);
  virtual void iterateDimmerRGBW(int rgbStep, int wStep);

  uint8_t buttonStep;               // 10
  uint8_t curRed;                   // 0 - 255
  uint8_t curGreen;                 // 0 - 255
  uint8_t curBlue;                  // 0 - 255
  uint8_t curColorBrightness;       // 0 - 100
  uint8_t curBrightness;            // 0 - 100
  uint8_t lastColorBrightness;      // 0 - 100
  uint8_t lastBrightness;           // 0 - 100
  uint8_t defaultDimmedBrightness;  // 20
  bool dimIterationDirection;
  int fadeEffect;
  int hwRed;              // 0 - 255
  int hwGreen;            // 0 - 255
  int hwBlue;             // 0 - 255
  int hwColorBrightness;  // 0 - 100
  int hwBrightness;       // 0 - 100
  int minBrightness = 0;
  int maxBrightness = 1023;
  int minColorBrightness = 0;
  int maxColorBrightness = 1023;
  uint16_t minMaxIterationDelay = 750;
  uint64_t lastTick = 0;
  uint64_t lastMsgReceivedMs = 0;
  uint64_t lastIterateDimmerTimestamp = 0;
  uint64_t iterationDelayTimestamp = 0;
  int8_t stateOnInit;
  uint8_t minIterationBrightness;
};

};  // namespace Control
};  // namespace Supla

#endif  // SRC_SUPLA_CONTROL_RGBW_BASE_H_

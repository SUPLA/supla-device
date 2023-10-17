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

class BrightnessAdjuster {
 public:
  virtual ~BrightnessAdjuster() = default;
  virtual int adjustBrightness(int input) = 0;
};

class GeometricBrightnessAdjuster : public BrightnessAdjuster {
 public:
  explicit GeometricBrightnessAdjuster(double power = 1.505, int offset = 0);
  int adjustBrightness(int input) override;

 private:
  double power = 1.505;
  int offset = 0;
};

class Button;

class RGBWBase : public ChannelElement, public ActionHandler {
 public:
  enum ButtonControlType {
    BUTTON_FOR_RGBW,
    BUTTON_FOR_RGB,
    BUTTON_FOR_W,
    BUTTON_NOT_USED
  };

  enum class AutoIterateMode {
    OFF,
    DIMMER,
    RGB,
    ALL
  };

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

  int handleNewValueFromServer(TSD_SuplaChannelNewValue *newValue) override;
  virtual void turnOn();
  virtual void turnOff();
  virtual void toggle();
  bool isOn();
  bool isOnW();
  bool isOnRGB();
  void handleAction(int event, int action) override;
  void setStep(int step);
  void setDefaultDimmedBrightness(int dimmedBrightness);
  void setFadeEffectTime(int timeMs);
  void setMinIterationBrightness(uint8_t minBright);
  void setMinMaxIterationDelay(uint16_t delayMs);

  void onInit() override;
  void iterateAlways() override;
  void onFastTimer() override;
  void onLoadState() override;
  void onSaveState() override;
  void onLoadConfig(SuplaDeviceClass *) override;

  void attach(Supla::Control::Button *);

  // Method is used by external integrations to prepare TSD_SuplaChannelNewValue
  // value for specific channel type (i.e. to prefill durationMS field when
  // required)
  void fillSuplaChannelNewValue(TSD_SuplaChannelNewValue *value) override;

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

  void setBrightnessAdjuster(BrightnessAdjuster *adjuster);
  int getCurrentDimmerBrightness() const;
  int getCurrentRGBBrightness() const;

 protected:
  uint8_t addWithLimit(int value, int addition, int limit = 255);
  virtual void iterateDimmerRGBW(int rgbStep, int wStep);
  // Set mapping between interface setting of brightness and actual value
  // set on device.
  // Input value is in range 0-100.
  // Returns value in range 0-1023 adjusted by selected function.
  int adjustBrightness(int value);

  double getStep(int step, int target, double current, int distance);

  uint8_t buttonStep = 10;               // 10
  uint8_t curRed = 0;                   // 0 - 255
  uint8_t curGreen = 0;                 // 0 - 255
  uint8_t curBlue = 0;                  // 0 - 255
  uint8_t curColorBrightness = 0;       // 0 - 100
  uint8_t curBrightness = 0;            // 0 - 100
  uint8_t lastColorBrightness = 100;      // 0 - 100
  uint8_t lastBrightness = 100;           // 0 - 100
  uint8_t defaultDimmedBrightness = 20;  // 20
  bool dimIterationDirection = false;
  int fadeEffect = 500;
  double hwRed = 0;              // 0 - 1023
  double hwGreen = 0;            // 0 - 1023
  double hwBlue = 0;             // 0 - 1023
  double hwColorBrightness = 0;  // 0 - 1023
  double hwBrightness = 0;       // 0 - 1023
  int minBrightness = 1;
  int maxBrightness = 1023;
  int minColorBrightness = 1;
  int maxColorBrightness = 1023;
  int redDistance = 0;
  int greenDistance = 0;
  int blueDistance = 0;
  int colorBrightnessDistance = 0;
  int brightnessDistance = 0;
  bool resetDisance = false;
  uint16_t minMaxIterationDelay = 750;
  uint32_t lastTick = 0;
  uint32_t lastMsgReceivedMs = 0;
  uint32_t lastIterateDimmerTimestamp = 0;
  uint32_t iterationDelayTimestamp = 0;
  int8_t stateOnInit = -1;
  uint8_t minIterationBrightness = 1;
  BrightnessAdjuster *brightnessAdjuster = nullptr;
  bool valueChanged = true;
  Supla::Control::Button *attachedButton = nullptr;
  enum ButtonControlType buttonControlType = BUTTON_FOR_RGBW;

  enum AutoIterateMode autoIterateMode = AutoIterateMode::OFF;
  uint32_t lastAutoIterateStartTimestamp = 0;
};

};  // namespace Control
};  // namespace Supla

#endif  // SRC_SUPLA_CONTROL_RGBW_BASE_H_

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

#define RGBW_STATE_ON_INIT_RESTORE -1
#define RGBW_STATE_ON_INIT_OFF     0
#define RGBW_STATE_ON_INIT_ON      1

namespace Supla {
namespace Control {

class BrightnessAdjuster {
 public:
  virtual ~BrightnessAdjuster() = default;
  virtual int adjustBrightness(int input) = 0;
  virtual void setMaxHwValue(int maxHwValue) = 0;
};

class GeometricBrightnessAdjuster : public BrightnessAdjuster {
 public:
  explicit GeometricBrightnessAdjuster(double power = 1.505,
                                       int offset = 0,
                                       int maxHwValue = 1023);
  void setMaxHwValue(int maxHwValue) override;
  int adjustBrightness(int input) override;

 private:
  double power = 1.505;
  int offset = 0;
  int maxHwValue = 1023;
};

class Button;

class RGBWBase : public ChannelElement, public ActionHandler {
 public:
  enum ButtonControlType : uint8_t {
    BUTTON_FOR_RGBW,
    BUTTON_FOR_RGB,
    BUTTON_FOR_W,
    BUTTON_NOT_USED
  };

  enum class AutoIterateMode : uint8_t {
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
                       bool toggle = false,
                       bool instant = false);

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
  void setMaxHwValue(int newMaxHwValue);

 protected:
  uint8_t addWithLimit(int value, int addition, int limit = 255);
  virtual void iterateDimmerRGBW(int rgbStep, int wStep);
  // Set mapping between interface setting of brightness and actual value
  // set on device.
  // Input value is in range 0-100.
  // Returns value in range 0-1023 adjusted by selected function.
  int adjustBrightness(int value);

  int getStep(int step, int target, int current, int distance) const;
  bool calculateAndUpdate(int targetValue,
                          uint16_t *hwValue,
                          int distance,
                          uint32_t *lastChangeMs) const;

  bool valueChanged = true;
  uint8_t buttonStep = 10;               // 10
  uint8_t curRed = 0;                   // 0 - 255
  uint8_t curGreen = 255;                 // 0 - 255
  uint8_t curBlue = 0;                  // 0 - 255
  uint8_t curColorBrightness = 0;       // 0 - 100
  uint8_t curBrightness = 0;            // 0 - 100
  uint8_t lastColorBrightness = 100;      // 0 - 100
  uint8_t lastBrightness = 100;           // 0 - 100
  uint8_t defaultDimmedBrightness = 20;  // 20
  bool dimIterationDirection = false;
  bool resetDisance = false;
  bool instant = false;
  int8_t stateOnInit = RGBW_STATE_ON_INIT_RESTORE;
  uint8_t minIterationBrightness = 1;

  enum ButtonControlType buttonControlType = BUTTON_FOR_RGBW;
  enum AutoIterateMode autoIterateMode = AutoIterateMode::OFF;

  uint16_t maxHwValue = 1023;
  uint16_t hwRed = 0;              // 0 - maxHwValue
  uint16_t hwGreen = 0;            // 0 - maxHwValue
  uint16_t hwBlue = 0;             // 0 - maxHwValue
  uint16_t hwColorBrightness = 0;  // 0 - maxHwValue
  uint16_t hwBrightness = 0;       // 0 - maxHwValue
  uint16_t minBrightness = 1;
  uint16_t maxBrightness = 1023;
  uint16_t minColorBrightness = 1;
  uint16_t maxColorBrightness = 1023;
  uint16_t redDistance = 0;
  uint16_t greenDistance = 0;
  uint16_t blueDistance = 0;
  uint16_t colorBrightnessDistance = 0;
  uint16_t brightnessDistance = 0;

  uint16_t minMaxIterationDelay = 750;
  uint16_t fadeEffect = 500;

  uint32_t lastTick = 0;
  uint32_t lastChangeRedMs = 0;
  uint32_t lastChangeGreenMs = 0;
  uint32_t lastChangeBlueMs = 0;
  uint32_t lastChangeColorBrightnessMs = 0;
  uint32_t lastChangeBrightnessMs = 0;
  uint32_t lastMsgReceivedMs = 0;
  uint32_t lastIterateDimmerTimestamp = 0;
  uint32_t iterationDelayTimestamp = 0;
  uint32_t lastAutoIterateStartTimestamp = 0;

  BrightnessAdjuster *brightnessAdjuster = nullptr;
  Supla::Control::Button *attachedButton = nullptr;
};

};  // namespace Control
};  // namespace Supla

#endif  // SRC_SUPLA_CONTROL_RGBW_BASE_H_

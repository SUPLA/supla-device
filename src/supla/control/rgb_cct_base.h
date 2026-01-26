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

#ifndef SRC_SUPLA_CONTROL_RGB_CCT_BASE_H_
#define SRC_SUPLA_CONTROL_RGB_CCT_BASE_H_

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

class RGBCCTBase : public ChannelElement, public ActionHandler {
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

  enum class LegacyChannelFunction : uint8_t {
    None,
    RGBW,
    RGB,
    Dimmer
  };

  /**
   * Constructor
   *
   * @param parent parent RGBW object - set to null, for first instance, then
   * remaining instances should be passed one after another in order to properly
   * handle channel disabling based on parent's function.
   */
  explicit RGBCCTBase(RGBCCTBase *parent = nullptr);
  virtual ~RGBCCTBase() = default;

  void purgeConfig() override;
  Supla::ApplyConfigResult applyChannelConfig(TSD_ChannelConfig *result,
                              bool local = false) override;
  void fillChannelConfig(void *channelConfig,
                         int *size,
                         uint8_t configType) override;

  virtual void setRGBCCTValueOnDevice(uint32_t red,
                                      uint32_t green,
                                      uint32_t blue,
                                      uint32_t colorBrightness,
                                      uint32_t white1Brightness,
                                      uint32_t white2Brightness) = 0;

  virtual void setRGBW(int red,
                       int green,
                       int blue,
                       int colorBrightness,
                       int whiteBrightness,
                       bool toggle = false,
                       bool instant = false);

  virtual void setRGBCCT(int red,
                         int green,
                         int blue,
                         int colorBrightness,
                         int whiteBrightness,
                         int whiteTemperature,
                         bool toggle = false,
                         bool instant = false);

  int32_t handleNewValueFromServer(TSD_SuplaChannelNewValue *newValue) override;
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
  bool isStateStorageMigrationNeeded() const override;
  void onLoadConfig(SuplaDeviceClass *) override;

  /**
   * Enables storage conversion from legacy channel function to new
   *
   * @param channelFunction None, RGBW, RGB, Dimmer
   */
  void convertStorageFromLegacyChannel(LegacyChannelFunction channelFunction);

  /**
   * Disables storage conversion from legacy channel function to new. Use
   * together with convertStorageFromLegacyChannel to keep legacy storage
   * format.
   */
  void setSkipLegacyMigration();

  void attach(Supla::Control::Button *);

  // Method is used by external integrations to prepare TSD_SuplaChannelNewValue
  // value for specific channel type (i.e. to prefill durationMS field when
  // required)
  void fillSuplaChannelNewValue(TSD_SuplaChannelNewValue *value) override;

  virtual RGBCCTBase &setDefaultStateOn();
  virtual RGBCCTBase &setDefaultStateOff();
  virtual RGBCCTBase &setDefaultStateRestore();
  // Set mapping between interface setting of brightness and actual value
  // set on device. Values should be between 0 and 1023 (min, max).
  // I.e. if limit is set to (100, 800), then values from Supla in range
  // 0-100% are mapped to PWM values in range 100 and 800.
  virtual RGBCCTBase &setBrightnessLimits(int min, int max);
  // Set mapping between interface setting of color brightness and actual value
  // set on device. Values should be between 0 and 1023 (min, max).
  virtual RGBCCTBase &setColorBrightnessLimits(int min, int max);

  void setBrightnessAdjuster(BrightnessAdjuster *adjuster);
  int getCurrentDimmerBrightness() const;
  int getCurrentRGBBrightness() const;
  void setMaxHwValue(int newMaxHwValue);

  /**
   * Checks if this instance has parent
   *
   * @return true if this instance has parent
   */
  bool hasParent() const;

  /**
   * Returns number of ancestor instances (by parent)
   *
   * @return number of ancestor instances
   */
  int getAncestorCount() const;

 protected:
  /**
   * Returns number of GPIO pins that are required to control this channel.
   * I.e. RGB channel requires 3 GPIOs, one is given by current RGBCCT object,
   * and remaining 2 are taken from descendant instances. So if descendant
   * instance call this on its parent, it will return 2
   *
   * @return number of GPIO pins that are required by parent object to allow
   *         proper operation of it's function
   */
  int getMissingGpioCount() const;

  void enableChannel();
  void disableChannel();

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
  uint8_t curWhiteBrightness = 0;            // 0 - 100
  uint8_t curWhiteTemperature = 0;            // 0 - 100
  uint8_t lastColorBrightness = 100;      // 0 - 100
  uint8_t lastWhiteBrightness = 100;           // 0 - 100
  uint8_t defaultDimmedBrightness = 20;  // 20
  bool dimIterationDirection = false;
  bool resetDisance = false;
  bool instant = false;
  bool enabled = true;
  bool initDone = false;
  bool skipLegacyMigration = false;
  int8_t stateOnInit = RGBW_STATE_ON_INIT_RESTORE;
  uint8_t minIterationBrightness = 1;
  LegacyChannelFunction legacyChannelFunction = LegacyChannelFunction::None;

  enum ButtonControlType buttonControlType = BUTTON_FOR_RGBW;
  enum AutoIterateMode autoIterateMode = AutoIterateMode::OFF;

  uint16_t maxHwValue = 1023;
  uint16_t hwRed = 0;              // 0 - maxHwValue
  uint16_t hwGreen = 0;            // 0 - maxHwValue
  uint16_t hwBlue = 0;             // 0 - maxHwValue
  uint16_t hwColorBrightness = 0;  // 0 - maxHwValue
  uint16_t hwBrightness = 0;       // 0 - maxHwValue
  uint16_t hwWhiteTemperature = 0;       // 0 - maxHwValue
  uint16_t hwWhite1Brightness = 0;       // 0 - maxHwValue
  uint16_t hwWhite2Brightness = 0;       // 0 - maxHwValue
  uint16_t minBrightness = 1;
  uint16_t maxBrightness = 1023;
  uint16_t minColorBrightness = 1;
  uint16_t maxColorBrightness = 1023;
  uint16_t redDistance = 0;
  uint16_t greenDistance = 0;
  uint16_t blueDistance = 0;
  uint16_t colorBrightnessDistance = 0;
  uint16_t brightnessDistance = 0;
  uint16_t whiteTemperatureDistance = 0;

  uint16_t minMaxIterationDelay = 750;
  uint16_t fadeEffect = 500;

  uint32_t lastTick = 0;
  uint32_t lastChangeRedMs = 0;
  uint32_t lastChangeGreenMs = 0;
  uint32_t lastChangeBlueMs = 0;
  uint32_t lastChangeColorBrightnessMs = 0;
  uint32_t lastChangeBrightnessMs = 0;
  uint32_t lastChangeWhiteTemperatureMs = 0;
  uint32_t lastMsgReceivedMs = 0;
  uint32_t lastIterateDimmerTimestamp = 0;
  uint32_t iterationDelayTimestamp = 0;
  uint32_t lastAutoIterateStartTimestamp = 0;

  float warmWhiteGain = 1.0;
  float coldWhiteGain = 1.0;

  BrightnessAdjuster *brightnessAdjuster = nullptr;
  Supla::Control::Button *attachedButton = nullptr;
  RGBCCTBase *parent = nullptr;
};

};  // namespace Control
};  // namespace Supla

#endif  // SRC_SUPLA_CONTROL_RGB_CCT_BASE_H_

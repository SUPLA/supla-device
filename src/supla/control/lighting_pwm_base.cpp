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

#include "lighting_pwm_base.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <supla/control/button.h>
#include <supla/log_wrapper.h>
#include <supla/storage/config.h>
#include <supla/storage/config_tags.h>

#include "../storage/storage.h"
#include "../time.h"
#include "../tools.h"
#include "supla/actions.h"

#ifdef ARDUINO_ARCH_ESP32
int esp32PwmChannelCounter = 0;
#endif

constexpr int SUPLA_MAX_OUTPUT_COUNT = 5;

namespace Supla {
namespace Control {

namespace {

float normalizeLimitRatio(float value) {
  if (value < 0.0f) {
    return 0.0f;
  }
  if (value > 1.0f) {
    return 1.0f;
  }
  return value;
}

void normalizeLimitPair(float &minValue, float &maxValue) {
  minValue = normalizeLimitRatio(minValue);
  maxValue = normalizeLimitRatio(maxValue);
  if (minValue > maxValue) {
    minValue = maxValue;
  }
}

uint32_t ratioToHwValue(float ratio, uint32_t maxHwValue) {
  if (ratio <= 0.0f) {
    return 0;
  }
  if (ratio >= 1.0f) {
    return maxHwValue;
  }
  return static_cast<uint32_t>(lroundf(ratio * maxHwValue));
}

}  // namespace

GeometricBrightnessAdjuster::GeometricBrightnessAdjuster(double power,
                                                         int offset,
                                                         int maxHwValue)
    : power(power), offset(offset), maxHwValue(maxHwValue) {
}

int GeometricBrightnessAdjuster::adjustBrightness(int input) {
  if (input == 0) {
    return 0;
  }
  double result = (input + offset) / (100.0 + offset);
  result = pow(result, power);
  result = result * maxHwValue;
  if (result > maxHwValue) {
    result = maxHwValue;
  }
  if (result < 0) {
    result = 0;
  }
  return round(result);
}

void GeometricBrightnessAdjuster::setMaxHwValue(int maxHwValue) {
  this->maxHwValue = maxHwValue;
}

LightingPwmBase::LightingPwmBase(LightingPwmBase *parent) : parent(parent) {
  channel.setType(SUPLA_CHANNELTYPE_DIMMERANDRGBLED);
  channel.setFlag(SUPLA_CHANNEL_FLAG_RGBW_COMMANDS_SUPPORTED);
  channel.setFlag(SUPLA_CHANNEL_FLAG_RUNTIME_CHANNEL_CONFIG_UPDATE);
  channel.setFuncList(
      SUPLA_RGBW_BIT_FUNC_DIMMER | SUPLA_RGBW_BIT_FUNC_RGB_LIGHTING |
      SUPLA_RGBW_BIT_FUNC_DIMMER_AND_RGB_LIGHTING |
      SUPLA_RGBW_BIT_FUNC_DIMMER_CCT | SUPLA_RGBW_BIT_FUNC_DIMMER_CCT_AND_RGB);
  channel.setDefault(SUPLA_CHANNELFNC_DIMMER_CCT_AND_RGB);
  usedConfigTypes.set(SUPLA_CONFIG_TYPE_DEFAULT);
}

void LightingPwmBase::setBrightnessAdjuster(BrightnessAdjuster *adjuster) {
  if (brightnessAdjuster) {
    delete brightnessAdjuster;
  }
  brightnessAdjuster = adjuster;
  brightnessAdjuster->setMaxHwValue(maxHwValue);
}

void LightingPwmBase::setRGBW(int red,
                              int green,
                              int blue,
                              int colorBrightness,
                              int whiteBrightness,
                              bool toggle,
                              bool instant) {
  setRGBCCT(
      red, green, blue, colorBrightness, whiteBrightness, -1, toggle, instant);
}

void LightingPwmBase::setRGBCCT(int red,
                                int green,
                                int blue,
                                int colorBrightness,
                                int whiteBrightness,
                                int whiteTemperature,
                                bool toggle,
                                bool instant) {
  if (!instant) {
    // Stop brightness adjustment when some command is received
    autoIterateMode = AutoIterateMode::OFF;
  }
  if (toggle) {
    timing.lastMsgReceivedMs = 1;
  } else {
    timing.lastMsgReceivedMs = millis();
  }

  // Store last non 0 brightness for turn on/toggle operations
  if (toggle && colorBrightness == 100) {
    colorBrightness = lastNonZero.colorBrightness;
  } else if (colorBrightness > 0) {
    lastNonZero.colorBrightness = colorBrightness;
  }
  if (toggle && whiteBrightness == 100) {
    whiteBrightness = lastNonZero.whiteBrightness;
  } else if (whiteBrightness > 0) {
    lastNonZero.whiteBrightness = whiteBrightness;
  }

  // Store current values
  if (red >= 0) {
    requested.red = red;
  }
  if (green >= 0) {
    requested.green = green;
  }
  if (blue >= 0) {
    requested.blue = blue;
  }
  if (colorBrightness >= 0) {
    requested.colorBrightness = colorBrightness;
  }
  if (whiteBrightness >= 0) {
    requested.whiteBrightness = whiteBrightness;
  }
  if (whiteTemperature >= 0) {
    requested.whiteTemperature = whiteTemperature;
  }

  this->instant = instant;
  resetDisance = true;

  SUPLA_LOG_DEBUG("Light[%d]: %d,%d,%d,%d,%d,%d",
                  getChannelNumber(),
                  requested.red,
                  requested.green,
                  requested.blue,
                  requested.colorBrightness,
                  requested.whiteBrightness,
                  requested.whiteTemperature);

  // Schedule save in 5 s after state change
  Supla::Storage::ScheduleSave(5000, 2000);
}

void LightingPwmBase::iterateAlways() {
  if (timing.lastMsgReceivedMs != 0 &&
      millis() - timing.lastMsgReceivedMs >= 400) {
    timing.lastMsgReceivedMs = 0;
    // Send to Supla server new values
    channel.setNewValue(requested.red,
                        requested.green,
                        requested.blue,
                        requested.colorBrightness,
                        requested.whiteBrightness,
                        requested.whiteTemperature);
  }
  updateEnabledState();
}

void LightingPwmBase::updateEnabledState() {
  if (hasParent() && parent->getMissingGpioCount() > 0) {
    disableChannel();
  } else {
    enableChannel();
  }
}

int32_t LightingPwmBase::handleNewValueFromServer(
    TSD_SuplaChannelNewValue *newValue) {
  uint8_t whiteTemperature = static_cast<uint8_t>(newValue->value[7]);
  uint8_t command = static_cast<uint8_t>(newValue->value[6]);
  uint8_t toggleOnOff = static_cast<uint8_t>(newValue->value[5]);
  uint8_t red = static_cast<uint8_t>(newValue->value[4]);
  uint8_t green = static_cast<uint8_t>(newValue->value[3]);
  uint8_t blue = static_cast<uint8_t>(newValue->value[2]);
  uint8_t colorBrightness = static_cast<uint8_t>(newValue->value[1]);
  uint8_t whiteBrightness = static_cast<uint8_t>(newValue->value[0]);

  SUPLA_LOG_INFO(
      "Light[%d] received: R=%d, R=%d, B=%d, colorBright=%d, "
      "whiteBright=%d, whiteTemp=%d, Cmd=%d, toggleOnOff=%d",
      getChannelNumber(),
      red,
      green,
      blue,
      colorBrightness,
      whiteBrightness,
      whiteTemperature,
      command,
      toggleOnOff);

  if (whiteBrightness > 100) {
    whiteBrightness = 100;
  }
  if (colorBrightness > 100) {
    colorBrightness = 100;
  }
  if (whiteTemperature > 100) {
    whiteTemperature = 100;
  }

  switch (command) {
    case RGBW_COMMAND_NOT_SET: {
      setRGBCCT(red,
                green,
                blue,
                colorBrightness,
                whiteBrightness,
                whiteTemperature,
                toggleOnOff > 0);
      break;
    }
    case RGBW_COMMAND_TURN_ON_DIMMER: {
      setRGBCCT(-1, -1, -1, -1, lastNonZero.whiteBrightness, -1);
      break;
    }
    case RGBW_COMMAND_TURN_OFF_DIMMER: {
      setRGBCCT(-1, -1, -1, -1, 0, -1);
      break;
    }
    case RGBW_COMMAND_TOGGLE_DIMMER: {
      setRGBCCT(-1,
                -1,
                -1,
                -1,
                requested.whiteBrightness > 0 ? 0 : lastNonZero.whiteBrightness,
                -1);
      break;
    }
    case RGBW_COMMAND_TURN_ON_RGB: {
      setRGBCCT(-1, -1, -1, lastNonZero.colorBrightness, -1, -1);
      break;
    }
    case RGBW_COMMAND_TURN_OFF_RGB: {
      setRGBCCT(-1, -1, -1, 0, -1, -1);
      break;
    }
    case RGBW_COMMAND_TOGGLE_RGB: {
      setRGBCCT(-1,
                -1,
                -1,
                requested.colorBrightness > 0 ? 0 : lastNonZero.colorBrightness,
                -1,
                -1);
      break;
    }
    case RGBW_COMMAND_TURN_ON_ALL: {
      turnOn();
      break;
    }
    case RGBW_COMMAND_TURN_OFF_ALL: {
      turnOff();
      break;
    }
    case RGBW_COMMAND_TOGGLE_ALL: {
      toggle();
      break;
    }
    case RGBW_COMMAND_SET_BRIGHTNESS_WITHOUT_TURN_ON: {
      if (requested.whiteBrightness > 0) {
        setRGBCCT(-1, -1, -1, -1, whiteBrightness, -1);
      } else {
        lastNonZero.whiteBrightness = whiteBrightness;
      }
      break;
    }
    case RGBW_COMMAND_SET_COLOR_BRIGHTNESS_WITHOUT_TURN_ON: {
      if (requested.colorBrightness > 0) {
        setRGBCCT(-1, -1, -1, colorBrightness, -1, -1);
      } else {
        lastNonZero.colorBrightness = colorBrightness;
      }
      break;
    }
    case RGBW_COMMAND_SET_RGB_WITHOUT_TURN_ON: {
      setRGBCCT(red, green, blue, -1, -1, -1);
      break;
    }
    case RGBW_COMMAND_BRIGHTNESS_ADJUSTMENT_DIMMER_START: {
      timing.lastAutoIterateStartTimestamp = millis();
      if (autoIterateMode == AutoIterateMode::OFF) {
        autoIterateMode = AutoIterateMode::DIMMER;
      } else if (autoIterateMode == AutoIterateMode::RGB) {
        autoIterateMode = AutoIterateMode::ALL;
      }
      break;
    }
    case RGBW_COMMAND_BRIGHTNESS_ADJUSTMENT_COLOR_START: {
      timing.lastAutoIterateStartTimestamp = millis();
      if (autoIterateMode == AutoIterateMode::OFF) {
        autoIterateMode = AutoIterateMode::RGB;
      } else if (autoIterateMode == AutoIterateMode::DIMMER) {
        autoIterateMode = AutoIterateMode::ALL;
      }
      break;
    }
    case RGBW_COMMAND_BRIGHTNESS_ADJUSTMENT_ALL_START: {
      autoIterateMode = AutoIterateMode::ALL;
      timing.lastAutoIterateStartTimestamp = millis();
      break;
    }
    case RGBW_COMMAND_BRIGHTNESS_ADJUSTMENT_DIMMER_STOP: {
      if (autoIterateMode == AutoIterateMode::DIMMER) {
        autoIterateMode = AutoIterateMode::OFF;
      } else if (autoIterateMode == AutoIterateMode::ALL) {
        autoIterateMode = AutoIterateMode::RGB;
      }
      break;
    }
    case RGBW_COMMAND_BRIGHTNESS_ADJUSTMENT_COLOR_STOP: {
      if (autoIterateMode == AutoIterateMode::RGB) {
        autoIterateMode = AutoIterateMode::OFF;
      } else if (autoIterateMode == AutoIterateMode::ALL) {
        autoIterateMode = AutoIterateMode::DIMMER;
      }
      break;
    }
    case RGBW_COMMAND_BRIGHTNESS_ADJUSTMENT_ALL_STOP: {
      autoIterateMode = AutoIterateMode::OFF;
      break;
    }
    case RGBW_COMMAND_SET_WHITE_TEMPERATURE_WITHOUT_TURN_ON: {
      setRGBCCT(-1, -1, -1, -1, -1, whiteTemperature);
      break;
    }
    default: {
      break;
    }
  }
  return -1;
}

void LightingPwmBase::turnOn() {
  setRGBCCT(
      -1, -1, -1, lastNonZero.colorBrightness, lastNonZero.whiteBrightness, -1);
}
void LightingPwmBase::turnOff() {
  setRGBCCT(-1, -1, -1, 0, 0, -1);
}

void LightingPwmBase::toggle() {
  if (isOn()) {
    turnOff();
  } else {
    turnOn();
  }
}

bool LightingPwmBase::isOn() {
  return isOnRGB() || isOnW();
}

bool LightingPwmBase::isOnW() {
  return requested.whiteBrightness > 0;
}

bool LightingPwmBase::isOnRGB() {
  return requested.colorBrightness > 0;
}

uint8_t LightingPwmBase::addWithLimit(int value, int addition, int limit) {
  if (addition > 0 && value + addition > limit) {
    return limit;
  }
  if (addition < 0 && value + addition < 0) {
    return 0;
  }
  return value + addition;
}

void LightingPwmBase::handleAction(int event, int action) {
  (void)(event);
  switch (action) {
    case TURN_ON: {
      turnOn();
      break;
    }
    case TURN_OFF: {
      turnOff();
      break;
    }
    case TOGGLE: {
      toggle();
      break;
    }
    case BRIGHTEN_ALL: {
      setRGBCCT(-1,
                -1,
                -1,
                addWithLimit(requested.colorBrightness, buttonStep, 100),
                addWithLimit(requested.whiteBrightness, buttonStep, 100),
                -1);
      break;
    }
    case DIM_ALL: {
      setRGBCCT(-1,
                -1,
                -1,
                addWithLimit(requested.colorBrightness, -buttonStep, 100),
                addWithLimit(requested.whiteBrightness, -buttonStep, 100),
                -1);
      break;
    }
    case BRIGHTEN_R: {
      setRGBCCT(addWithLimit(requested.red, buttonStep), -1, -1, -1, -1, -1);
      break;
    }
    case DIM_R: {
      setRGBCCT(addWithLimit(requested.red, -buttonStep), -1, -1, -1, -1, -1);
      break;
    }
    case BRIGHTEN_G: {
      setRGBCCT(-1, addWithLimit(requested.green, buttonStep), -1, -1, -1, -1);
      break;
    }
    case DIM_G: {
      setRGBCCT(-1, addWithLimit(requested.green, -buttonStep), -1, -1, -1, -1);
      break;
    }
    case BRIGHTEN_B: {
      setRGBCCT(-1, -1, addWithLimit(requested.blue, buttonStep), -1, -1, -1);
      break;
    }
    case DIM_B: {
      setRGBCCT(-1, -1, addWithLimit(requested.blue, -buttonStep), -1, -1, -1);
      break;
    }
    case BRIGHTEN_W: {
      setRGBCCT(-1,
                -1,
                -1,
                -1,
                addWithLimit(requested.whiteBrightness, buttonStep, 100),
                -1);
      break;
    }
    case DIM_W: {
      setRGBCCT(-1,
                -1,
                -1,
                -1,
                addWithLimit(requested.whiteBrightness, -buttonStep, 100),
                -1);
      break;
    }
    case BRIGHTEN_RGB: {
      setRGBCCT(-1,
                -1,
                -1,
                addWithLimit(requested.colorBrightness, buttonStep, 100),
                -1,
                -1);
      break;
    }
    case DIM_RGB: {
      setRGBCCT(-1,
                -1,
                -1,
                addWithLimit(requested.colorBrightness, -buttonStep, 100),
                -1,
                -1);
      break;
    }
    case TURN_ON_RGB: {
      setRGBCCT(-1, -1, -1, lastNonZero.colorBrightness, -1, -1);
      break;
    }
    case TURN_OFF_RGB: {
      setRGBCCT(-1, -1, -1, 0, -1, -1);
      break;
    }
    case TOGGLE_RGB: {
      setRGBCCT(-1,
                -1,
                -1,
                requested.colorBrightness > 0 ? 0 : lastNonZero.colorBrightness,
                -1,
                -1);
      break;
    }
    case TURN_ON_W: {
      setRGBCCT(-1, -1, -1, -1, lastNonZero.whiteBrightness, -1);
      break;
    }
    case TURN_OFF_W: {
      setRGBCCT(-1, -1, -1, -1, 0, -1);
      break;
    }
    case TOGGLE_W: {
      setRGBCCT(-1,
                -1,
                -1,
                -1,
                requested.whiteBrightness > 0 ? 0 : lastNonZero.whiteBrightness,
                -1);
      break;
    }
    case TURN_ON_RGB_DIMMED: {
      if (requested.colorBrightness == 0) {
        setRGBCCT(-1, -1, -1, defaultDimmedBrightness, -1, -1);
      }
      break;
    }
    case TURN_ON_W_DIMMED: {
      if (requested.whiteBrightness == 0) {
        setRGBCCT(-1, -1, -1, -1, defaultDimmedBrightness, -1);
      }
      break;
    }
    case TURN_ON_ALL_DIMMED: {
      if (requested.whiteBrightness == 0 && requested.colorBrightness == 0) {
        setRGBCCT(
            -1, -1, -1, defaultDimmedBrightness, defaultDimmedBrightness, -1);
      }
      break;
    }
    case ITERATE_DIM_RGB: {
      iterateDimmerRGBW(buttonStep, 0);
      break;
    }
    case ITERATE_DIM_W: {
      iterateDimmerRGBW(0, buttonStep);
      break;
    }
    case ITERATE_DIM_ALL: {
      iterateDimmerRGBW(buttonStep, buttonStep);
      break;
    }
  }
}

void LightingPwmBase::iterateDimmerRGBW(int rgbStep, int wStep) {
  // if we iterate both RGB and W, then we should sync brightness
  if (rgbStep > 0 && wStep > 0) {
    requested.whiteBrightness = requested.colorBrightness;
  }

  // change iteration direction if there was no action in last 0.5 s
  if (millis() - timing.lastIterateDimmerTimestamp >= 500) {
    dimIterationDirection = !dimIterationDirection;
    timing.iterationDelayTimestamp = 0;
    if (requested.whiteBrightness <= 5) {
      dimIterationDirection = false;
    } else if (requested.whiteBrightness >= 95) {
      dimIterationDirection = true;
    }
    if (millis() - timing.lastIterateDimmerTimestamp >= 10000) {
      if (requested.whiteBrightness <= 40) {
        dimIterationDirection = false;
      } else if (requested.whiteBrightness >= 60) {
        dimIterationDirection = true;
      }
    }
  }

  timing.lastIterateDimmerTimestamp = millis();

  if (rgbStep > 0) {
    if (requested.colorBrightness <= minIterationBrightness &&
        dimIterationDirection == true) {
      if (timing.iterationDelayTimestamp == 0) {
        timing.iterationDelayTimestamp = millis();
      }
      if (millis() - timing.iterationDelayTimestamp > minMaxIterationDelay) {
        dimIterationDirection = false;
        timing.iterationDelayTimestamp = 0;
      } else {
        return;
      }
    } else if (requested.colorBrightness == 100 &&
               dimIterationDirection == false) {
      if (timing.iterationDelayTimestamp == 0) {
        timing.iterationDelayTimestamp = millis();
      }
      if (millis() - timing.iterationDelayTimestamp > minMaxIterationDelay) {
        dimIterationDirection = true;
        timing.iterationDelayTimestamp = 0;
      } else {
        return;
      }
    }
  } else if (wStep > 0) {
    if (requested.whiteBrightness <= minIterationBrightness &&
        dimIterationDirection == true) {
      if (timing.iterationDelayTimestamp == 0) {
        timing.iterationDelayTimestamp = millis();
      }
      if (millis() - timing.iterationDelayTimestamp > minMaxIterationDelay) {
        dimIterationDirection = false;
        timing.iterationDelayTimestamp = 0;
      } else {
        return;
      }
    } else if (requested.whiteBrightness == 100 &&
               dimIterationDirection == false) {
      if (timing.iterationDelayTimestamp == 0) {
        timing.iterationDelayTimestamp = millis();
      }
      if (millis() - timing.iterationDelayTimestamp > minMaxIterationDelay) {
        dimIterationDirection = true;
        timing.iterationDelayTimestamp = 0;
      } else {
        return;
      }
    }
  }
  timing.iterationDelayTimestamp = 0;

  // If direction is dim, then brightness step is set to negative
  if (dimIterationDirection) {
    rgbStep = -rgbStep;
    wStep = -wStep;
  }

  if (rgbStep && requested.colorBrightness + rgbStep < minIterationBrightness) {
    rgbStep = minIterationBrightness - requested.colorBrightness;
  }
  if (wStep && requested.whiteBrightness + wStep < minIterationBrightness) {
    wStep = minIterationBrightness - requested.whiteBrightness;
  }

  if ((wStep != 0 && requested.whiteBrightness == 0) ||
      (rgbStep != 0 && requested.colorBrightness == 0)) {
    timing.iterationDelayTimestamp = millis();
    dimIterationDirection = true;
  }

  setRGBCCT(-1,
            -1,
            -1,
            addWithLimit(requested.colorBrightness, rgbStep, 100),
            addWithLimit(requested.whiteBrightness, wStep, 100),
            -1,
            false,
            true);
}

void LightingPwmBase::setStep(int step) {
  buttonStep = step;
}

void LightingPwmBase::setDefaultDimmedBrightness(int dimmedBrightness) {
  defaultDimmedBrightness = dimmedBrightness;
}

void LightingPwmBase::setFadeEffectTime(int timeMs) {
  fadeEffect = timeMs;
}

int LightingPwmBase::adjustBrightness(int value) {
  if (brightnessAdjuster) {
    return brightnessAdjuster->adjustBrightness(value);
  }
  return adjustRange(value, 0, 100, 0, maxHwValue);
}

int LightingPwmBase::getStep(int step, int target, int current) const {
  if (step && target != current) {
    int result = step;
    if (target > current) {
      if (current + result > target) {
        result = target - current;
      }
      return result;
    } else if (target < current) {
      result = -step;
      if (current + result < target) {
        result = target - current;
      }
      return result;
    }
  }
  return 0;
}

bool LightingPwmBase::calculateAndUpdate(int targetValue,
                                         int16_t *hwValue,
                                         int distance,
                                         uint32_t *lastChangeMs,
                                         const uint32_t now) const {
  uint32_t timeDiff = now - *lastChangeMs;
  if (targetValue == *hwValue || timeDiff == 0) {
    *lastChangeMs = now;
    return false;
  }

  int currentFadeEffectTime = fadeEffect;
  if (distance < maxHwValue / 10) {
    currentFadeEffectTime = fadeEffect / 3;
  }

  float divider = 1.0 * currentFadeEffectTime / timeDiff;
  if (divider <= 1) {
    divider = 1;
  }

  int step = distance / divider;
  if (step < 1) {
    return false;
  }

  int valueStep = getStep(step, targetValue, *hwValue);
  if (valueStep == 0) {
    return false;
  }

  *hwValue += valueStep;
  *lastChangeMs = now;
  return true;
}

void LightingPwmBase::onFastTimer() {
  if (!enabled) {
    return;
  }
  uint32_t now = millis();

  uint32_t fn = getChannel()->getDefaultFunction();
  if (fn != previousChannelFunction) {
    autoIterateMode = AutoIterateMode::OFF;
    auto fnCopy = previousChannelFunction;
    previousChannelFunction = fn;
    fn = fnCopy;
    setRGBCCT(0, 255, 0, 0, 0, -1, 0, 1);
  }

  if (timing.lastTick == 0) {
    timing.lastTick = now;
    timing.lastChangeRedMs = now;
    timing.lastChangeGreenMs = now;
    timing.lastChangeBlueMs = now;
    timing.lastChangeBrightnessMs = now;
    timing.lastChangeWhiteTemperatureMs = now;
    timing.lastChangeColorBrightnessMs = now;
    return;
  }

  if (autoIterateMode != AutoIterateMode::OFF &&
      now - timing.lastAutoIterateStartTimestamp < 10000) {
    if (now - timing.lastIterateDimmerTimestamp >= 35) {
      // timing.lastIterateDimmerTimestamp is updated in handleAction calls
      // below
      switch (autoIterateMode) {
        case AutoIterateMode::DIMMER: {
          handleAction(0, Supla::ITERATE_DIM_W);
          break;
        }
        case AutoIterateMode::RGB: {
          handleAction(0, Supla::ITERATE_DIM_RGB);
          break;
        }
        case AutoIterateMode::ALL: {
          handleAction(0, Supla::ITERATE_DIM_ALL);
          break;
        }
        default:
          break;
      }
    }
  } else {
    // disable auto iterate after 10 s timeout
    autoIterateMode = AutoIterateMode::OFF;
  }

  uint32_t timeDiff = now - timing.lastTick;

  if (timeDiff == 0) {
    return;
  }

  timing.lastTick = now;
  bool valueChanged = false;

  if (hardware.red == -1) {
    hardware.red = 0;
    hardware.green = 0;
    hardware.blue = 0;
    hardware.colorBrightness = 0;
    hardware.brightness = 0;
    hardware.whiteTemperature = 0;
    valueChanged = true;
  }

  const bool useRGB = (fn == SUPLA_CHANNELFNC_RGBLIGHTING) ||
                      (fn == SUPLA_CHANNELFNC_DIMMERANDRGBLIGHTING) ||
                      (fn == SUPLA_CHANNELFNC_DIMMER_CCT_AND_RGB);

  const bool useDimmer = (fn == SUPLA_CHANNELFNC_DIMMER) ||
                         (fn == SUPLA_CHANNELFNC_DIMMER_CCT) ||
                         (fn == SUPLA_CHANNELFNC_DIMMERANDRGBLIGHTING) ||
                         (fn == SUPLA_CHANNELFNC_DIMMER_CCT_AND_RGB);

  const bool useCCT = (fn == SUPLA_CHANNELFNC_DIMMER_CCT) ||
                      (fn == SUPLA_CHANNELFNC_DIMMER_CCT_AND_RGB);

  // target values are in 0..maxHwValue range
  int targetRed = 0;
  int targetGreen = 0;
  int targetBlue = 0;
  int targetColorBrightness = 0;
  int targetBrightness = 0;
  int targetWhiteTemperature = 0;

  if (useRGB) {
    targetRed = adjustRange(requested.red, 0, 255, 0, maxHwValue);
    targetGreen = adjustRange(requested.green, 0, 255, 0, maxHwValue);
    targetBlue = adjustRange(requested.blue, 0, 255, 0, maxHwValue);
    targetColorBrightness = adjustBrightness(requested.colorBrightness);
  }
  if (useDimmer) {
    targetBrightness = adjustBrightness(requested.whiteBrightness);
  }
  if (useCCT) {
    targetWhiteTemperature =
        adjustRange(requested.whiteTemperature, 0, 100, 0, maxHwValue);
  }

  if (resetDisance) {
    resetDisance = false;

    if (useRGB) {
      hardware.redDistance = abs(targetRed - hardware.red);
      hardware.greenDistance = abs(targetGreen - hardware.green);
      hardware.blueDistance = abs(targetBlue - hardware.blue);
      hardware.colorBrightnessDistance =
          abs(targetColorBrightness - hardware.colorBrightness);
    }
    if (useDimmer) {
      hardware.brightnessDistance = abs(targetBrightness - hardware.brightness);
    }
    if (useCCT) {
      hardware.whiteTemperatureDistance =
          abs(targetWhiteTemperature - hardware.whiteTemperature);
    }
  }

  if (instant) {
    hardware.red = targetRed;
    hardware.green = targetGreen;
    hardware.blue = targetBlue;
    hardware.colorBrightness = targetColorBrightness;
    hardware.brightness = targetBrightness;
    hardware.whiteTemperature = targetWhiteTemperature;
    valueChanged = true;
    instant = false;
  } else {
    if (useRGB) {
      if (calculateAndUpdate(targetRed,
                             &hardware.red,
                             hardware.redDistance,
                             &timing.lastChangeRedMs,
                             now)) {
        valueChanged = true;
      }
      if (calculateAndUpdate(targetGreen,
                             &hardware.green,
                             hardware.greenDistance,
                             &timing.lastChangeGreenMs,
                             now)) {
        valueChanged = true;
      }
      if (calculateAndUpdate(targetBlue,
                             &hardware.blue,
                             hardware.blueDistance,
                             &timing.lastChangeBlueMs,
                             now)) {
        valueChanged = true;
      }
      if (calculateAndUpdate(targetColorBrightness,
                             &hardware.colorBrightness,
                             hardware.colorBrightnessDistance,
                             &timing.lastChangeColorBrightnessMs,
                             now)) {
        valueChanged = true;
      }
    }
    if (useDimmer) {
      if (calculateAndUpdate(targetBrightness,
                             &hardware.brightness,
                             hardware.brightnessDistance,
                             &timing.lastChangeBrightnessMs,
                             now)) {
        valueChanged = true;
      }
    }
    if (useCCT) {
      if (calculateAndUpdate(targetWhiteTemperature,
                             &hardware.whiteTemperature,
                             hardware.whiteTemperatureDistance,
                             &timing.lastChangeWhiteTemperatureMs,
                             now)) {
        valueChanged = true;
      }
    }
  }

  if (!valueChanged) {
    return;
  }

  // RGB Color brightness
  uint32_t adjColorBrightness = hardware.colorBrightness;
  if (useRGB && hardware.colorBrightness > 0) {
    const uint32_t minColorBrightness =
        ratioToHwValue(minColorBrightnessRatio, maxHwValue);
    const uint32_t maxColorBrightness =
        ratioToHwValue(maxColorBrightnessRatio, maxHwValue);
    adjColorBrightness = adjustRange(adjColorBrightness,
                                     1,
                                     maxHwValue,
                                     minColorBrightness,
                                     maxColorBrightness);
  } else {
    hardware.colorBrightness = 0;
    adjColorBrightness = 0;
  }

  // White channel(s) brightness
  uint32_t adjBrightness = hardware.brightness;
  if (useDimmer && hardware.brightness > 0) {
    const uint32_t minBrightness =
        ratioToHwValue(minBrightnessRatio, maxHwValue);
    const uint32_t maxBrightness =
        ratioToHwValue(maxBrightnessRatio, maxHwValue);
    adjBrightness =
        adjustRange(adjBrightness, 1, maxHwValue, minBrightness, maxBrightness);
  } else {
    hardware.brightness = 0;
    adjBrightness = 0;
  }

  // by default white1 is warm, white2 is cold.
  // whiteTemperature is in 0..100 range, where 0 is warm and 100 is cold
  uint32_t white1Brightness = adjBrightness;
  uint32_t white2Brightness = 0;

  if (useCCT && hardware.whiteTemperature > 0) {
    const uint32_t minBrightness =
        ratioToHwValue(minBrightnessRatio, maxHwValue);
    float white2Fraction = 1.0 * hardware.whiteTemperature / maxHwValue;
    white2Brightness = adjBrightness * white2Fraction * warmWhiteGain;
    white1Brightness = adjBrightness * (1.0 - white2Fraction) * coldWhiteGain;
    if (white1Brightness > 0 && white1Brightness < minBrightness) {
      white1Brightness = minBrightness;
    }
    if (white2Brightness > 0 && white2Brightness < minBrightness) {
      white2Brightness = minBrightness;
    }
    if (white1Brightness > maxHwValue) {
      white1Brightness = maxHwValue;
    }
    if (white2Brightness > maxHwValue) {
      white2Brightness = maxHwValue;
    }
    // TODO(klew): add warm/cold channel swap
  }

  uint32_t red = 0;
  uint32_t green = 0;
  uint32_t blue = 0;
  if (useRGB) {
    red = hardware.red * adjColorBrightness / maxHwValue;
    green = hardware.green * adjColorBrightness / maxHwValue;
    blue = hardware.blue * adjColorBrightness / maxHwValue;
    if (red > maxHwValue) {
      red = maxHwValue;
    }
    if (green > maxHwValue) {
      green = maxHwValue;
    }
    if (blue > maxHwValue) {
      blue = maxHwValue;
    }
  }

  uint32_t valueAdj[SUPLA_MAX_OUTPUT_COUNT] = {0};
  usedChannels = 0;

  if (useRGB) {
    valueAdj[usedChannels++] = red;
    valueAdj[usedChannels++] = green;
    valueAdj[usedChannels++] = blue;
  }

  if (useDimmer) {
    valueAdj[usedChannels++] = white1Brightness;
  }

  if (useCCT) {
    valueAdj[usedChannels++] = white2Brightness;
  }

  if (usedChannels > 0) {
    setRGBCCTValueOnDevice(valueAdj, usedChannels);
    for (int i = 0; i < usedChannels; i++) {
      valueAdj[i] = valueAdj[i] * 100.0 / maxHwValue;
    }
  }
}

void LightingPwmBase::onInit() {
  updateEnabledState();
  if (!enabled) {
    SUPLA_LOG_DEBUG("Light[%d] disabled", getChannel()->getChannelNumber());
  }
  if (attachedButton) {
    SUPLA_LOG_DEBUG("Light[%d] configuring attachedButton, control type %d",
                    getChannel()->getChannelNumber(),
                    buttonControlType);
    if (attachedButton->isMonostable()) {
      SUPLA_LOG_DEBUG("Light[%d] configuring monostable button",
                      getChannel()->getChannelNumber());
      switch (buttonControlType) {
        case BUTTON_FOR_RGBW: {
          attachedButton->addAction(
              Supla::ITERATE_DIM_ALL, this, Supla::ON_HOLD);
          attachedButton->addAction(Supla::TOGGLE, this, Supla::ON_CLICK_1);
          break;
        }
        case BUTTON_FOR_RGB: {
          attachedButton->addAction(
              Supla::ITERATE_DIM_RGB, this, Supla::ON_HOLD);
          attachedButton->addAction(Supla::TOGGLE_RGB, this, Supla::ON_CLICK_1);
          break;
        }
        case BUTTON_FOR_W: {
          attachedButton->addAction(Supla::ITERATE_DIM_W, this, Supla::ON_HOLD);
          attachedButton->addAction(Supla::TOGGLE_W, this, Supla::ON_CLICK_1);
          break;
        }
        case BUTTON_NOT_USED: {
          break;
        }
      }
    } else if (attachedButton->isBistable()) {
      SUPLA_LOG_DEBUG("Light[%d] configuring bistable button",
                      getChannel()->getChannelNumber());
      switch (buttonControlType) {
        case BUTTON_FOR_RGBW: {
          attachedButton->addAction(
              Supla::TOGGLE, this, Supla::CONDITIONAL_ON_CHANGE);
          break;
        }
        case BUTTON_FOR_RGB: {
          attachedButton->addAction(
              Supla::TOGGLE_RGB, this, Supla::CONDITIONAL_ON_CHANGE);
          break;
        }
        case BUTTON_FOR_W: {
          attachedButton->addAction(
              Supla::TOGGLE_W, this, Supla::CONDITIONAL_ON_CHANGE);
          break;
        }
        case BUTTON_NOT_USED: {
          break;
        }
      }
    } else if (attachedButton->isMotionSensor() ||
               attachedButton->isCentral()) {
      SUPLA_LOG_DEBUG("Light[%d] configuring motion sensor/central button",
                      getChannel()->getChannelNumber());
      switch (buttonControlType) {
        case BUTTON_FOR_RGBW: {
          attachedButton->addAction(Supla::TURN_ON, this, Supla::ON_PRESS);
          attachedButton->addAction(Supla::TURN_OFF, this, Supla::ON_RELEASE);
          break;
        }
        case BUTTON_FOR_RGB: {
          attachedButton->addAction(Supla::TURN_ON_RGB, this, Supla::ON_PRESS);
          attachedButton->addAction(
              Supla::TURN_OFF_RGB, this, Supla::ON_RELEASE);
          break;
        }
        case BUTTON_FOR_W: {
          attachedButton->addAction(Supla::TURN_ON_W, this, Supla::ON_PRESS);
          attachedButton->addAction(Supla::TURN_OFF_W, this, Supla::ON_RELEASE);
          break;
        }
        case BUTTON_NOT_USED: {
          break;
        }
      }
      if (attachedButton->getLastState() == Supla::Control::PRESSED) {
        SUPLA_LOG_DEBUG("Light[%d] button pressed",
                        getChannel()->getChannelNumber());
        switch (buttonControlType) {
          case BUTTON_FOR_RGBW: {
            requested.colorBrightness = lastNonZero.colorBrightness;
            requested.whiteBrightness = lastNonZero.whiteBrightness;
            break;
          }
          case BUTTON_FOR_RGB: {
            requested.colorBrightness = lastNonZero.colorBrightness;
            break;
          }
          case BUTTON_FOR_W: {
            requested.whiteBrightness = lastNonZero.whiteBrightness;
            break;
          }
          case BUTTON_NOT_USED: {
            break;
          }
        }
      } else {
        SUPLA_LOG_DEBUG("Light[%d] button not pressed",
                        getChannel()->getChannelNumber());
        switch (buttonControlType) {
          case BUTTON_FOR_RGBW: {
            requested.colorBrightness = 0;
            requested.whiteBrightness = 0;
            break;
          }
          case BUTTON_FOR_RGB: {
            requested.colorBrightness = 0;
            break;
          }
          case BUTTON_FOR_W: {
            requested.whiteBrightness = 0;
            break;
          }
          case BUTTON_NOT_USED: {
            break;
          }
        }
      }
    } else {
      SUPLA_LOG_WARNING("Light[%d] unknown button type",
                        getChannel()->getChannelNumber());
    }
  }

  bool toggle = false;
  if (stateOnInit == RGBW_STATE_ON_INIT_ON) {
    SUPLA_LOG_DEBUG("Light[%d] TURN on onInit",
                    getChannel()->getChannelNumber());
    requested.colorBrightness = 100;
    requested.whiteBrightness = 100;
    toggle = true;
  } else if (stateOnInit == RGBW_STATE_ON_INIT_OFF) {
    SUPLA_LOG_DEBUG("Light[%d] TURN off onInit",
                    getChannel()->getChannelNumber());
    requested.colorBrightness = 0;
    requested.whiteBrightness = 0;
  }

  initDone = true;

  previousChannelFunction = getChannel()->getDefaultFunction();

  setRGBCCT(requested.red,
            requested.green,
            requested.blue,
            requested.colorBrightness,
            requested.whiteBrightness,
            requested.whiteTemperature,
            toggle);
}

void LightingPwmBase::onSaveState() {
  if (!skipLegacyMigration && initDone &&
      legacyChannelFunction != LegacyChannelFunction::None) {
    // save migration done to cfg
    auto cfg = Supla::Storage::ConfigInstance();
    if (cfg) {
      char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
      Supla::Config::generateKey(key,
                                 getChannel()->getChannelNumber(),
                                 Supla::ConfigTag::LegacyMigrationTag);
      cfg->setUInt8(key, 1);
      legacyChannelFunction = LegacyChannelFunction::None;
    }
  }

  switch (legacyChannelFunction) {
    case LegacyChannelFunction::None: {
      Supla::Storage::WriteState((unsigned char *)&requested.red,
                                 sizeof(requested.red));
      Supla::Storage::WriteState((unsigned char *)&requested.green,
                                 sizeof(requested.green));
      Supla::Storage::WriteState((unsigned char *)&requested.blue,
                                 sizeof(requested.blue));
      Supla::Storage::WriteState((unsigned char *)&requested.colorBrightness,
                                 sizeof(requested.colorBrightness));
      Supla::Storage::WriteState((unsigned char *)&requested.whiteBrightness,
                                 sizeof(requested.whiteBrightness));
      Supla::Storage::WriteState((unsigned char *)&lastNonZero.colorBrightness,
                                 sizeof(lastNonZero.colorBrightness));
      Supla::Storage::WriteState((unsigned char *)&lastNonZero.whiteBrightness,
                                 sizeof(lastNonZero.whiteBrightness));
      Supla::Storage::WriteState((unsigned char *)&requested.whiteTemperature,
                                 sizeof(requested.whiteTemperature));
      break;
    }
    case LegacyChannelFunction::RGBW: {
      Supla::Storage::WriteState((unsigned char *)&requested.red,
                                 sizeof(requested.red));
      Supla::Storage::WriteState((unsigned char *)&requested.green,
                                 sizeof(requested.green));
      Supla::Storage::WriteState((unsigned char *)&requested.blue,
                                 sizeof(requested.blue));
      Supla::Storage::WriteState((unsigned char *)&requested.colorBrightness,
                                 sizeof(requested.colorBrightness));
      Supla::Storage::WriteState((unsigned char *)&requested.whiteBrightness,
                                 sizeof(requested.whiteBrightness));
      Supla::Storage::WriteState((unsigned char *)&lastNonZero.colorBrightness,
                                 sizeof(lastNonZero.colorBrightness));
      Supla::Storage::WriteState((unsigned char *)&lastNonZero.whiteBrightness,
                                 sizeof(lastNonZero.whiteBrightness));
      break;
    }
    case LegacyChannelFunction::RGB: {
      Supla::Storage::WriteState((unsigned char *)&requested.red,
                                 sizeof(requested.red));
      Supla::Storage::WriteState((unsigned char *)&requested.green,
                                 sizeof(requested.green));
      Supla::Storage::WriteState((unsigned char *)&requested.blue,
                                 sizeof(requested.blue));
      Supla::Storage::WriteState((unsigned char *)&requested.colorBrightness,
                                 sizeof(requested.colorBrightness));
      Supla::Storage::WriteState((unsigned char *)&lastNonZero.colorBrightness,
                                 sizeof(lastNonZero.colorBrightness));
      break;
    }
    case LegacyChannelFunction::Dimmer: {
      Supla::Storage::WriteState((unsigned char *)&requested.whiteBrightness,
                                 sizeof(requested.whiteBrightness));
      Supla::Storage::WriteState((unsigned char *)&lastNonZero.whiteBrightness,
                                 sizeof(lastNonZero.whiteBrightness));
      break;
    }
  }
}

void LightingPwmBase::onLoadState() {
  switch (legacyChannelFunction) {
    case LegacyChannelFunction::None: {
      Supla::Storage::ReadState((unsigned char *)&requested.red,
                                sizeof(requested.red));
      Supla::Storage::ReadState((unsigned char *)&requested.green,
                                sizeof(requested.green));
      Supla::Storage::ReadState((unsigned char *)&requested.blue,
                                sizeof(requested.blue));
      Supla::Storage::ReadState((unsigned char *)&requested.colorBrightness,
                                sizeof(requested.colorBrightness));
      Supla::Storage::ReadState((unsigned char *)&requested.whiteBrightness,
                                sizeof(requested.whiteBrightness));
      Supla::Storage::ReadState((unsigned char *)&lastNonZero.colorBrightness,
                                sizeof(lastNonZero.colorBrightness));
      Supla::Storage::ReadState((unsigned char *)&lastNonZero.whiteBrightness,
                                sizeof(lastNonZero.whiteBrightness));
      Supla::Storage::ReadState((unsigned char *)&requested.whiteTemperature,
                                sizeof(requested.whiteTemperature));
      break;
    }
    case LegacyChannelFunction::RGBW: {
      Supla::Storage::ReadState((unsigned char *)&requested.red,
                                sizeof(requested.red));
      Supla::Storage::ReadState((unsigned char *)&requested.green,
                                sizeof(requested.green));
      Supla::Storage::ReadState((unsigned char *)&requested.blue,
                                sizeof(requested.blue));
      Supla::Storage::ReadState((unsigned char *)&requested.colorBrightness,
                                sizeof(requested.colorBrightness));
      Supla::Storage::ReadState((unsigned char *)&requested.whiteBrightness,
                                sizeof(requested.whiteBrightness));
      Supla::Storage::ReadState((unsigned char *)&lastNonZero.colorBrightness,
                                sizeof(lastNonZero.colorBrightness));
      Supla::Storage::ReadState((unsigned char *)&lastNonZero.whiteBrightness,
                                sizeof(lastNonZero.whiteBrightness));
      break;
    }
    case LegacyChannelFunction::RGB: {
      Supla::Storage::ReadState((unsigned char *)&requested.red,
                                sizeof(requested.red));
      Supla::Storage::ReadState((unsigned char *)&requested.green,
                                sizeof(requested.green));
      Supla::Storage::ReadState((unsigned char *)&requested.blue,
                                sizeof(requested.blue));
      Supla::Storage::ReadState((unsigned char *)&requested.colorBrightness,
                                sizeof(requested.colorBrightness));
      Supla::Storage::ReadState((unsigned char *)&lastNonZero.colorBrightness,
                                sizeof(lastNonZero.colorBrightness));
      break;
    }
    case LegacyChannelFunction::Dimmer: {
      Supla::Storage::ReadState((unsigned char *)&requested.whiteBrightness,
                                sizeof(requested.whiteBrightness));
      Supla::Storage::ReadState((unsigned char *)&lastNonZero.whiteBrightness,
                                sizeof(lastNonZero.whiteBrightness));
      break;
    }
  }
  SUPLA_LOG_DEBUG(
      "Light[%d] loaded state: r=%d, g=%d, b=%d, "
      "colorBrigh=%d, whiteBrigh=%d, whiteTemp=%d",
      getChannel()->getChannelNumber(),
      requested.red,
      requested.green,
      requested.blue,
      lastNonZero.colorBrightness,
      lastNonZero.whiteBrightness,
      requested.whiteTemperature);
}

LightingPwmBase &LightingPwmBase::setDefaultStateOn() {
  stateOnInit = RGBW_STATE_ON_INIT_ON;
  return *this;
}

LightingPwmBase &LightingPwmBase::setDefaultStateOff() {
  stateOnInit = RGBW_STATE_ON_INIT_OFF;
  return *this;
}

LightingPwmBase &LightingPwmBase::setDefaultStateRestore() {
  stateOnInit = RGBW_STATE_ON_INIT_RESTORE;
  return *this;
}

void LightingPwmBase::setMinIterationBrightness(uint8_t minBright) {
  minIterationBrightness = minBright;
}

void LightingPwmBase::setMinMaxIterationDelay(uint16_t delayMs) {
  minMaxIterationDelay = delayMs;
}

LightingPwmBase &LightingPwmBase::setBrightnessRatioLimits(float min,
                                                           float max) {
  minBrightnessRatio = min;
  maxBrightnessRatio = max;
  normalizeLimitPair(minBrightnessRatio, maxBrightnessRatio);
  SUPLA_LOG_DEBUG("Light[%d] set brightness limits: min=%.3f, max=%.3f",
                  getChannel()->getChannelNumber(),
                  static_cast<double>(minBrightnessRatio),
                  static_cast<double>(maxBrightnessRatio));
  return *this;
}
LightingPwmBase &LightingPwmBase::setColorBrightnessRatioLimits(float min,
                                                                float max) {
  minColorBrightnessRatio = min;
  maxColorBrightnessRatio = max;
  normalizeLimitPair(minColorBrightnessRatio, maxColorBrightnessRatio);
  SUPLA_LOG_DEBUG("Light[%d] set color brightness limits: min=%.3f, max=%.3f",
                  getChannel()->getChannelNumber(),
                  static_cast<double>(minColorBrightnessRatio),
                  static_cast<double>(maxColorBrightnessRatio));
  return *this;
}

void LightingPwmBase::attach(Supla::Control::Button *button) {
  attachedButton = button;
}

void LightingPwmBase::onLoadConfig(SuplaDeviceClass *sdc) {
  (void)(sdc);
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    loadFunctionFromConfig();
    loadConfigChangeFlag();

    char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    Supla::Config::generateKey(
        key, getChannel()->getChannelNumber(), Supla::ConfigTag::RgbwButtonTag);
    int32_t rgbwButtonControlType = 0;
    // try to read RGBW button control type from channel specific parameter
    // and if it is missing, read global value setting
    if (!cfg->getInt32(key, &rgbwButtonControlType)) {
      cfg->getInt32(Supla::ConfigTag::RgbwButtonTag, &rgbwButtonControlType);
    }
    if (rgbwButtonControlType >= 0 && rgbwButtonControlType <= 4) {
      buttonControlType = static_cast<ButtonControlType>(rgbwButtonControlType);
    }

    if (!skipLegacyMigration &&
        legacyChannelFunction != LegacyChannelFunction::None) {
      Supla::Config::generateKey(key,
                                 getChannel()->getChannelNumber(),
                                 Supla::ConfigTag::LegacyMigrationTag);
      uint8_t migrationDoneFlag = 0;
      // try to read migration done flag
      // and if it is missing, check if minimal config is NOT ready (so we are
      // in factory defaults state)
      // if so, mark migration as done / not needed
      if (!cfg->getUInt8(key, &migrationDoneFlag)) {
        if (!cfg->isMinimalConfigReady(false)) {
          cfg->setUInt8(key, 1);
          migrationDoneFlag = 1;
        }
      }
      if (migrationDoneFlag == 1) {
        legacyChannelFunction = LegacyChannelFunction::None;
      }
    }

    // load PWM frequency from config
    uint32_t cfgFrequency = pwmFrequency;
    if (cfg->getUInt32(Supla::ConfigTag::PwmFrequencyTag, &cfgFrequency)) {
      SUPLA_LOG_INFO("Light[%d] PWM frequency loaded from config: %d",
                     getChannel()->getChannelNumber(),
                     cfgFrequency);
    }
    if (cfgFrequency > UINT16_MAX) {
      cfgFrequency = UINT16_MAX;
    }
    setPwmFrequency(cfgFrequency);
  }
  SUPLA_LOG_DEBUG(
      "Light[%d] button control type: %d, legacy migration needed: %d",
      getChannel()->getChannelNumber(),
      buttonControlType,
      !skipLegacyMigration &&
          legacyChannelFunction != LegacyChannelFunction::None);
}

void LightingPwmBase::fillSuplaChannelNewValue(
    TSD_SuplaChannelNewValue *value) {
  if (value == nullptr) {
    return;
  }

  value->value[0] = requested.whiteBrightness;
  value->value[1] = requested.colorBrightness;
  value->value[2] = requested.blue;
  value->value[3] = requested.green;
  value->value[4] = requested.red;
  value->value[7] = requested.whiteTemperature;
  SUPLA_LOG_DEBUG("Light[%d] fill: %d,%d,%d,%d,%d",
                  getChannelNumber(),
                  requested.red,
                  requested.green,
                  requested.blue,
                  requested.colorBrightness,
                  requested.whiteBrightness);
}

int LightingPwmBase::getCurrentDimmerBrightness() const {
  return requested.whiteBrightness;
}

int LightingPwmBase::getCurrentRGBBrightness() const {
  return requested.colorBrightness;
}

void LightingPwmBase::setMaxHwValue(int newMaxHwValue) {
  if (newMaxHwValue < 1) {
    newMaxHwValue = 1;
  } else if (newMaxHwValue > UINT16_MAX) {
    newMaxHwValue = UINT16_MAX;
  }
  maxHwValue = newMaxHwValue;
  if (brightnessAdjuster) {
    brightnessAdjuster->setMaxHwValue(newMaxHwValue);
  }
}

void LightingPwmBase::purgeConfig() {
  Supla::ChannelElement::purgeConfig();
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    generateKey(key, Supla::ConfigTag::RgbwButtonTag);
    cfg->eraseKey(key);
  }
}

ApplyConfigResult LightingPwmBase::applyChannelConfig(TSD_ChannelConfig *,
                                                      bool) {
  SUPLA_LOG_WARNING("Light[%d] applyChannelConfig missing", getChannelNumber());
  return ApplyConfigResult::Success;
}

void LightingPwmBase::fillChannelConfig(void *, int *size, uint8_t) {
  SUPLA_LOG_DEBUG("Light[%d] fillChannelConfig missing", getChannelNumber());
  if (size) {
    *size = 0;
  }
}

void LightingPwmBase::convertStorageFromLegacyChannel(
    LegacyChannelFunction channelFunction) {
  legacyChannelFunction = channelFunction;
}

int LightingPwmBase::getMissingGpioCount() const {
  if (hasParent()) {
    auto missingGpioCount = parent->getMissingGpioCount();
    if (missingGpioCount > 0) {
      return missingGpioCount - 1;
    }
  }
  switch (getChannel()->getDefaultFunction()) {
    case SUPLA_CHANNELFNC_DIMMER: {
      return 0;
    }
    case SUPLA_CHANNELFNC_DIMMER_CCT: {
      return 1;
    }
    case SUPLA_CHANNELFNC_RGBLIGHTING: {
      return 2;
    }
    case SUPLA_CHANNELFNC_DIMMERANDRGBLIGHTING: {
      return 3;
    }
    case SUPLA_CHANNELFNC_DIMMER_CCT_AND_RGB: {
      return 4;
    }
  }
  return 0;
}

void LightingPwmBase::enableChannel() {
  if (enabled) {
    return;
  }

  timing.lastTick = 0;
  enabled = true;
  getChannel()->setStateOnline();
}

void LightingPwmBase::disableChannel() {
  if (!enabled) {
    return;
  }

  uint32_t valueAdj[SUPLA_MAX_OUTPUT_COUNT] = {0};
  setRGBCCTValueOnDevice(valueAdj, usedChannels);
  hardware.red = 0;
  hardware.green = 0;
  hardware.blue = 0;
  hardware.colorBrightness = 0;
  hardware.brightness = 0;
  hardware.whiteTemperature = 0;
  usedChannels = 0;

  enabled = false;
  getChannel()->setStateOnlineAndNotAvailable();
}

bool LightingPwmBase::hasParent() const {
  return parent != nullptr;
}

int LightingPwmBase::getAncestorCount() const {
  if (hasParent()) {
    return parent->getAncestorCount() + 1;
  }
  return 0;
}

bool LightingPwmBase::isStateStorageMigrationNeeded() const {
  return !skipLegacyMigration &&
         legacyChannelFunction != LegacyChannelFunction::None;
}

void LightingPwmBase::setSkipLegacyMigration() {
  skipLegacyMigration = true;
}

void LightingPwmBase::setMinPwmFrequency(uint16_t minPwmFrequency) {
  this->minPwmFrequency = minPwmFrequency;
}

void LightingPwmBase::setMaxPwmFrequency(uint16_t maxPwmFrequency) {
  this->maxPwmFrequency = maxPwmFrequency;
}

void LightingPwmBase::setStepPwmFrequency(uint16_t stepPwmFrequency) {
  this->stepPwmFrequency = stepPwmFrequency;
}

void LightingPwmBase::setPwmResolutionBits(uint8_t resolutionBits) {
  pwmResolutionBits = resolutionBits;

  uint32_t hwMax = 0;
  if (resolutionBits > 0) {
    hwMax = (1UL << resolutionBits) - 1;
  }
  if (hwMax > UINT16_MAX) {
    hwMax = UINT16_MAX;
  }

  setMaxHwValue(static_cast<int>(hwMax));
}

void LightingPwmBase::setPwmFrequency(uint16_t frequency) {
  if (frequency < minPwmFrequency) {
    frequency = minPwmFrequency;
  } else if (frequency > maxPwmFrequency) {
    frequency = maxPwmFrequency;
  }

  if ((frequency - minPwmFrequency) % stepPwmFrequency != 0) {
    frequency =
        minPwmFrequency +
        ((frequency - minPwmFrequency) / stepPwmFrequency) * stepPwmFrequency;
  }

  pwmFrequency = frequency;
  SUPLA_LOG_INFO(
      "Light[%d] PWM frequency set to %d", getChannelNumber(), pwmFrequency);
}

uint16_t LightingPwmBase::getMinPwmFrequency() const {
  return minPwmFrequency;
}

uint16_t LightingPwmBase::getMaxPwmFrequency() const {
  return maxPwmFrequency;
}

uint16_t LightingPwmBase::getPwmFrequency() const {
  return pwmFrequency;
}

uint16_t LightingPwmBase::getStepPwmFrequency() const {
  return stepPwmFrequency;
}

uint8_t LightingPwmBase::getPwmResolutionBits() const {
  return pwmResolutionBits;
}

};  // namespace Control
};  // namespace Supla

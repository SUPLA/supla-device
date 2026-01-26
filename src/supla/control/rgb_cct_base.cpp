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

#include "rgb_cct_base.h"

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

namespace Supla {
namespace Control {

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

RGBCCTBase::RGBCCTBase(RGBCCTBase *parent) : parent(parent) {
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

void RGBCCTBase::setBrightnessAdjuster(BrightnessAdjuster *adjuster) {
  if (brightnessAdjuster) {
    delete brightnessAdjuster;
  }
  brightnessAdjuster = adjuster;
  brightnessAdjuster->setMaxHwValue(maxHwValue);
}

void RGBCCTBase::setRGBW(int red,
                         int green,
                         int blue,
                         int colorBrightness,
                         int whiteBrightness,
                         bool toggle,
                         bool instant) {
  setRGBCCT(
      red, green, blue, colorBrightness, whiteBrightness, -1, toggle, instant);
}

void RGBCCTBase::setRGBCCT(int red,
                           int green,
                           int blue,
                           int colorBrightness,
                           int whiteBrightness,
                           int whiteTemperature,
                           bool toggle,
                           bool instant) {
  if (toggle) {
    lastMsgReceivedMs = 1;
  } else {
    lastMsgReceivedMs = millis();
  }

  // Store last non 0 brightness for turn on/toggle operations
  if (toggle && colorBrightness == 100) {
    colorBrightness = lastColorBrightness;
  } else if (colorBrightness > 0) {
    lastColorBrightness = colorBrightness;
  }
  if (toggle && whiteBrightness == 100) {
    whiteBrightness = lastWhiteBrightness;
  } else if (whiteBrightness > 0) {
    lastWhiteBrightness = whiteBrightness;
  }

  // Store current values
  if (red >= 0) {
    curRed = red;
  }
  if (green >= 0) {
    curGreen = green;
  }
  if (blue >= 0) {
    curBlue = blue;
  }
  if (colorBrightness >= 0) {
    curColorBrightness = colorBrightness;
  }
  if (whiteBrightness >= 0) {
    curWhiteBrightness = whiteBrightness;
  }
  if (whiteTemperature >= 0) {
    curWhiteTemperature = whiteTemperature;
  }

  this->instant = instant;
  resetDisance = true;

  SUPLA_LOG_DEBUG("RGBCCT[%d]: %d,%d,%d,%d,%d,%d",
                  getChannelNumber(),
                  curRed,
                  curGreen,
                  curBlue,
                  curColorBrightness,
                  curWhiteBrightness,
                  curWhiteTemperature);

  // Schedule save in 5 s after state change
  Supla::Storage::ScheduleSave(5000, 2000);
}

void RGBCCTBase::iterateAlways() {
  if (lastMsgReceivedMs != 0 && millis() - lastMsgReceivedMs >= 400) {
    lastMsgReceivedMs = 0;
    // Send to Supla server new values
    channel.setNewValue(curRed,
                        curGreen,
                        curBlue,
                        curColorBrightness,
                        curWhiteBrightness,
                        curWhiteTemperature);
  }
  if (hasParent() && parent->getMissingGpioCount() > 0) {
    disableChannel();
  } else {
    enableChannel();
  }
}

int32_t RGBCCTBase::handleNewValueFromServer(
    TSD_SuplaChannelNewValue *newValue) {
  uint8_t whiteTemperature = static_cast<uint8_t>(newValue->value[7]);
  uint8_t command = static_cast<uint8_t>(newValue->value[6]);
  uint8_t toggleOnOff = static_cast<uint8_t>(newValue->value[5]);
  uint8_t red = static_cast<uint8_t>(newValue->value[4]);
  uint8_t green = static_cast<uint8_t>(newValue->value[3]);
  uint8_t blue = static_cast<uint8_t>(newValue->value[2]);
  uint8_t colorBrightness = static_cast<uint8_t>(newValue->value[1]);
  uint8_t whiteBrightness = static_cast<uint8_t>(newValue->value[0]);

  SUPLA_LOG_DEBUG(
      "RGBCCT[%d]: red=%d, green=%d, blue=%d, colorBrightness=%d, "
      "whiteBrightness=%d, whiteTemperature=%d, command=%d, toggleOnOff=%d",
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
      setRGBCCT(-1, -1, -1, -1, lastWhiteBrightness, -1);
      break;
    }
    case RGBW_COMMAND_TURN_OFF_DIMMER: {
      setRGBCCT(-1, -1, -1, -1, 0, -1);
      break;
    }
    case RGBW_COMMAND_TOGGLE_DIMMER: {
      setRGBCCT(
          -1, -1, -1, -1, curWhiteBrightness > 0 ? 0 : lastWhiteBrightness, -1);
      break;
    }
    case RGBW_COMMAND_TURN_ON_RGB: {
      setRGBCCT(-1, -1, -1, lastColorBrightness, -1, -1);
      break;
    }
    case RGBW_COMMAND_TURN_OFF_RGB: {
      setRGBCCT(-1, -1, -1, 0, -1, -1);
      break;
    }
    case RGBW_COMMAND_TOGGLE_RGB: {
      setRGBCCT(
          -1, -1, -1, curColorBrightness > 0 ? 0 : lastColorBrightness, -1, -1);
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
      if (curWhiteBrightness > 0) {
        setRGBCCT(-1, -1, -1, -1, whiteBrightness, -1);
      } else {
        lastWhiteBrightness = whiteBrightness;
      }
      break;
    }
    case RGBW_COMMAND_SET_COLOR_BRIGHTNESS_WITHOUT_TURN_ON: {
      if (curColorBrightness > 0) {
        setRGBCCT(-1, -1, -1, colorBrightness, -1, -1);
      } else {
        lastColorBrightness = colorBrightness;
      }
      break;
    }
    case RGBW_COMMAND_SET_RGB_WITHOUT_TURN_ON: {
      setRGBCCT(red, green, blue, -1, -1, -1);
      break;
    }
    case RGBW_COMMAND_START_ITERATE_DIMMER: {
      lastAutoIterateStartTimestamp = millis();
      if (autoIterateMode == AutoIterateMode::OFF) {
        autoIterateMode = AutoIterateMode::DIMMER;
      } else {
        autoIterateMode = AutoIterateMode::ALL;
      }
      break;
    }
    case RGBW_COMMAND_START_ITERATE_RGB: {
      lastAutoIterateStartTimestamp = millis();
      if (autoIterateMode == AutoIterateMode::OFF) {
        autoIterateMode = AutoIterateMode::RGB;
      } else {
        autoIterateMode = AutoIterateMode::ALL;
      }
      break;
    }
    case RGBW_COMMAND_START_ITERATE_ALL: {
      autoIterateMode = AutoIterateMode::ALL;
      lastAutoIterateStartTimestamp = millis();
      break;
    }
    case RGBW_COMMAND_STOP_ITERATE_DIMMER: {
      if (autoIterateMode == AutoIterateMode::DIMMER) {
        autoIterateMode = AutoIterateMode::OFF;
      } else if (autoIterateMode == AutoIterateMode::ALL) {
        autoIterateMode = AutoIterateMode::RGB;
      }
      break;
    }
    case RGBW_COMMAND_STOP_ITERATE_RGB: {
      if (autoIterateMode == AutoIterateMode::RGB) {
        autoIterateMode = AutoIterateMode::OFF;
      } else if (autoIterateMode == AutoIterateMode::ALL) {
        autoIterateMode = AutoIterateMode::DIMMER;
      }
      break;
    }
    case RGBW_COMMAND_STOP_ITERATE_ALL: {
      autoIterateMode = AutoIterateMode::OFF;
      break;
    }
    case RGBW_COMMAND_SET_DIMMER_CCT_WITHOUT_TURN_ON: {
      setRGBCCT(-1, -1, -1, -1, -1, whiteTemperature);
      break;
    }
    default: {
      break;
    }
  }
  return -1;
}

void RGBCCTBase::turnOn() {
  setRGBCCT(-1, -1, -1, lastColorBrightness, lastWhiteBrightness, -1);
}
void RGBCCTBase::turnOff() {
  setRGBCCT(-1, -1, -1, 0, 0, -1);
}

void RGBCCTBase::toggle() {
  if (isOn()) {
    turnOff();
  } else {
    turnOn();
  }
}

bool RGBCCTBase::isOn() {
  return isOnRGB() || isOnW();
}

bool RGBCCTBase::isOnW() {
  return curWhiteBrightness > 0;
}

bool RGBCCTBase::isOnRGB() {
  return curColorBrightness > 0;
}

uint8_t RGBCCTBase::addWithLimit(int value, int addition, int limit) {
  if (addition > 0 && value + addition > limit) {
    return limit;
  }
  if (addition < 0 && value + addition < 0) {
    return 0;
  }
  return value + addition;
}

void RGBCCTBase::handleAction(int event, int action) {
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
                addWithLimit(curColorBrightness, buttonStep, 100),
                addWithLimit(curWhiteBrightness, buttonStep, 100),
                -1);
      break;
    }
    case DIM_ALL: {
      setRGBCCT(-1,
                -1,
                -1,
                addWithLimit(curColorBrightness, -buttonStep, 100),
                addWithLimit(curWhiteBrightness, -buttonStep, 100),
                -1);
      break;
    }
    case BRIGHTEN_R: {
      setRGBCCT(addWithLimit(curRed, buttonStep), -1, -1, -1, -1, -1);
      break;
    }
    case DIM_R: {
      setRGBCCT(addWithLimit(curRed, -buttonStep), -1, -1, -1, -1, -1);
      break;
    }
    case BRIGHTEN_G: {
      setRGBCCT(-1, addWithLimit(curGreen, buttonStep), -1, -1, -1, -1);
      break;
    }
    case DIM_G: {
      setRGBCCT(-1, addWithLimit(curGreen, -buttonStep), -1, -1, -1, -1);
      break;
    }
    case BRIGHTEN_B: {
      setRGBCCT(-1, -1, addWithLimit(curBlue, buttonStep), -1, -1, -1);
      break;
    }
    case DIM_B: {
      setRGBCCT(-1, -1, addWithLimit(curBlue, -buttonStep), -1, -1, -1);
      break;
    }
    case BRIGHTEN_W: {
      setRGBCCT(-1,
                -1,
                -1,
                -1,
                addWithLimit(curWhiteBrightness, buttonStep, 100),
                -1);
      break;
    }
    case DIM_W: {
      setRGBCCT(-1,
                -1,
                -1,
                -1,
                addWithLimit(curWhiteBrightness, -buttonStep, 100),
                -1);
      break;
    }
    case BRIGHTEN_RGB: {
      setRGBCCT(-1,
                -1,
                -1,
                addWithLimit(curColorBrightness, buttonStep, 100),
                -1,
                -1);
      break;
    }
    case DIM_RGB: {
      setRGBCCT(-1,
                -1,
                -1,
                addWithLimit(curColorBrightness, -buttonStep, 100),
                -1,
                -1);
      break;
    }
    case TURN_ON_RGB: {
      setRGBCCT(-1, -1, -1, lastColorBrightness, -1, -1);
      break;
    }
    case TURN_OFF_RGB: {
      setRGBCCT(-1, -1, -1, 0, -1, -1);
      break;
    }
    case TOGGLE_RGB: {
      setRGBCCT(
          -1, -1, -1, curColorBrightness > 0 ? 0 : lastColorBrightness, -1, -1);
      break;
    }
    case TURN_ON_W: {
      setRGBCCT(-1, -1, -1, -1, lastWhiteBrightness, -1);
      break;
    }
    case TURN_OFF_W: {
      setRGBCCT(-1, -1, -1, -1, 0, -1);
      break;
    }
    case TOGGLE_W: {
      setRGBCCT(
          -1, -1, -1, -1, curWhiteBrightness > 0 ? 0 : lastWhiteBrightness, -1);
      break;
    }
    case TURN_ON_RGB_DIMMED: {
      if (curColorBrightness == 0) {
        setRGBCCT(-1, -1, -1, defaultDimmedBrightness, -1, -1);
      }
      break;
    }
    case TURN_ON_W_DIMMED: {
      if (curWhiteBrightness == 0) {
        setRGBCCT(-1, -1, -1, -1, defaultDimmedBrightness, -1);
      }
      break;
    }
    case TURN_ON_ALL_DIMMED: {
      if (curWhiteBrightness == 0 && curColorBrightness == 0) {
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

void RGBCCTBase::iterateDimmerRGBW(int rgbStep, int wStep) {
  // if we iterate both RGB and W, then we should sync brightness
  if (rgbStep > 0 && wStep > 0) {
    curWhiteBrightness = curColorBrightness;
  }

  // change iteration direction if there was no action in last 0.5 s
  if (millis() - lastIterateDimmerTimestamp >= 500) {
    dimIterationDirection = !dimIterationDirection;
    iterationDelayTimestamp = 0;
    if (curWhiteBrightness <= 5) {
      dimIterationDirection = false;
    } else if (curWhiteBrightness >= 95) {
      dimIterationDirection = true;
    }
    if (millis() - lastIterateDimmerTimestamp >= 10000) {
      if (curWhiteBrightness <= 40) {
        dimIterationDirection = false;
      } else if (curWhiteBrightness >= 60) {
        dimIterationDirection = true;
      }
    }
  }

  lastIterateDimmerTimestamp = millis();

  if (rgbStep > 0) {
    if (curColorBrightness <= minIterationBrightness &&
        dimIterationDirection == true) {
      if (iterationDelayTimestamp == 0) {
        iterationDelayTimestamp = millis();
      }
      if (millis() - iterationDelayTimestamp > minMaxIterationDelay) {
        dimIterationDirection = false;
        iterationDelayTimestamp = 0;
      } else {
        return;
      }
    } else if (curColorBrightness == 100 && dimIterationDirection == false) {
      if (iterationDelayTimestamp == 0) {
        iterationDelayTimestamp = millis();
      }
      if (millis() - iterationDelayTimestamp > minMaxIterationDelay) {
        dimIterationDirection = true;
        iterationDelayTimestamp = 0;
      } else {
        return;
      }
    }
  } else if (wStep > 0) {
    if (curWhiteBrightness <= minIterationBrightness &&
        dimIterationDirection == true) {
      if (iterationDelayTimestamp == 0) {
        iterationDelayTimestamp = millis();
      }
      if (millis() - iterationDelayTimestamp > minMaxIterationDelay) {
        dimIterationDirection = false;
        iterationDelayTimestamp = 0;
      } else {
        return;
      }
    } else if (curWhiteBrightness == 100 && dimIterationDirection == false) {
      if (iterationDelayTimestamp == 0) {
        iterationDelayTimestamp = millis();
      }
      if (millis() - iterationDelayTimestamp > minMaxIterationDelay) {
        dimIterationDirection = true;
        iterationDelayTimestamp = 0;
      } else {
        return;
      }
    }
  }
  iterationDelayTimestamp = 0;

  // If direction is dim, then brightness step is set to negative
  if (dimIterationDirection) {
    rgbStep = -rgbStep;
    wStep = -wStep;
  }

  if (rgbStep && curColorBrightness + rgbStep < minIterationBrightness) {
    rgbStep = minIterationBrightness - curColorBrightness;
  }
  if (wStep && curWhiteBrightness + wStep < minIterationBrightness) {
    wStep = minIterationBrightness - curWhiteBrightness;
  }

  if ((wStep != 0 && curWhiteBrightness == 0) ||
      (rgbStep != 0 && curColorBrightness == 0)) {
    iterationDelayTimestamp = millis();
    dimIterationDirection = true;
  }

  setRGBCCT(-1,
            -1,
            -1,
            addWithLimit(curColorBrightness, rgbStep, 100),
            addWithLimit(curWhiteBrightness, wStep, 100),
            -1,
            false,
            true);
}

void RGBCCTBase::setStep(int step) {
  buttonStep = step;
}

void RGBCCTBase::setDefaultDimmedBrightness(int dimmedBrightness) {
  defaultDimmedBrightness = dimmedBrightness;
}

void RGBCCTBase::setFadeEffectTime(int timeMs) {
  fadeEffect = timeMs;
}

int RGBCCTBase::adjustBrightness(int value) {
  if (brightnessAdjuster) {
    return brightnessAdjuster->adjustBrightness(value);
  }
  return adjustRange(value, 0, 100, 0, maxHwValue);
}

int RGBCCTBase::getStep(int step, int target, int current, int distance) const {
  (void)(distance);
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

bool RGBCCTBase::calculateAndUpdate(int targetValue,
                                    uint16_t *hwValue,
                                    int distance,
                                    uint32_t *lastChangeMs) const {
  if (targetValue != *hwValue) {
    uint32_t timeDiff = millis() - *lastChangeMs;
    if (timeDiff == 0) {
      return false;
    }

    int currentFadeEffectTime = fadeEffect;
    if (distance < maxHwValue / 10) {
      currentFadeEffectTime = fadeEffect / 3;
    }

    double divider = 1.0 * currentFadeEffectTime / timeDiff;
    if (divider <= 1) {
      divider = 1;
    }

    int step = distance / divider;
    if (step < 1) {
      return false;
    }

    int valueStep = getStep(step, targetValue, *hwValue, distance);
    if (valueStep != 0) {
      *hwValue += valueStep;
      *lastChangeMs = millis();
      return true;
    }
  } else {
    *lastChangeMs = millis();
  }
  return false;
}

void RGBCCTBase::onFastTimer() {
  if (!enabled) {
    return;
  }
  if (lastTick == 0) {
    lastTick = millis();
    lastChangeRedMs = millis();
    lastChangeGreenMs = millis();
    lastChangeBlueMs = millis();
    lastChangeBrightnessMs = millis();
    lastChangeWhiteTemperatureMs = millis();
    lastChangeColorBrightnessMs = millis();
    return;
  }
  uint32_t timeDiff = millis() - lastTick;

  if (autoIterateMode != AutoIterateMode::OFF &&
      millis() - lastAutoIterateStartTimestamp < 10000) {
    if (millis() - lastIterateDimmerTimestamp >= 35) {
      // lastIterateDimmerTimestamp is updated in handleAction calls below
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

  if (timeDiff > 0) {
    lastTick = millis();

    // target values are in 0..maxHwValue range
    int targetRed = adjustRange(curRed, 0, 255, 0, maxHwValue);
    int targetGreen = adjustRange(curGreen, 0, 255, 0, maxHwValue);
    int targetBlue = adjustRange(curBlue, 0, 255, 0, maxHwValue);
    int targetColorBrightness = adjustBrightness(curColorBrightness);
    int targetBrightness = adjustBrightness(curWhiteBrightness);
    int targetWhiteTemperature =
        adjustRange(curWhiteTemperature, 0, 100, 0, maxHwValue);

    if (resetDisance) {
      resetDisance = false;

      redDistance = abs(targetRed - hwRed);
      greenDistance = abs(targetGreen - hwGreen);
      blueDistance = abs(targetBlue - hwBlue);
      colorBrightnessDistance = abs(targetColorBrightness - hwColorBrightness);
      brightnessDistance = abs(targetBrightness - hwBrightness);
      whiteTemperatureDistance =
          abs(targetWhiteTemperature - hwWhiteTemperature);
    }

    if (instant) {
      hwRed = targetRed;
      hwGreen = targetGreen;
      hwBlue = targetBlue;
      hwColorBrightness = targetColorBrightness;
      hwBrightness = targetBrightness;
      valueChanged = true;
      instant = false;
    } else {
      if (calculateAndUpdate(
              targetRed, &hwRed, redDistance, &lastChangeRedMs)) {
        valueChanged = true;
      }
      if (calculateAndUpdate(
              targetGreen, &hwGreen, greenDistance, &lastChangeGreenMs)) {
        valueChanged = true;
      }
      if (calculateAndUpdate(
              targetBlue, &hwBlue, blueDistance, &lastChangeBlueMs)) {
        valueChanged = true;
      }
      if (calculateAndUpdate(targetColorBrightness,
                             &hwColorBrightness,
                             colorBrightnessDistance,
                             &lastChangeColorBrightnessMs)) {
        valueChanged = true;
      }
      if (calculateAndUpdate(targetBrightness,
                             &hwBrightness,
                             brightnessDistance,
                             &lastChangeBrightnessMs)) {
        valueChanged = true;
      }
      if (calculateAndUpdate(targetWhiteTemperature,
                             &hwWhiteTemperature,
                             whiteTemperatureDistance,
                             &lastChangeWhiteTemperatureMs)) {
        valueChanged = true;
      }
    }

    if (valueChanged) {
      // RGB Color brightness
      uint32_t adjColorBrightness = hwColorBrightness;
      if (hwColorBrightness > 0) {
        adjColorBrightness = adjustRange(adjColorBrightness,
                                         1,
                                         maxHwValue,
                                         minColorBrightness,
                                         maxColorBrightness);
      } else {
        hwColorBrightness = 0;
      }

      // White channel(s) brightness
      uint32_t adjBrightness = hwBrightness;
      if (hwBrightness > 0) {
        adjBrightness = adjustRange(
            adjBrightness, 1, maxHwValue, minBrightness, maxBrightness);
      } else {
        hwBrightness = 0;
      }

      uint32_t white1Brightness = adjBrightness;
      uint32_t white2Brightness = 0;

      if (hwWhiteTemperature > 0) {
        float white1Fraction = 1.0 * hwWhiteTemperature / maxHwValue;
        white1Brightness = adjBrightness * white1Fraction * warmWhiteGain;
        white2Brightness =
            adjBrightness * (1.0 - white1Fraction) * coldWhiteGain;
        if (white1Brightness > maxHwValue) {
          white1Brightness = maxHwValue;
        }
        if (white2Brightness > maxHwValue) {
          white2Brightness = maxHwValue;
        }
        // TODO(klew): add warm/cold channel swap
      }

      setRGBCCTValueOnDevice(hwRed,
                             hwGreen,
                             hwBlue,
                             adjColorBrightness,
                             white1Brightness,
                             white2Brightness);
      valueChanged = false;
    }
  }
}

void RGBCCTBase::onInit() {
  if (attachedButton) {
    SUPLA_LOG_DEBUG("RGBCCT[%d] configuring attachedButton, control type %d",
                    getChannel()->getChannelNumber(),
                    buttonControlType);
    if (attachedButton->isMonostable()) {
      SUPLA_LOG_DEBUG("RGBCCT[%d] configuring monostable button",
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
      SUPLA_LOG_DEBUG("RGBCCT[%d] configuring bistable button",
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
      SUPLA_LOG_DEBUG("RGBCCT[%d] configuring motion sensor/central button",
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
        SUPLA_LOG_DEBUG("RGBCCT[%d] button pressed",
                        getChannel()->getChannelNumber());
        switch (buttonControlType) {
          case BUTTON_FOR_RGBW: {
            curColorBrightness = lastColorBrightness;
            curWhiteBrightness = lastWhiteBrightness;
            break;
          }
          case BUTTON_FOR_RGB: {
            curColorBrightness = lastColorBrightness;
            break;
          }
          case BUTTON_FOR_W: {
            curWhiteBrightness = lastWhiteBrightness;
            break;
          }
          case BUTTON_NOT_USED: {
            break;
          }
        }
      } else {
        SUPLA_LOG_DEBUG("RGBCCT[%d] button not pressed",
                        getChannel()->getChannelNumber());
        switch (buttonControlType) {
          case BUTTON_FOR_RGBW: {
            curColorBrightness = 0;
            curWhiteBrightness = 0;
            break;
          }
          case BUTTON_FOR_RGB: {
            curColorBrightness = 0;
            break;
          }
          case BUTTON_FOR_W: {
            curWhiteBrightness = 0;
            break;
          }
          case BUTTON_NOT_USED: {
            break;
          }
        }
      }
    } else {
      SUPLA_LOG_WARNING("RGBCCT[%d] unknown button type",
                        getChannel()->getChannelNumber());
    }
  }

  bool toggle = false;
  if (stateOnInit == RGBW_STATE_ON_INIT_ON) {
    SUPLA_LOG_DEBUG("RGBCCT[%d] TURN on onInit",
                    getChannel()->getChannelNumber());
    curColorBrightness = 100;
    curWhiteBrightness = 100;
    toggle = true;
  } else if (stateOnInit == RGBW_STATE_ON_INIT_OFF) {
    SUPLA_LOG_DEBUG("RGBCCT[%d] TURN off onInit",
                    getChannel()->getChannelNumber());
    curColorBrightness = 0;
    curWhiteBrightness = 0;
  }

  initDone = true;

  setRGBCCT(curRed,
            curGreen,
            curBlue,
            curColorBrightness,
            curWhiteBrightness,
            curWhiteTemperature,
            toggle);
}

void RGBCCTBase::onSaveState() {
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
      Supla::Storage::WriteState((unsigned char *)&curRed, sizeof(curRed));
      Supla::Storage::WriteState((unsigned char *)&curGreen, sizeof(curGreen));
      Supla::Storage::WriteState((unsigned char *)&curBlue, sizeof(curBlue));
      Supla::Storage::WriteState((unsigned char *)&curColorBrightness,
                                 sizeof(curColorBrightness));
      Supla::Storage::WriteState((unsigned char *)&curWhiteBrightness,
                                 sizeof(curWhiteBrightness));
      Supla::Storage::WriteState((unsigned char *)&lastColorBrightness,
                                 sizeof(lastColorBrightness));
      Supla::Storage::WriteState((unsigned char *)&lastWhiteBrightness,
                                 sizeof(lastWhiteBrightness));
      Supla::Storage::WriteState((unsigned char *)&curWhiteTemperature,
                                 sizeof(curWhiteTemperature));
      break;
    }
    case LegacyChannelFunction::RGBW: {
      Supla::Storage::WriteState((unsigned char *)&curRed, sizeof(curRed));
      Supla::Storage::WriteState((unsigned char *)&curGreen, sizeof(curGreen));
      Supla::Storage::WriteState((unsigned char *)&curBlue, sizeof(curBlue));
      Supla::Storage::WriteState((unsigned char *)&curColorBrightness,
                                 sizeof(curColorBrightness));
      Supla::Storage::WriteState((unsigned char *)&curWhiteBrightness,
                                 sizeof(curWhiteBrightness));
      Supla::Storage::WriteState((unsigned char *)&lastColorBrightness,
                                 sizeof(lastColorBrightness));
      Supla::Storage::WriteState((unsigned char *)&lastWhiteBrightness,
                                 sizeof(lastWhiteBrightness));
      break;
    }
    case LegacyChannelFunction::RGB: {
      Supla::Storage::WriteState((unsigned char *)&curRed, sizeof(curRed));
      Supla::Storage::WriteState((unsigned char *)&curGreen, sizeof(curGreen));
      Supla::Storage::WriteState((unsigned char *)&curBlue, sizeof(curBlue));
      Supla::Storage::WriteState((unsigned char *)&curColorBrightness,
                                 sizeof(curColorBrightness));
      Supla::Storage::WriteState((unsigned char *)&lastColorBrightness,
                                 sizeof(lastColorBrightness));
      break;
    }
    case LegacyChannelFunction::Dimmer: {
      Supla::Storage::WriteState((unsigned char *)&curWhiteBrightness,
                                 sizeof(curWhiteBrightness));
      Supla::Storage::WriteState((unsigned char *)&lastWhiteBrightness,
                                 sizeof(lastWhiteBrightness));
      break;
    }
  }
}

void RGBCCTBase::onLoadState() {
  switch (legacyChannelFunction) {
    case LegacyChannelFunction::None: {
      Supla::Storage::ReadState((unsigned char *)&curRed, sizeof(curRed));
      Supla::Storage::ReadState((unsigned char *)&curGreen, sizeof(curGreen));
      Supla::Storage::ReadState((unsigned char *)&curBlue, sizeof(curBlue));
      Supla::Storage::ReadState((unsigned char *)&curColorBrightness,
                                sizeof(curColorBrightness));
      Supla::Storage::ReadState((unsigned char *)&curWhiteBrightness,
                                sizeof(curWhiteBrightness));
      Supla::Storage::ReadState((unsigned char *)&lastColorBrightness,
                                sizeof(lastColorBrightness));
      Supla::Storage::ReadState((unsigned char *)&lastWhiteBrightness,
                                sizeof(lastWhiteBrightness));
      Supla::Storage::ReadState((unsigned char *)&curWhiteTemperature,
                                sizeof(curWhiteTemperature));
      break;
    }
    case LegacyChannelFunction::RGBW: {
      Supla::Storage::ReadState((unsigned char *)&curRed, sizeof(curRed));
      Supla::Storage::ReadState((unsigned char *)&curGreen, sizeof(curGreen));
      Supla::Storage::ReadState((unsigned char *)&curBlue, sizeof(curBlue));
      Supla::Storage::ReadState((unsigned char *)&curColorBrightness,
                                sizeof(curColorBrightness));
      Supla::Storage::ReadState((unsigned char *)&curWhiteBrightness,
                                sizeof(curWhiteBrightness));
      Supla::Storage::ReadState((unsigned char *)&lastColorBrightness,
                                sizeof(lastColorBrightness));
      Supla::Storage::ReadState((unsigned char *)&lastWhiteBrightness,
                                sizeof(lastWhiteBrightness));
      break;
    }
    case LegacyChannelFunction::RGB: {
      Supla::Storage::ReadState((unsigned char *)&curRed, sizeof(curRed));
      Supla::Storage::ReadState((unsigned char *)&curGreen, sizeof(curGreen));
      Supla::Storage::ReadState((unsigned char *)&curBlue, sizeof(curBlue));
      Supla::Storage::ReadState((unsigned char *)&curColorBrightness,
                                sizeof(curColorBrightness));
      Supla::Storage::ReadState((unsigned char *)&lastColorBrightness,
                                sizeof(lastColorBrightness));
      break;
    }
    case LegacyChannelFunction::Dimmer: {
      Supla::Storage::ReadState((unsigned char *)&curWhiteBrightness,
                                sizeof(curWhiteBrightness));
      Supla::Storage::ReadState((unsigned char *)&lastWhiteBrightness,
                                sizeof(lastWhiteBrightness));
      break;
    }
  }
  SUPLA_LOG_DEBUG(
      "RGBCCT[%d] loaded state: r=%d, g=%d, b=%d, "
      "colorBrigh=%d, whiteBrigh=%d, whiteTemp=%d",
      getChannel()->getChannelNumber(),
      curRed,
      curGreen,
      curBlue,
      lastColorBrightness,
      lastWhiteBrightness,
      curWhiteTemperature);
}

RGBCCTBase &RGBCCTBase::setDefaultStateOn() {
  stateOnInit = RGBW_STATE_ON_INIT_ON;
  return *this;
}

RGBCCTBase &RGBCCTBase::setDefaultStateOff() {
  stateOnInit = RGBW_STATE_ON_INIT_OFF;
  return *this;
}

RGBCCTBase &RGBCCTBase::setDefaultStateRestore() {
  stateOnInit = RGBW_STATE_ON_INIT_RESTORE;
  return *this;
}

void RGBCCTBase::setMinIterationBrightness(uint8_t minBright) {
  minIterationBrightness = minBright;
}

void RGBCCTBase::setMinMaxIterationDelay(uint16_t delayMs) {
  minMaxIterationDelay = delayMs;
}

RGBCCTBase &RGBCCTBase::setBrightnessLimits(int min, int max) {
  if (min < 0) {
    min = 0;
  }
  if (max > maxHwValue) {
    setMaxHwValue(maxHwValue);
  }
  if (min > max) {
    min = max;
  }
  minBrightness = min;
  maxBrightness = max;
  return *this;
}
RGBCCTBase &RGBCCTBase::setColorBrightnessLimits(int min, int max) {
  if (min < 0) {
    min = 0;
  }
  if (max > maxHwValue) {
    setMaxHwValue(maxHwValue);
  }
  if (min > max) {
    min = max;
  }
  minColorBrightness = min;
  maxColorBrightness = max;
  return *this;
}

void RGBCCTBase::attach(Supla::Control::Button *button) {
  attachedButton = button;
}

void RGBCCTBase::onLoadConfig(SuplaDeviceClass *sdc) {
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
  }
  SUPLA_LOG_DEBUG(
      "RGBCCT[%d] button control type: %d, legacy migration needed: %d",
      getChannel()->getChannelNumber(),
      buttonControlType,
      !skipLegacyMigration &&
          legacyChannelFunction != LegacyChannelFunction::None);
}

void RGBCCTBase::fillSuplaChannelNewValue(TSD_SuplaChannelNewValue *value) {
  if (value == nullptr) {
    return;
  }

  value->value[0] = curWhiteBrightness;
  value->value[1] = curColorBrightness;
  value->value[2] = curBlue;
  value->value[3] = curGreen;
  value->value[4] = curRed;
  value->value[7] = curWhiteTemperature;
  SUPLA_LOG_DEBUG("RGBCCT[%d] fill: %d,%d,%d,%d,%d",
                  getChannelNumber(),
                  curRed,
                  curGreen,
                  curBlue,
                  curColorBrightness,
                  curWhiteBrightness);
}

int RGBCCTBase::getCurrentDimmerBrightness() const {
  return curWhiteBrightness;
}

int RGBCCTBase::getCurrentRGBBrightness() const {
  return curColorBrightness;
}

void RGBCCTBase::setMaxHwValue(int newMaxHwValue) {
  maxHwValue = newMaxHwValue;
  if (brightnessAdjuster) {
    brightnessAdjuster->setMaxHwValue(newMaxHwValue);
  }
}

void RGBCCTBase::purgeConfig() {
  Supla::ChannelElement::purgeConfig();
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    generateKey(key, Supla::ConfigTag::RgbwButtonTag);
    cfg->eraseKey(key);
  }
}

ApplyConfigResult RGBCCTBase::applyChannelConfig(TSD_ChannelConfig *, bool) {
  SUPLA_LOG_WARNING("RGBCCT[%d] applyChannelConfig missing",
                    getChannelNumber());
  return ApplyConfigResult::Success;
}

void RGBCCTBase::fillChannelConfig(void *, int *size, uint8_t) {
  SUPLA_LOG_DEBUG("RGBCCT[%d] fillChannelConfig missing", getChannelNumber());
  if (size) {
    *size = 0;
  }
}

void RGBCCTBase::convertStorageFromLegacyChannel(
    LegacyChannelFunction channelFunction) {
  legacyChannelFunction = channelFunction;
}

int RGBCCTBase::getMissingGpioCount() const {
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

void RGBCCTBase::enableChannel() {
  if (enabled) {
    return;
  }

  lastTick = 0;
  enabled = true;
  getChannel()->setStateOnline();
}

void RGBCCTBase::disableChannel() {
  if (!enabled) {
    return;
  }

  enabled = false;
  getChannel()->setStateOnlineAndNotAvailable();
}

bool RGBCCTBase::hasParent() const {
  return parent != nullptr;
}

int RGBCCTBase::getAncestorCount() const {
  if (hasParent()) {
    return parent->getAncestorCount() + 1;
  }
  return 0;
}

bool RGBCCTBase::isStateStorageMigrationNeeded() const {
  return !skipLegacyMigration &&
         legacyChannelFunction != LegacyChannelFunction::None;
}

void RGBCCTBase::setSkipLegacyMigration() {
  skipLegacyMigration = true;
}

};  // namespace Control
};  // namespace Supla

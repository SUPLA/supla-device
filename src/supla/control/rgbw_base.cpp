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

#include "rgbw_base.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <supla/log_wrapper.h>
#include <supla/control/button.h>
#include <supla/storage/config.h>
#include <supla/network/html/rgbw_button_parameters.h>

#include "../storage/storage.h"
#include "../time.h"
#include "../tools.h"
#include "supla/actions.h"

#ifdef ARDUINO_ARCH_ESP32
int esp32PwmChannelCounter = 0;
#endif

#define RGBW_STATE_ON_INIT_RESTORE -1
#define RGBW_STATE_ON_INIT_OFF     0
#define RGBW_STATE_ON_INIT_ON      1

namespace Supla {
namespace Control {

GeometricBrightnessAdjuster::GeometricBrightnessAdjuster(double power,
                                                         int offset)
    : power(power), offset(offset) {
}

int GeometricBrightnessAdjuster::adjustBrightness(int input) {
  if (input == 0) {
    return 0;
  }
  double result = (input + offset) / (100.0 + offset);
  result = pow(result, power);
  result = result * 1023.0;
  if (result > 1023) {
    result = 1023;
  }
  if (result < 0) {
    result = 0;
  }
  return round(result);
}


RGBWBase::RGBWBase()
    : buttonStep(5),
      curRed(0),
      curGreen(255),
      curBlue(0),
      curColorBrightness(0),
      curBrightness(0),
      lastColorBrightness(100),
      lastBrightness(100),
      defaultDimmedBrightness(20),
      dimIterationDirection(false),
      fadeEffect(500),
      hwRed(0),
      hwGreen(0),
      hwBlue(0),
      hwColorBrightness(0),
      hwBrightness(0),
      stateOnInit(RGBW_STATE_ON_INIT_RESTORE),
      minIterationBrightness(1) {
  channel.setType(SUPLA_CHANNELTYPE_DIMMERANDRGBLED);
  channel.setDefault(SUPLA_CHANNELFNC_DIMMERANDRGBLIGHTING);
  channel.setFlag(SUPLA_CHANNEL_FLAG_RGBW_COMMANDS_SUPPORTED);
}

void RGBWBase::setBrightnessAdjuster(BrightnessAdjuster *adjuster) {
  if (brightnessAdjuster) {
    delete brightnessAdjuster;
  }
  brightnessAdjuster = adjuster;
}

void RGBWBase::setRGBW(int red,
                       int green,
                       int blue,
                       int colorBrightness,
                       int brightness,
                       bool toggle) {
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
  if (toggle && brightness == 100) {
    brightness = lastBrightness;
  } else if (brightness > 0) {
    lastBrightness = brightness;
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
  if (brightness >= 0) {
    curBrightness = brightness;
  }

  resetDisance = true;

  SUPLA_LOG_DEBUG("RGBW: %d,%d,%d,%d,%d", curRed, curGreen, curBlue,
                  curColorBrightness, curBrightness);

  // Schedule save in 5 s after state change
  Supla::Storage::ScheduleSave(5000);
}

void RGBWBase::iterateAlways() {
  if (lastMsgReceivedMs != 0 && millis() - lastMsgReceivedMs >= 400) {
    lastMsgReceivedMs = 0;
    // Send to Supla server new values
    channel.setNewValue(
        curRed, curGreen, curBlue, curColorBrightness, curBrightness);
  }
}

int RGBWBase::handleNewValueFromServer(TSD_SuplaChannelNewValue *newValue) {
  uint8_t command = static_cast<uint8_t>(newValue->value[6]);
  uint8_t toggleOnOff = static_cast<uint8_t>(newValue->value[5]);
  uint8_t red = static_cast<uint8_t>(newValue->value[4]);
  uint8_t green = static_cast<uint8_t>(newValue->value[3]);
  uint8_t blue = static_cast<uint8_t>(newValue->value[2]);
  uint8_t colorBrightness = static_cast<uint8_t>(newValue->value[1]);
  uint8_t brightness = static_cast<uint8_t>(newValue->value[0]);

  if (brightness > 100) {
    brightness = 100;
  }
  if (colorBrightness > 100) {
    colorBrightness = 100;
  }

  switch (command) {
    case RGBW_COMMAND_NOT_SET: {
      setRGBW(red, green, blue, colorBrightness, brightness, toggleOnOff > 0);
      break;
    }
    case RGBW_COMMAND_TURN_ON_DIMMER: {
      setRGBW(-1, -1, -1, -1, lastBrightness);
      break;
    }
    case RGBW_COMMAND_TURN_OFF_DIMMER: {
      setRGBW(-1, -1, -1, -1, 0);
      break;
    }
    case RGBW_COMMAND_TOGGLE_DIMMER: {
      setRGBW(-1, -1, -1, -1, curBrightness > 0 ? 0 : lastBrightness);
      break;
    }
    case RGBW_COMMAND_TURN_ON_RGB: {
      setRGBW(-1, -1, -1, lastColorBrightness, -1);
      break;
    }
    case RGBW_COMMAND_TURN_OFF_RGB: {
      setRGBW(-1, -1, -1, 0, -1);
      break;
    }
    case RGBW_COMMAND_TOGGLE_RGB: {
      setRGBW(-1, -1, -1, curColorBrightness > 0 ? 0 : lastColorBrightness, -1);
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
      if (curBrightness > 0) {
        setRGBW(-1, -1, -1, -1, brightness);
      } else {
        lastBrightness = brightness;
      }
      break;
    }
    case RGBW_COMMAND_SET_COLOR_BRIGHTNESS_WITHOUT_TURN_ON: {
      if (curColorBrightness > 0) {
        setRGBW(-1, -1, -1, colorBrightness, -1);
      } else {
        lastColorBrightness = colorBrightness;
      }
      break;
    }
    case RGBW_COMMAND_SET_RGB_WITHOUT_TURN_ON: {
      setRGBW(red, green, blue, -1, -1);
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
  }
  return -1;
}

void RGBWBase::turnOn() {
  setRGBW(-1, -1, -1, lastColorBrightness, lastBrightness);
}
void RGBWBase::turnOff() {
  setRGBW(-1, -1, -1, 0, 0);
}

void RGBWBase::toggle() {
  if (isOn()) {
    turnOff();
  } else {
    turnOn();
  }
}

bool RGBWBase::isOn() {
  return isOnRGB() || isOnW();
}

bool RGBWBase::isOnW() {
  return curBrightness > 0;
}

bool RGBWBase::isOnRGB() {
  return curColorBrightness > 0;
}

uint8_t RGBWBase::addWithLimit(int value, int addition, int limit) {
  if (addition > 0 && value + addition > limit) {
    return limit;
  }
  if (addition < 0 && value + addition < 0) {
    return 0;
  }
  return value + addition;
}

void RGBWBase::handleAction(int event, int action) {
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
      setRGBW(-1,
              -1,
              -1,
              addWithLimit(curColorBrightness, buttonStep, 100),
              addWithLimit(curBrightness, buttonStep, 100));
      break;
    }
    case DIM_ALL: {
      setRGBW(-1,
              -1,
              -1,
              addWithLimit(curColorBrightness, -buttonStep, 100),
              addWithLimit(curBrightness, -buttonStep, 100));
      break;
    }
    case BRIGHTEN_R: {
      setRGBW(addWithLimit(curRed, buttonStep), -1, -1, -1, -1);
      break;
    }
    case DIM_R: {
      setRGBW(addWithLimit(curRed, -buttonStep), -1, -1, -1, -1);
      break;
    }
    case BRIGHTEN_G: {
      setRGBW(-1, addWithLimit(curGreen, buttonStep), -1, -1, -1);
      break;
    }
    case DIM_G: {
      setRGBW(-1, addWithLimit(curGreen, -buttonStep), -1, -1, -1);
      break;
    }
    case BRIGHTEN_B: {
      setRGBW(-1, -1, addWithLimit(curBlue, buttonStep), -1, -1);
      break;
    }
    case DIM_B: {
      setRGBW(-1, -1, addWithLimit(curBlue, -buttonStep), -1, -1);
      break;
    }
    case BRIGHTEN_W: {
      setRGBW(-1, -1, -1, -1, addWithLimit(curBrightness, buttonStep, 100));
      break;
    }
    case DIM_W: {
      setRGBW(-1, -1, -1, -1, addWithLimit(curBrightness, -buttonStep, 100));
      break;
    }
    case BRIGHTEN_RGB: {
      setRGBW(
          -1, -1, -1, addWithLimit(curColorBrightness, buttonStep, 100), -1);
      break;
    }
    case DIM_RGB: {
      setRGBW(
          -1, -1, -1, addWithLimit(curColorBrightness, -buttonStep, 100), -1);
      break;
    }
    case TURN_ON_RGB: {
      setRGBW(-1, -1, -1, lastColorBrightness, -1);
      break;
    }
    case TURN_OFF_RGB: {
      setRGBW(-1, -1, -1, 0, -1);
      break;
    }
    case TOGGLE_RGB: {
      setRGBW(-1, -1, -1, curColorBrightness > 0 ? 0 : lastColorBrightness, -1);
      break;
    }
    case TURN_ON_W: {
      setRGBW(-1, -1, -1, -1, lastBrightness);
      break;
    }
    case TURN_OFF_W: {
      setRGBW(-1, -1, -1, -1, 0);
      break;
    }
    case TOGGLE_W: {
      setRGBW(-1, -1, -1, -1, curBrightness > 0 ? 0 : lastBrightness);
      break;
    }
    case TURN_ON_RGB_DIMMED: {
      if (curColorBrightness == 0) {
        setRGBW(-1, -1, -1, defaultDimmedBrightness, -1);
      }
      break;
    }
    case TURN_ON_W_DIMMED: {
      if (curBrightness == 0) {
        setRGBW(-1, -1, -1, -1, defaultDimmedBrightness);
      }
      break;
    }
    case TURN_ON_ALL_DIMMED: {
      if (curBrightness == 0 && curColorBrightness == 0) {
        setRGBW(-1, -1, -1, defaultDimmedBrightness, defaultDimmedBrightness);
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

void RGBWBase::iterateDimmerRGBW(int rgbStep, int wStep) {
  // if we iterate both RGB and W, then we should sync brightness
  if (rgbStep > 0 && wStep > 0) {
    curBrightness = curColorBrightness;
  }

  // change iteration direction if there was no action in last 0.5 s
  if (millis() - lastIterateDimmerTimestamp >= 500) {
    dimIterationDirection = !dimIterationDirection;
    iterationDelayTimestamp = 0;
    if (curBrightness <= 5) {
      dimIterationDirection = false;
    } else if (curBrightness >= 95) {
      dimIterationDirection = true;
    }
    if (millis() - lastIterateDimmerTimestamp >= 10000) {
      if (curBrightness <= 40) {
        dimIterationDirection = false;
      } else if (curBrightness >= 60) {
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
    if (curBrightness <= minIterationBrightness &&
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
    } else if (curBrightness == 100 && dimIterationDirection == false) {
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
  if (wStep && curBrightness + wStep < minIterationBrightness) {
    wStep = minIterationBrightness - curBrightness;
  }

  if ((wStep != 0 && curBrightness == 0)
      || (rgbStep != 0 && curColorBrightness == 0)) {
    iterationDelayTimestamp = millis();
    dimIterationDirection = true;
  }

  setRGBW(-1,
          -1,
          -1,
          addWithLimit(curColorBrightness, rgbStep, 100),
          addWithLimit(curBrightness, wStep, 100));
}

void RGBWBase::setStep(int step) {
  buttonStep = step;
}

void RGBWBase::setDefaultDimmedBrightness(int dimmedBrightness) {
  defaultDimmedBrightness = dimmedBrightness;
}

void RGBWBase::setFadeEffectTime(int timeMs) {
  fadeEffect = timeMs;
}

int RGBWBase::adjustBrightness(int value) {
  if (brightnessAdjuster) {
    return brightnessAdjuster->adjustBrightness(value);
  }
  return adjustRange(value, 0, 100, 0, 1023);
}

double RGBWBase::getStep(int step, int target, double current, int distance) {
  if (step) {
    double result = step;
    if (target > current) {
      if (fadeEffect > 0 && distance < 100) {
        result = step / 3.0;
      }
      if (current + result > target) {
        result = target - current;
      }
      return result;
    } else if (target < current) {
      result = -step;
      if (fadeEffect > 0 && distance < 100) {
        result = result / 3.0;
      }
      if (current + result < target) {
        result = target - current;
      }
      return result;
    }
  }
  return 0;
}

void RGBWBase::onFastTimer() {
  if (lastTick == 0) {
    lastTick = millis();
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
    double divider = 1.0 * fadeEffect / timeDiff;
    if (divider <= 0) {
      divider = 1;
    }

    int step = 1023 / divider;
    if (step < 1) {
      return;
    }

    lastTick = millis();

    // target values are in 0..1023 range
    int targetRed = adjustRange(curRed, 0, 255, 0, 1023);
    int targetGreen = adjustRange(curGreen, 0, 255, 0, 1023);
    int targetBlue = adjustRange(curBlue, 0, 255, 0, 1023);
    int targetColorBrightness = adjustBrightness(curColorBrightness);
    int targetBrightness = adjustBrightness(curBrightness);

    if (resetDisance) {
      resetDisance = false;

      redDistance = abs(targetRed - hwRed);
      greenDistance = abs(targetGreen - hwGreen);
      blueDistance = abs(targetBlue - hwBlue);
      colorBrightnessDistance = abs(targetColorBrightness - hwColorBrightness);
      brightnessDistance = abs(targetBrightness - hwBrightness);
    }

    auto redStep = getStep(step, targetRed, hwRed, redDistance);
    if (redStep != 0) {
      valueChanged = true;
      hwRed += redStep;
    }

    auto greenStep = getStep(step, targetGreen, hwGreen, greenDistance);
    if (greenStep != 0) {
      valueChanged = true;
      hwGreen += greenStep;
    }

    auto blueStep = getStep(step, targetBlue, hwBlue, blueDistance);
    if (blueStep != 0) {
      valueChanged = true;
      hwBlue += blueStep;
    }

    auto colorBrightnessStep = getStep(step,
                                       targetColorBrightness,
                                       hwColorBrightness,
                                       colorBrightnessDistance);
    if (colorBrightnessStep != 0) {
      valueChanged = true;
      hwColorBrightness += colorBrightnessStep;
    }

    auto brightnessStep =
        getStep(step, targetBrightness, hwBrightness, brightnessDistance);
    if (brightnessStep != 0) {
      valueChanged = true;
      hwBrightness += brightnessStep;
    }
    if (valueChanged) {
      uint32_t adjColorBrightness = hwColorBrightness;
      if (hwColorBrightness > 0) {
        adjColorBrightness = adjustRange(adjColorBrightness,
                                         1,
                                         1023,
                                         minColorBrightness,
                                         maxColorBrightness);
      }
      uint32_t adjBrightness = hwBrightness;
      if (hwBrightness > 0) {
        adjBrightness =
            adjustRange(adjBrightness, 1, 1023, minBrightness, maxBrightness);
      }

      setRGBWValueOnDevice(
          hwRed, hwGreen, hwBlue, adjColorBrightness, adjBrightness);
      valueChanged = false;
    }
  }
}

void RGBWBase::onInit() {
  if (attachedButton) {
    SUPLA_LOG_DEBUG("RGBWBase[%d] configuring attachedButton, control type %d",
                    getChannel()->getChannelNumber(), buttonControlType);
    if (attachedButton->isMonostable()) {
      SUPLA_LOG_DEBUG("RGBWBase[%d] configuring monostable button",
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
      SUPLA_LOG_DEBUG("RGBWBase[%d] configuring bistable button",
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
    } else if (attachedButton->isMotionSensor()) {
      SUPLA_LOG_DEBUG("RGBWBase[%d] configuring motion sensor button",
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
        SUPLA_LOG_DEBUG("RGBWBase[%d] button pressed",
                        getChannel()->getChannelNumber());
        switch (buttonControlType) {
          case BUTTON_FOR_RGBW: {
            curColorBrightness = lastColorBrightness;
            curBrightness = lastBrightness;
            break;
          }
          case BUTTON_FOR_RGB: {
            curColorBrightness = lastColorBrightness;
            break;
          }
          case BUTTON_FOR_W: {
            curBrightness = lastBrightness;
            break;
          }
          case BUTTON_NOT_USED: {
            break;
          }
        }
      } else {
        SUPLA_LOG_DEBUG("RGBWBase[%d] button not pressed",
                        getChannel()->getChannelNumber());
        switch (buttonControlType) {
          case BUTTON_FOR_RGBW: {
            curColorBrightness = 0;
            curBrightness = 0;
            break;
          }
          case BUTTON_FOR_RGB: {
            curColorBrightness = 0;
            break;
          }
          case BUTTON_FOR_W: {
            curBrightness = 0;
            break;
          }
          case BUTTON_NOT_USED: {
            break;
          }
        }
      }
    } else {
      SUPLA_LOG_WARNING("RGBWBase[%d] unknown button type",
                        getChannel()->getChannelNumber());
    }
  }

  bool toggle = false;
  if (stateOnInit == RGBW_STATE_ON_INIT_ON) {
    SUPLA_LOG_DEBUG("RGBWBase[%d] TURN on onInit",
                    getChannel()->getChannelNumber());
    curColorBrightness = 100;
    curBrightness = 100;
    toggle = true;
  } else if (stateOnInit == RGBW_STATE_ON_INIT_OFF) {
    SUPLA_LOG_DEBUG("RGBWBase[%d] TURN off onInit",
                    getChannel()->getChannelNumber());
    curColorBrightness = 0;
    curBrightness = 0;
  }

  setRGBW(curRed, curGreen, curBlue, curColorBrightness, curBrightness, toggle);
}

void RGBWBase::onSaveState() {
  /*
  uint8_t curRed;                   // 0 - 255
  uint8_t curGreen;                 // 0 - 255
  uint8_t curBlue;                  // 0 - 255
  uint8_t curColorBrightness;       // 0 - 100
  uint8_t curBrightness;            // 0 - 100
  uint8_t lastColorBrightness;      // 0 - 100
  uint8_t lastBrightness;           // 0 - 100
  */
  Supla::Storage::WriteState((unsigned char *)&curRed, sizeof(curRed));
  Supla::Storage::WriteState((unsigned char *)&curGreen, sizeof(curGreen));
  Supla::Storage::WriteState((unsigned char *)&curBlue, sizeof(curBlue));
  Supla::Storage::WriteState((unsigned char *)&curColorBrightness,
                             sizeof(curColorBrightness));
  Supla::Storage::WriteState((unsigned char *)&curBrightness,
                             sizeof(curBrightness));
  Supla::Storage::WriteState((unsigned char *)&lastColorBrightness,
                             sizeof(lastColorBrightness));
  Supla::Storage::WriteState((unsigned char *)&lastBrightness,
                             sizeof(lastBrightness));
}

void RGBWBase::onLoadState() {
  Supla::Storage::ReadState((unsigned char *)&curRed, sizeof(curRed));
  Supla::Storage::ReadState((unsigned char *)&curGreen, sizeof(curGreen));
  Supla::Storage::ReadState((unsigned char *)&curBlue, sizeof(curBlue));
  Supla::Storage::ReadState((unsigned char *)&curColorBrightness,
                            sizeof(curColorBrightness));
  Supla::Storage::ReadState((unsigned char *)&curBrightness,
                            sizeof(curBrightness));
  Supla::Storage::ReadState((unsigned char *)&lastColorBrightness,
                            sizeof(lastColorBrightness));
  Supla::Storage::ReadState((unsigned char *)&lastBrightness,
                            sizeof(lastBrightness));
  SUPLA_LOG_DEBUG(
      "RGBWBase[%d] loaded state: red=%d, green=%d, blue=%d, "
      "colorBrightness=%d, brightness=%d",
      getChannel()->getChannelNumber(), curRed, curGreen, curBlue,
      curColorBrightness, curBrightness);
}

RGBWBase &RGBWBase::setDefaultStateOn() {
  stateOnInit = RGBW_STATE_ON_INIT_ON;
  return *this;
}

RGBWBase &RGBWBase::setDefaultStateOff() {
  stateOnInit = RGBW_STATE_ON_INIT_OFF;
  return *this;
}

RGBWBase &RGBWBase::setDefaultStateRestore() {
  stateOnInit = RGBW_STATE_ON_INIT_RESTORE;
  return *this;
}

void RGBWBase::setMinIterationBrightness(uint8_t minBright) {
  minIterationBrightness = minBright;
}

void RGBWBase::setMinMaxIterationDelay(uint16_t delayMs) {
  minMaxIterationDelay = delayMs;
}


RGBWBase &RGBWBase::setBrightnessLimits(int min, int max) {
  if (min < 0) {
    min = 0;
  }
  if (max > 1023) {
    max = 1023;
  }
  if (min > max) {
    min = max;
  }
  minBrightness = min;
  maxBrightness = max;
  return *this;
}
RGBWBase &RGBWBase::setColorBrightnessLimits(int min, int max) {
  if (min < 0) {
    min = 0;
  }
  if (max > 1023) {
    max = 1023;
  }
  if (min > max) {
    min = max;
  }
  minColorBrightness = min;
  maxColorBrightness = max;
  return *this;
}

void RGBWBase::attach(Supla::Control::Button *button) {
  attachedButton = button;
}

void RGBWBase::onLoadConfig(SuplaDeviceClass *sdc) {
  (void)(sdc);
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    Supla::Config::generateKey(
        key, getChannel()->getChannelNumber(), Supla::Html::RgbwButtonTag);
    int32_t rgbwButtonControlType = 0;
    // try to read RGBW button control type from channel specific parameter
    // and if it is missgin, read gloal value setting
    if (!cfg->getInt32(key, &rgbwButtonControlType)) {
      cfg->getInt32(Supla::Html::RgbwButtonTag, &rgbwButtonControlType);
    }
    if (rgbwButtonControlType >= 0 && rgbwButtonControlType <= 4) {
      buttonControlType = static_cast<ButtonControlType>(rgbwButtonControlType);
    }
  }
  SUPLA_LOG_DEBUG("RGBWBase[%d] button control type: %d",
                  getChannel()->getChannelNumber(),
                  buttonControlType);
}

void RGBWBase::fillSuplaChannelNewValue(TSD_SuplaChannelNewValue *value) {
  if (value == nullptr) {
    return;
  }

  value->value[0] = curBrightness;
  value->value[1] = curColorBrightness;
  value->value[2] = curBlue;
  value->value[3] = curGreen;
  value->value[4] = curRed;
  SUPLA_LOG_DEBUG("RGBW fill: %d,%d,%d,%d,%d", curRed, curGreen, curBlue,
                  curColorBrightness, curBrightness);
}

int RGBWBase::getCurrentDimmerBrightness() const {
  return curBrightness;
}

int RGBWBase::getCurrentRGBBrightness() const {
  return curColorBrightness;
}

};  // namespace Control
};  // namespace Supla

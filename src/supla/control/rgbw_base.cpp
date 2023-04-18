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

#include <stdint.h>

#include "../storage/storage.h"
#include "../time.h"
#include "../tools.h"
#include "rgbw_base.h"

#define RGBW_STATE_ON_INIT_RESTORE -1
#define RGBW_STATE_ON_INIT_OFF     0
#define RGBW_STATE_ON_INIT_ON      1

#ifdef ARDUINO_ARCH_ESP32
int esp32PwmChannelCouner = 0;
#endif

namespace Supla {
namespace Control {

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
      hwRed(-1),
      hwGreen(0),
      hwBlue(0),
      hwColorBrightness(0),
      hwBrightness(0),
      stateOnInit(RGBW_STATE_ON_INIT_RESTORE),
      minIterationBrightness(1) {
  channel.setType(SUPLA_CHANNELTYPE_DIMMERANDRGBLED);
  channel.setDefault(SUPLA_CHANNELFNC_DIMMERANDRGBLIGHTING);
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
  uint8_t toggle = static_cast<uint8_t>(newValue->value[5]);
  uint8_t red = static_cast<uint8_t>(newValue->value[4]);
  uint8_t green = static_cast<uint8_t>(newValue->value[3]);
  uint8_t blue = static_cast<uint8_t>(newValue->value[2]);
  uint8_t colorBrightness = static_cast<uint8_t>(newValue->value[1]);
  uint8_t brightness = static_cast<uint8_t>(newValue->value[0]);

  setRGBW(red, green, blue, colorBrightness, brightness, toggle > 0);

  return -1;
}

void RGBWBase::turnOn() {
  setRGBW(-1, -1, -1, lastColorBrightness, lastBrightness);
}
void RGBWBase::turnOff() {
  setRGBW(-1, -1, -1, 0, 0);
}

void RGBWBase::toggle() {
  setRGBW(-1,
          -1,
          -1,
          curColorBrightness > 0 ? 0 : lastColorBrightness,
          curBrightness > 0 ? 0 : lastBrightness);
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

void RGBWBase::onTimer() {
  uint64_t timeDiff = millis() - lastTick;
  lastTick = millis();

  if (timeDiff > 0) {
    double divider = 1.0 * fadeEffect / timeDiff;
    if (divider <= 0) {
      divider = 1;
    }

    double step = 1023 / divider;
    bool valueChanged = false;
    if (step < 1) {
      step = 1;
    }

    int curRedAdj = adjustRange(curRed, 0, 255, 0, 1023);
    int curGreenAdj = adjustRange(curGreen, 0, 255, 0, 1023);
    int curBlueAdj = adjustRange(curBlue, 0, 255, 0, 1023);
    int curColorBrightnessAdj =
        adjustRange(curColorBrightness, 0, 100, 0, 1023);
    int curBrightnessAdj = adjustRange(curBrightness, 0, 100, 0, 1023);

    if (curRedAdj > hwRed) {
      valueChanged = true;
      hwRed += step;
      if (hwRed > curRedAdj) {
        hwRed = curRedAdj;
      }
    } else if (curRedAdj < hwRed) {
      valueChanged = true;
      hwRed -= step;
      if (hwRed < curRedAdj) {
        hwRed = curRedAdj;
      }
    }

    if (curGreenAdj > hwGreen) {
      valueChanged = true;
      hwGreen += step;
      if (hwGreen > curGreenAdj) {
        hwGreen = curGreenAdj;
      }
    } else if (curGreenAdj < hwGreen) {
      valueChanged = true;
      hwGreen -= step;
      if (hwGreen < curGreenAdj) {
        hwGreen = curGreenAdj;
      }
    }

    if (curBlueAdj > hwBlue) {
      valueChanged = true;
      hwBlue += step;
      if (hwBlue > curBlueAdj) {
        hwBlue = curBlueAdj;
      }
    } else if (curBlueAdj < hwBlue) {
      valueChanged = true;
      hwBlue -= step;
      if (hwBlue < curBlueAdj) {
        hwBlue = curBlueAdj;
      }
    }

    if (curColorBrightnessAdj > hwColorBrightness) {
      valueChanged = true;
      hwColorBrightness += step;
      if (hwColorBrightness > curColorBrightnessAdj) {
        hwColorBrightness = curColorBrightnessAdj;
      }
    } else if (curColorBrightnessAdj < hwColorBrightness) {
      valueChanged = true;
      hwColorBrightness -= step;
      if (hwColorBrightness < curColorBrightnessAdj) {
        hwColorBrightness = curColorBrightnessAdj;
      }
    }

    if (curBrightnessAdj > hwBrightness) {
      valueChanged = true;
      hwBrightness += step;
      if (hwBrightness > curBrightnessAdj) {
        hwBrightness = curBrightnessAdj;
      }
    } else if (curBrightnessAdj < hwBrightness) {
      valueChanged = true;
      hwBrightness -= step;
      if (hwBrightness < curBrightnessAdj) {
        hwBrightness = curBrightnessAdj;
      }
    }

    if (valueChanged) {
      uint32_t adjColorBrightness = adjustRange(
          hwColorBrightness, 0, 1023, minColorBrightness, maxColorBrightness);
      uint32_t adjBrightness =
          adjustRange(hwBrightness, 0, 1023, minBrightness, maxBrightness);
      setRGBWValueOnDevice(
          hwRed, hwGreen, hwBlue, adjColorBrightness, adjBrightness);
    }
  }
}

void RGBWBase::onInit() {
  if (stateOnInit == RGBW_STATE_ON_INIT_ON) {
    curColorBrightness = 100;
    curBrightness = 100;
  } else if (stateOnInit == RGBW_STATE_ON_INIT_OFF) {
    curColorBrightness = 0;
    curBrightness = 0;
  }

  setRGBW(curRed, curGreen, curBlue, curColorBrightness, curBrightness);
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

};  // namespace Control
};  // namespace Supla

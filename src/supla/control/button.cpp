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

#include <supla/time.h>
#include <supla/storage/storage.h>
#include <supla/storage/config.h>
#include <supla/network/html/button_multiclick_parameters.h>
#include <supla/network/html/button_hold_time_parameters.h>
#include <supla/network/html/button_type_parameters.h>
#include <supla/network/html/button_config_parameters.h>
#include <supla/events.h>
#include <supla/actions.h>
#include <SuplaDevice.h>
#include <supla/log_wrapper.h>

#include "button.h"

#define CFG_MODE_ON_HOLD_TIME 5000

using Supla::Control::Button;

int Button::buttonCounter = 0;

Button::Button(Supla::Io *io, int pin, bool pullUp, bool invertLogic)
    : SimpleButton(io, pin, pullUp, invertLogic) {
  buttonNumber = buttonCounter;
  buttonCounter++;
}

Button::Button(int pin, bool pullUp, bool invertLogic)
    : SimpleButton(pin, pullUp, invertLogic) {
  buttonNumber = buttonCounter;
  buttonCounter++;
}

void Button::onInit() {
  SimpleButton::onInit();
}

void Button::onTimer() {
  uint32_t timeDelta = millis() - lastStateChangeMs;
  bool stateChanged = false;
  int stateResult = state.update();
  if (stateResult == TO_PRESSED) {
    stateChanged = true;
    runAction(ON_PRESS);
    runAction(ON_CHANGE);
    if (clickCounter == 0 && holdSend == 0) {
      runAction(CONDITIONAL_ON_PRESS);
      runAction(CONDITIONAL_ON_CHANGE);
    }
  } else if (stateResult == TO_RELEASED) {
    stateChanged = true;
    runAction(ON_RELEASE);
    runAction(ON_CHANGE);
    if (clickCounter <= 1 && holdSend == 0) {
      runAction(CONDITIONAL_ON_RELEASE);
      runAction(CONDITIONAL_ON_CHANGE);
    }
  }

  if (stateChanged) {
    lastStateChangeMs = millis();
    if (multiclickTimeMs > 0 && (stateResult == TO_PRESSED || isBistable() ||
        isMotionSensor())) {
      if (clickCounter <= maxMulticlickValueConfigured) {
        // don't increase counter if already at max value
        clickCounter++;
      }
    }
  }

  if (!stateChanged && lastStateChangeMs) {
    if (isMonostable() && stateResult == PRESSED) {
      if (clickCounter <= 1 && holdTimeMs > 0 &&
          timeDelta > (holdTimeMs + holdSend * repeatOnHoldMs) &&
          (repeatOnHoldEnabled || holdSend == 0)) {
        runAction(ON_HOLD);
        ++holdSend;
      }
    } else if (stateResult == RELEASED || isBistable() || isMotionSensor()) {
      // for all button types (monostable, bistable, and motion sensor)
      if (multiclickTimeMs == 0) {
        holdSend = 0;
        clickCounter = 0;
      }
      if (multiclickTimeMs > 0 &&
          (timeDelta > multiclickTimeMs ||
           maxMulticlickValueConfigured == clickCounter)) {
        if (holdSend == 0 && clickCounter != 255) {
          switch (clickCounter) {
            case 1: {
              runAction(ON_CLICK_1);
              break;
            }
            case 2:
              runAction(ON_CLICK_2);
              break;
            case 3:
              runAction(ON_CLICK_3);
              break;
            case 4:
              runAction(ON_CLICK_4);
              break;
            case 5:
              runAction(ON_CLICK_5);
              break;
            case 6:
              runAction(ON_CLICK_6);
              break;
            case 7:
              runAction(ON_CLICK_7);
              break;
            case 8:
              runAction(ON_CLICK_8);
              break;
            case 9:
              runAction(ON_CLICK_9);
              break;
            case 10:
              runAction(ON_CLICK_10);
              runAction(ON_CRAZY_CLICKER);
              break;
          }
        } else {
          switch (clickCounter) {
            // for LONG_CLICK counter was incremented once by ON_HOLD
            case 1:
              runAction(ON_LONG_CLICK_0);
              break;
            case 2:
              runAction(ON_LONG_CLICK_1);
              break;
            case 3:
              runAction(ON_LONG_CLICK_2);
              break;
            case 4:
              runAction(ON_LONG_CLICK_3);
              break;
            case 5:
              runAction(ON_LONG_CLICK_4);
              break;
            case 6:
              runAction(ON_LONG_CLICK_5);
              break;
            case 7:
              runAction(ON_LONG_CLICK_6);
              break;
            case 8:
              runAction(ON_LONG_CLICK_7);
              break;
            case 9:
              runAction(ON_LONG_CLICK_8);
              break;
            case 10:
              runAction(ON_LONG_CLICK_9);
              break;
            case 11:
              runAction(ON_LONG_CLICK_10);
              break;
          }
        }
        clickCounter = 255;
        if (timeDelta > multiclickTimeMs) {
          holdSend = 0;
          clickCounter = 0;
        }
      }
    }
  }
}

void Button::addAction(int action, ActionHandler *client, int event,
      bool alwaysEnabled) {
  SimpleButton::addAction(action, client, event, alwaysEnabled);
  evaluateMaxMulticlickValue();
}

void Button::disableAction(int action, ActionHandler *client, int event) {
  SimpleButton::disableAction(action, client, event);
  evaluateMaxMulticlickValue();
}

void Button::enableAction(int action, ActionHandler *client, int event) {
  SimpleButton::enableAction(action, client, event);
  evaluateMaxMulticlickValue();
}

void Button::evaluateMaxMulticlickValue() {
  auto ptr = ActionHandlerClient::begin;
  uint8_t clickCounterValueForEvent = 0;
  maxMulticlickValueConfigured = 0;
  while (ptr) {
    if (ptr->trigger == this && ptr->isEnabled()) {
      switch (ptr->onEvent) {
        case ON_LONG_CLICK_1:
        case ON_CLICK_1: {
          clickCounterValueForEvent = 1;
          break;
        }
        case ON_LONG_CLICK_2:
        case ON_CLICK_2: {
          clickCounterValueForEvent = 2;
          break;
        }
        case ON_LONG_CLICK_3:
        case ON_CLICK_3: {
          clickCounterValueForEvent = 3;
          break;
        }
        case ON_LONG_CLICK_4:
        case ON_CLICK_4: {
          clickCounterValueForEvent = 4;
          break;
        }
        case ON_LONG_CLICK_5:
        case ON_CLICK_5: {
          clickCounterValueForEvent = 5;
          break;
        }
        case ON_LONG_CLICK_6:
        case ON_CLICK_6: {
          clickCounterValueForEvent = 6;
          break;
        }
        case ON_LONG_CLICK_7:
        case ON_CLICK_7: {
          clickCounterValueForEvent = 7;
          break;
        }
        case ON_LONG_CLICK_8:
        case ON_CLICK_8: {
          clickCounterValueForEvent = 8;
          break;
        }
        case ON_LONG_CLICK_9:
        case ON_CLICK_9: {
          clickCounterValueForEvent = 9;
          break;
        }
        case ON_CRAZY_CLICKER:
        case ON_LONG_CLICK_10:
        case ON_CLICK_10: {
          clickCounterValueForEvent = 10;
          break;
        }
      }
    }
    ptr = ptr->next;

    if (clickCounterValueForEvent > maxMulticlickValueConfigured) {
      maxMulticlickValueConfigured = clickCounterValueForEvent;
    }
  }
}

void Button::addAction(int action, ActionHandler &client, int event,
      bool alwaysEnabled) {
  Button::addAction(action, &client, event, alwaysEnabled);
}

void Button::setHoldTime(unsigned int timeMs) {
  holdTimeMs = timeMs;
  if (isBistable() || isMotionSensor()) {
    holdTimeMs = 0;
  }
  SUPLA_LOG_DEBUG("Button[%d]::setHoldTime: %u", getButtonNumber(), holdTimeMs);
}

void Button::setMulticlickTime(unsigned int timeMs, bool bistableButton) {
  multiclickTimeMs = timeMs;
  if (bistableButton) {
    buttonType = ButtonType::BISTABLE;
  }
  if (isBistable() || isMotionSensor()) {
    holdTimeMs = 0;
  }
  SUPLA_LOG_DEBUG(
      "Button[%d]::setMulticlickTime: %u", getButtonNumber(), timeMs);
}

void Button::repeatOnHoldEvery(unsigned int timeMs) {
  repeatOnHoldMs = timeMs;
  repeatOnHoldEnabled = (timeMs > 0);
}

bool Button::isBistable() const {
  return buttonType == ButtonType::BISTABLE;
}

bool Button::isMonostable() const {
  return buttonType == ButtonType::MONOSTABLE;
}
bool Button::isMotionSensor() const {
  return buttonType == ButtonType::MOTION_SENSOR;
}

void Button::onLoadConfig(SuplaDeviceClass *sdc) {
  if (!useOnLoadConfig) {
    SUPLA_LOG_DEBUG("Button[%d]::onLoadConfig: skip", getButtonNumber());
    return;
  }

  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    Supla::Config::generateKey(key, getButtonNumber(), Supla::Html::BtnTypeTag);
    int32_t btnTypeValue = 0;
    if (cfg->getInt32(key, &btnTypeValue)) {
      SUPLA_LOG_DEBUG("Button[%d]::onLoadConfig: btnType: %d",
                      getButtonNumber(),
                      btnTypeValue);
      switch (btnTypeValue) {
        default:
        case 0:
          setButtonType(ButtonType::MONOSTABLE);
          break;
        case 1:
          setButtonType(ButtonType::BISTABLE);
          break;
        case 2:
          setButtonType(ButtonType::MOTION_SENSOR);
          break;
      }
    }

    bool saveConfig = false;

    uint32_t multiclickTimeMsValue = 0;
    if (cfg->getUInt32(Supla::Html::BtnMulticlickTag, &multiclickTimeMsValue)) {
      if (multiclickTimeMsValue < 200) {
        multiclickTimeMsValue = 200;
      }
      if (multiclickTimeMsValue > 10000) {
        multiclickTimeMsValue = 10000;
      }
      setMulticlickTime(multiclickTimeMsValue, isBistable());
    } else {
      cfg->setUInt32(Supla::Html::BtnMulticlickTag, multiclickTimeMs);
      saveConfig = true;
    }

    uint32_t holdTimeMsValue = CFG_MODE_ON_HOLD_TIME;
    if (cfg->getUInt32(Supla::Html::BtnHoldTag, &holdTimeMsValue)) {
      if (holdTimeMsValue < 200) {
        holdTimeMsValue = 200;
      }
      if (holdTimeMsValue > 10000) {
        holdTimeMsValue = 10000;
      }
      setHoldTime(holdTimeMsValue);
    } else {
      cfg->setUInt32(Supla::Html::BtnHoldTag, holdTimeMs);
      saveConfig = true;
    }

    int32_t useInputAsConfigButtonValue = 0;
    Supla::Config::generateKey(
        key, getButtonNumber(), Supla::Html::BtnConfigTag);
    if (!cfg->getInt32(key, &useInputAsConfigButtonValue)) {
      cfg->getInt32(Supla::Html::BtnConfigTag, &useInputAsConfigButtonValue);
    }

    if (useInputAsConfigButtonValue == 0) {
      // ON is "0", which is default value
      SUPLA_LOG_DEBUG("Button[%d] enabling IN as config button",
                      getButtonNumber());
      configButton = true;
      addAction(Supla::ENTER_CONFIG_MODE_OR_RESET_TO_FACTORY,
                sdc,
                Supla::ON_CLICK_10,
                true);
      addAction(
          Supla::LEAVE_CONFIG_MODE_AND_RESET, sdc, Supla::ON_CLICK_1, true);
    }

    if (saveConfig) {
      cfg->commit();
    }
  }
}

void Button::configureAsConfigButton(SuplaDeviceClass *sdc) {
  SUPLA_LOG_DEBUG("Button[%d]::configureAsConfigButton", getButtonNumber());
  configButton = true;
  useOnLoadConfig = false;
  setHoldTime(CFG_MODE_ON_HOLD_TIME);
  setMulticlickTime(300, isBistable());
  addAction(Supla::ENTER_CONFIG_MODE_OR_RESET_TO_FACTORY,
                sdc,
                Supla::ON_HOLD,
                true);
  addAction(
      Supla::LEAVE_CONFIG_MODE_AND_RESET, sdc, Supla::ON_CLICK_1, true);
}

bool Button::disableActionsInConfigMode() {
  return configButton;
}

void Button::setButtonType(const ButtonType type) {
  SUPLA_LOG_DEBUG("Button[%d]::setButtonType: %d", getButtonNumber(), type);
  buttonType = type;
}

uint8_t Button::getMaxMulticlickValue() {
  return maxMulticlickValueConfigured;
}

int8_t Button::getButtonNumber() const {
  return buttonNumber;
}

void Button::setButtonNumber(int8_t btnNumber) {
  buttonNumber = btnNumber;
}

void Button::dontUseOnLoadConfig() {
  useOnLoadConfig = false;
}

void Button::disableRepeatOnHold(uint32_t threshold) {
  if (threshold == 0 || repeatOnHoldMs < threshold) {
    repeatOnHoldEnabled = false;
  }
}

void Button::enableRepeatOnHold() {
  repeatOnHoldEnabled = (repeatOnHoldMs > 0);
}

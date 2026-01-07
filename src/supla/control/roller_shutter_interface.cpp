/*
   Copyright (C) AC SOFTWARE SP. Z O.O

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

#include "roller_shutter_interface.h"

#include <supla/actions.h>
#include <supla/control/button.h>
#include <supla/io.h>
#include <supla/local_action.h>
#include <supla/log_wrapper.h>
#include <supla/storage/config.h>
#include <supla/storage/config_tags.h>
#include <supla/storage/storage.h>
#include <supla/time.h>

#include "supla/events.h"

using Supla::Control::RollerShutterInterface;

int16_t RollerShutterInterface::rsStorageSaveDelay = 5000;

#define RS_FLAG_CALIBRATE          (1 << 0)
#define RS_FLAG_CALIBRATION_LOST   (1 << 1)
#define RS_FLAG_CALIBRATION_FAILED (1 << 2)
#define RS_FLAG_MOTOR_PROBLEM      (1 << 3)

#pragma pack(push, 1)
struct RollerShutterStateData {
  uint32_t closingTimeMs = 0;
  uint32_t openingTimeMs = 0;
  int8_t currentPosition = -1;  // 0 - open; 100 - closed
};

struct RollerShutterWithTiltStateData {
  uint32_t closingTimeMs = 0;
  uint32_t openingTimeMs = 0;
  int8_t currentPosition = -1;
  int8_t tiltPosition = -1;
};
#pragma pack(pop)

RollerShutterInterface::RollerShutterInterface(bool tiltFunctionsSupported) {
  channel.setType(SUPLA_CHANNELTYPE_RELAY);
  channel.setFuncList(SUPLA_BIT_FUNC_CONTROLLINGTHEROLLERSHUTTER |
                      SUPLA_BIT_FUNC_CONTROLLINGTHEROOFWINDOW |
                      SUPLA_BIT_FUNC_TERRACE_AWNING |
                      SUPLA_BIT_FUNC_ROLLER_GARAGE_DOOR |
                      SUPLA_BIT_FUNC_CURTAIN | SUPLA_BIT_FUNC_PROJECTOR_SCREEN);
  if (tiltFunctionsSupported) {
    addTiltFunctions();
  }
  channel.setDefaultFunction(SUPLA_CHANNELFNC_CONTROLLINGTHEROLLERSHUTTER);
  channel.setFlag(SUPLA_CHANNEL_FLAG_RUNTIME_CHANNEL_CONFIG_UPDATE);
  usedConfigTypes.set(SUPLA_CONFIG_TYPE_DEFAULT);
}

RollerShutterInterface::~RollerShutterInterface() {
  ButtonListElement *currentElement = buttonList;
  while (currentElement) {
    ButtonListElement *nextElement = currentElement->next;
    delete currentElement;
    currentElement = nextElement;
  }
}

bool RollerShutterInterface::isFunctionSupported(
    int32_t channelFunction) const {
  switch (channelFunction) {
    case SUPLA_CHANNELFNC_CONTROLLINGTHEROLLERSHUTTER: {
      return getChannel()->getFuncList() &
             SUPLA_BIT_FUNC_CONTROLLINGTHEROLLERSHUTTER;
    }
    case SUPLA_CHANNELFNC_CONTROLLINGTHEROOFWINDOW: {
      return getChannel()->getFuncList() &
             SUPLA_BIT_FUNC_CONTROLLINGTHEROOFWINDOW;
    }
    case SUPLA_CHANNELFNC_TERRACE_AWNING: {
      return getChannel()->getFuncList() & SUPLA_BIT_FUNC_TERRACE_AWNING;
    }
    case SUPLA_CHANNELFNC_ROLLER_GARAGE_DOOR: {
      return getChannel()->getFuncList() & SUPLA_BIT_FUNC_ROLLER_GARAGE_DOOR;
    }
    case SUPLA_CHANNELFNC_CURTAIN: {
      return getChannel()->getFuncList() & SUPLA_BIT_FUNC_CURTAIN;
    }
    case SUPLA_CHANNELFNC_PROJECTOR_SCREEN: {
      return getChannel()->getFuncList() & SUPLA_BIT_FUNC_PROJECTOR_SCREEN;
    }
    case SUPLA_CHANNELFNC_CONTROLLINGTHEFACADEBLIND: {
      return getChannel()->getFuncList() &
             SUPLA_BIT_FUNC_CONTROLLINGTHEFACADEBLIND;
    }
    case SUPLA_CHANNELFNC_VERTICAL_BLIND: {
      return getChannel()->getFuncList() & SUPLA_BIT_FUNC_VERTICAL_BLIND;
    }
    default: {
      return false;
    }
  }
}

void RollerShutterInterface::onInit() {
  for (auto button = buttonList; button; button = button->next) {
    setupButtonActions(button->button, button->upButton, button->asInternal);
  }
  iterateAlways();
}

/*
 * Protocol:
 * value[0]:
 *  0 - stop
 *  1 - down
 *  2 - up
 *  10 - 110 - 0% - 100%
 *
 * time is send in 0.1 s. i.e. 105 -> 10.5 s
 * time * 100 = gives time in ms
 *
 */

int32_t RollerShutterInterface::handleNewValueFromServer(
    TSD_SuplaChannelNewValue *newValue) {
  uint32_t newClosingTime = (newValue->DurationMS & 0xFFFF) * 100;
  uint32_t newOpeningTime = ((newValue->DurationMS >> 16) & 0xFFFF) * 100;

  setOpenCloseTime(newClosingTime, newOpeningTime);

  int8_t task = newValue->value[0];
  int8_t tilt = newValue->value[1];
  SUPLA_LOG_INFO("RS[%d] new value from server: position/task %d, tilt %d",
                 channel.getChannelNumber(),
                 task,
                 tilt);
  switch (task) {
    case 0: {
      stop();
      break;
    }
    case 1: {
      moveDown();
      break;
    }
    case 2: {
      moveUp();
      break;
    }
    case 3: {  // down or stop
      if (inMove()) {
        stop();
      } else {
        moveDown();
      }
      break;
    }
    case 4: {  // up or stop
      if (inMove()) {
        stop();
      } else {
        moveUp();
      }
      break;
    }
    case 5: {  // sbs
      if (inMove()) {
        stop();
      } else if (lastDirectionWasOpen()) {
        moveDown();
      } else if (lastDirectionWasClose()) {
        moveUp();
      } else if (getCurrentPosition() < 50) {
        moveDown();
      } else {
        moveUp();
      }
      break;
    }

    default: {
      if (isTiltFunctionEnabled()) {
        if (tilt < 10 || tilt > 110) {
          tilt = UNKNOWN_POSITION;
        } else {
          tilt -= 10;
        }
        if (tilt == UNKNOWN_POSITION) {
          if (task == 110) {
            tilt = 100;
          } else if (task == 10) {
            tilt = 0;
          }
        }
      } else {
        tilt = 0;
      }
      if (task >= 10 && task <= 110) {
        setTargetPosition(task - 10, tilt);
      } else if (task == UNKNOWN_POSITION) {
        setTargetPosition(UNKNOWN_POSITION, tilt);
      }
      break;
    }
  }
  return -1;
}

void RollerShutterInterface::setOpenCloseTime(uint32_t newClosingTimeMs,
                                              uint32_t newOpeningTimeMs) {
  if (isTimeSettingAvailable()) {
    if (newClosingTimeMs != closingTimeMs ||
        newOpeningTimeMs != openingTimeMs) {
      closingTimeMs = newClosingTimeMs;
      openingTimeMs = newOpeningTimeMs;
      setCalibrationNeeded();
      SUPLA_LOG_DEBUG(
          "RS[%d] new time settings received. Opening time: %d ms; "
          "closing time: %d ms. Setting \"not calibrated\"",
          channel.getChannelNumber(),
          openingTimeMs,
          closingTimeMs);
    }
  }
}

void RollerShutterInterface::setTiltingTime(uint32_t newTiltingTimeMs,
                                            bool local) {
  if (isTimeSettingAvailable()) {
    if (newTiltingTimeMs != tiltConfig.tiltingTime) {
      tiltConfig.tiltingTime = newTiltingTimeMs;
      SUPLA_LOG_DEBUG("FB[%d] new tilting time received. Tilting time: %d ms. ",
                      channel.getChannelNumber(),
                      tiltConfig.tiltingTime);
      if (local) {
        saveConfig();
      }
    }
  }
}

void RollerShutterInterface::setTiltControlType(uint8_t newTiltControlType,
                                                bool local) {
  if (newTiltControlType != tiltConfig.tiltControlType) {
    tiltConfig.tiltControlType = newTiltControlType;
    SUPLA_LOG_DEBUG(
        "FB[%d] new tilt control type received. Tilt control type: %d. ",
        channel.getChannelNumber(),
        tiltConfig.tiltControlType);
    if (local) {
      saveConfig();
    }
  }
}

void RollerShutterInterface::handleAction(int, int action) {
  switch (action) {
    case CLOSE_OR_STOP: {
      if (inMove()) {
        stop();
      } else {
        close();
      }
      break;
    }

    case CLOSE: {
      close();
      break;
    }
    case OPEN_OR_STOP: {
      if (inMove()) {
        stop();
      } else {
        open();
      }
      break;
    }

    case OPEN: {
      open();
      break;
    }

    case COMFORT_DOWN_POSITION: {
      setTargetPosition(comfortDownValue, comfortDownTiltValue);
      break;
    }

    case COMFORT_UP_POSITION: {
      setTargetPosition(comfortUpValue, comfortUpTiltValue);
      break;
    }

    case INTERNAL_BUTTON_COMFORT_UP: {
      if (rsConfig.buttonsUpsideDown == 2) {
        setTargetPosition(comfortDownValue, comfortDownTiltValue);
      } else {
        setTargetPosition(comfortUpValue, comfortUpTiltValue);
      }
      break;
    }

    case INTERNAL_BUTTON_COMFORT_DOWN: {
      if (rsConfig.buttonsUpsideDown == 2) {
        setTargetPosition(comfortUpValue, comfortUpTiltValue);
      } else {
        setTargetPosition(comfortDownValue, comfortDownTiltValue);
      }
      break;
    }

    case STOP: {
      stop();
      break;
    }

    case STEP_BY_STEP: {
      if (inMove()) {
        stop();
      } else if (lastDirectionWasOpen()) {
        moveDown();
      } else if (lastDirectionWasClose()) {
        moveUp();
      } else if (getCurrentPosition() < 50) {
        moveDown();
      } else {
        moveUp();
      }
      break;
    }

    case MOVE_UP: {
      moveUp();
      break;
    }

    case MOVE_DOWN: {
      moveDown();
      break;
    }

    case MOVE_UP_OR_STOP: {
      if (inMove()) {
        stop();
      } else {
        moveUp();
      }
      break;
    }
    case INTERNAL_BUTTON_MOVE_UP_OR_STOP: {
      if (inMove()) {
        stop();
      } else {
        if (rsConfig.buttonsUpsideDown == 2) {
          moveDown();
        } else {
          moveUp();
        }
      }
      break;
    }
    case INTERNAL_BUTTON_MOVE_UP: {
      if (rsConfig.buttonsUpsideDown == 2) {
        moveDown();
      } else {
        moveUp();
      }
      break;
    }
    case MOVE_DOWN_OR_STOP: {
      if (inMove()) {
        stop();
      } else {
        moveDown();
      }
      break;
    }
    case INTERNAL_BUTTON_MOVE_DOWN_OR_STOP: {
      if (inMove()) {
        stop();
      } else {
        if (rsConfig.buttonsUpsideDown == 2) {
          moveUp();
        } else {
          moveDown();
        }
      }
      break;
    }
    case INTERNAL_BUTTON_MOVE_DOWN: {
      if (rsConfig.buttonsUpsideDown == 2) {
        moveUp();
      } else {
        moveDown();
      }
      break;
    }
  }
}

void RollerShutterInterface::close() {
  setTargetPosition(100, 100);
}

void RollerShutterInterface::open() {
  setTargetPosition(0, 0);
}

void RollerShutterInterface::moveDown() {
  setTargetPosition(MOVE_DOWN_POSITION);
}

void RollerShutterInterface::moveUp() {
  setTargetPosition(MOVE_UP_POSITION);
}

void RollerShutterInterface::stop() {
  setTargetPosition(STOP_REQUEST);
}

void RollerShutterInterface::setCurrentPosition(int newPosition, int newTilt) {
  if (newPosition < UNKNOWN_POSITION) {
    newPosition = UNKNOWN_POSITION;
  } else if (newPosition > 100) {
    newPosition = 100;
  }
  if (isTiltFunctionEnabled()) {
    if (newTilt < UNKNOWN_POSITION) {
      newTilt = UNKNOWN_POSITION;
    } else if (newTilt > 100) {
      newTilt = 100;
    }
    currentTilt =
        newTilt == UNKNOWN_POSITION ? UNKNOWN_POSITION : newTilt * 100;
  } else {
    currentTilt = 0;
  }

  calibrationTime = 0;
  if (newPosition == UNKNOWN_POSITION) {
    currentPosition = UNKNOWN_POSITION;
    return;
  }

  currentPosition = newPosition * 100;
  setCalibrate(false);
}

void RollerShutterInterface::setNotCalibrated() {
  currentPosition = UNKNOWN_POSITION;
  if (isTiltFunctionEnabled()) {
    currentTilt = UNKNOWN_POSITION;
  } else {
    currentTilt = 0;
  }

  calibrationTime = 0;
  setCalibrate(false);
}

void RollerShutterInterface::setTargetPosition(int newPosition, int newTilt) {
  SUPLA_LOG_DEBUG("RS[%d] set target position: %d, tilt: %d",
                  channel.getChannelNumber(),
                  newPosition,
                  newTilt);
  targetPosition = newPosition;
  if (isTiltFunctionEnabled()) {
    if ((targetPosition == UNKNOWN_POSITION || targetPosition >= 0) &&
        newTilt >= 0) {
      targetTilt = newTilt;
    } else {
      targetTilt = UNKNOWN_POSITION;
    }
  } else {
    targetTilt = -1;
  }

  if (targetPosition == MOVE_UP_POSITION) {
    lastDirection = Directions::UP_DIR;
  } else if (targetPosition == MOVE_DOWN_POSITION) {
    lastDirection = Directions::DOWN_DIR;
  } else if (targetPosition >= 0 || targetPosition == UNKNOWN_POSITION) {
    if (targetPosition != UNKNOWN_POSITION &&
        targetPosition < getCurrentPosition()) {
      lastDirection = Directions::UP_DIR;
    } else if (targetPosition > getCurrentPosition()) {
      lastDirection = Directions::DOWN_DIR;
    } else if (isTiltFunctionEnabled()) {
      if (targetTilt < getCurrentTilt()) {
        lastDirection = Directions::UP_DIR;
      } else if (targetTilt > getCurrentTilt()) {
        lastDirection = Directions::DOWN_DIR;
      }
    }
  }
  newTargetPositionAvailable = true;
}

bool RollerShutterInterface::lastDirectionWasOpen() const {
  return lastDirection == Directions::UP_DIR;
}

bool RollerShutterInterface::lastDirectionWasClose() const {
  return lastDirection == Directions::DOWN_DIR;
}

bool RollerShutterInterface::inMove() {
  return currentDirection != Directions::STOP_DIR;
}

void RollerShutterInterface::triggerCalibration() {
  setCalibrationNeeded();
  setTargetPosition(0, 0);
}

void RollerShutterInterface::setCalibrationNeeded() {
  setCurrentPosition(UNKNOWN_POSITION, UNKNOWN_POSITION);
  setCalibrate(true);
}

bool RollerShutterInterface::isCalibrationRequested() const {
  return getCalibrate() && (!isTimeSettingAvailable() ||
                            (openingTimeMs != 0 && closingTimeMs != 0));
}

bool RollerShutterInterface::isCalibrated() const {
  if (isTimeSettingAvailable()) {
    return !getCalibrate() && openingTimeMs != 0 && closingTimeMs != 0;
  } else {
    return !getCalibrate() && currentPosition != UNKNOWN_POSITION;
  }
}

bool RollerShutterInterface::isCalibrationInProgress() const {
  return calibrationTime > 0;
}

void RollerShutterInterface::startCalibration(uint32_t timeMs) {
  // Time used for calibaration is 10% higher then requested by user
  calibrationTime = timeMs * 1.1;
}

void RollerShutterInterface::stopCalibration() {
  calibrationTime = 0;
}

void RollerShutterInterface::setCalibrationOngoing(int calibrationTime) {
  this->calibrationTime = calibrationTime;
}

void RollerShutterInterface::setCalibrationFinished() {
  calibrationTime = 0;
  setCalibrate(false);
}

union RsFbValue {
  TDSC_RollerShutterValue rs;
  TDSC_FacadeBlindValue fb;
};

static_assert(sizeof(TDSC_RollerShutterValue) == sizeof(TDSC_FacadeBlindValue));

void RollerShutterInterface::iterateAlways() {
  if (lastUpdateTime != 0 && millis() - lastUpdateTime < 300) {
    return;
  }
  lastUpdateTime = millis();
  RsFbValue value = {};
  value.rs.position = getCurrentPosition();
  if (isCalibrationInProgress()) {
    value.rs.flags |= RS_VALUE_FLAG_CALIBRATION_IN_PROGRESS;
  }
  if (isCalibrationFailed()) {
    value.rs.flags |= RS_VALUE_FLAG_CALIBRATION_FAILED;
  }
  if (isCalibrationLost()) {
    value.rs.flags |= RS_VALUE_FLAG_CALIBRATION_LOST;
  }
  if (isMotorProblem()) {
    value.rs.flags |= RS_VALUE_FLAG_MOTOR_PROBLEM;
  }
  if (isTiltFunctionEnabled()) {
    value.fb.tilt = getCurrentTilt();
    value.fb.flags |= RS_VALUE_FLAG_TILT_IS_SET;
    channel.setNewValue(value.fb);
  } else {
    channel.setNewValue(value.rs);
  }
}

void RollerShutterInterface::configComfortUpValue(uint8_t position) {
  comfortUpValue = position;
  if (comfortUpValue > 100) {
    comfortUpValue = 100;
  }
}

void RollerShutterInterface::configComfortDownValue(uint8_t position) {
  comfortDownValue = position;
  if (comfortDownValue > 100) {
    comfortDownValue = 100;
  }
}

void RollerShutterInterface::configComfortUpTiltValue(uint8_t position) {
  comfortUpTiltValue = position;
  if (comfortUpTiltValue > 100) {
    comfortUpTiltValue = 100;
  }
}

void RollerShutterInterface::configComfortDownTiltValue(uint8_t position) {
  comfortDownTiltValue = position;
  if (comfortDownTiltValue > 100) {
    comfortDownTiltValue = 100;
  }
}

void RollerShutterInterface::onLoadState() {
  if (isTiltFunctionsSupported()) {
    RollerShutterWithTiltStateData data;
    if (Supla::Storage::ReadState((unsigned char *)&data, sizeof(data))) {
      closingTimeMs = data.closingTimeMs;
      openingTimeMs = data.openingTimeMs;
      currentPosition = data.currentPosition * 100;
      if (currentPosition >= 0) {
        setCalibrate(false);
      }
      currentTilt = data.tiltPosition * 100;
      SUPLA_LOG_DEBUG(
          "RS[%d] settings restored from storage. Opening time: %d "
          "ms; closing time: %d ms. Position: %d, Tilt: %d",
          channel.getChannelNumber(),
          openingTimeMs,
          closingTimeMs,
          currentPosition,
          currentTilt);
    }

  } else {
    RollerShutterStateData data;
    if (Supla::Storage::ReadState((unsigned char *)&data, sizeof(data))) {
      closingTimeMs = data.closingTimeMs;
      openingTimeMs = data.openingTimeMs;
      currentPosition = data.currentPosition * 100;
      if (currentPosition >= 0) {
        setCalibrate(false);
      }
      SUPLA_LOG_DEBUG(
          "RS[%d] settings restored from storage. Opening time: %d "
          "ms; closing time: %d ms. Position: %d",
          channel.getChannelNumber(),
          openingTimeMs,
          closingTimeMs,
          currentPosition);
    }
  }
}

void RollerShutterInterface::onSaveState() {
  if (isTiltFunctionsSupported()) {
    RollerShutterWithTiltStateData data;
    data.closingTimeMs = closingTimeMs;
    data.openingTimeMs = openingTimeMs;
    data.currentPosition = getCurrentPosition();
    data.tiltPosition = getCurrentTilt();
    Supla::Storage::WriteState((unsigned char *)&data, sizeof(data));
  } else {
    RollerShutterStateData data;
    data.closingTimeMs = closingTimeMs;
    data.openingTimeMs = openingTimeMs;
    data.currentPosition = getCurrentPosition();

    Supla::Storage::WriteState((unsigned char *)&data, sizeof(data));
  }
}

int RollerShutterInterface::getCurrentPosition() const {
  if (currentPosition < 0) {
    return UNKNOWN_POSITION;
  }
  return (currentPosition) / 100;
}

int RollerShutterInterface::getCurrentTilt() const {
  if (isTiltFunctionEnabled() && currentTilt >= 0) {
    return (currentTilt) / 100;
  }
  return UNKNOWN_POSITION;
}

int RollerShutterInterface::getTargetPosition() const {
  return targetPosition;
}

int RollerShutterInterface::getTargetTilt() const {
  if (isTiltFunctionEnabled()) {
    return targetTilt;
  }
  return UNKNOWN_POSITION;
}

int RollerShutterInterface::getCurrentDirection() const {
  return static_cast<int>(currentDirection);
}

uint32_t RollerShutterInterface::getClosingTimeMs() const {
  return closingTimeMs;
}

uint32_t RollerShutterInterface::getOpeningTimeMs() const {
  return openingTimeMs;
}

uint32_t RollerShutterInterface::getTiltingTimeMs() const {
  return tiltConfig.tiltingTime;
}

uint32_t RollerShutterInterface::getTiltControlType() const {
  return tiltConfig.tiltControlType;
}

void RollerShutterInterface::attach(Supla::Control::Button *up,
                                    Supla::Control::Button *down) {
  attach(up, true, true);
  attach(down, false, true);
}

void RollerShutterInterface::attach(Supla::Control::Button *button,
                                    bool upButton,
                                    bool asInternal) {
  if (button == nullptr) {
    return;
  }

  SUPLA_LOG_DEBUG("RS[%d] attaching button %d, %s, %s",
                  channel.getChannelNumber(),
                  button->getButtonNumber(),
                  upButton ? "up" : "down",
                  asInternal ? "internal" : "external");
  auto lastButtonListElement = buttonList;
  while (lastButtonListElement && lastButtonListElement->next) {
    lastButtonListElement = lastButtonListElement->next;
  }

  if (lastButtonListElement) {
    lastButtonListElement->next = new ButtonListElement;
    lastButtonListElement = lastButtonListElement->next;
  } else {
    lastButtonListElement = new ButtonListElement;
  }

  lastButtonListElement->button = button;
  lastButtonListElement->upButton = upButton;
  lastButtonListElement->asInternal = asInternal;

  if (buttonList == nullptr) {
    buttonList = lastButtonListElement;
  }
}

int RollerShutterInterface::handleCalcfgFromServer(
    TSD_DeviceCalCfgRequest *request) {
  if (request) {
    if (request->Command == SUPLA_CALCFG_CMD_RECALIBRATE) {
      if (!request->SuperUserAuthorized) {
        return SUPLA_CALCFG_RESULT_UNAUTHORIZED;
      }
      SUPLA_LOG_INFO("RS[%d] - CALCFG recalibrate received",
                     channel.getChannelNumber());
      triggerCalibration();
      return SUPLA_CALCFG_RESULT_DONE;
    }
  }
  return SUPLA_CALCFG_RESULT_NOT_SUPPORTED;
}

Supla::ApplyConfigResult RollerShutterInterface::applyChannelConfig(
    TSD_ChannelConfig *result, bool) {
  SUPLA_LOG_DEBUG(
      "RS[%d] applyChannelConfig, func %d, configtype %d, configsize %d",
      getChannelNumber(),
      result->Func,
      result->ConfigType,
      result->ConfigSize);

  if (result->ConfigSize == 0) {
    return Supla::ApplyConfigResult::SetChannelConfigNeeded;
  }

  bool setChannelConfigNeeded = false;
  switch (result->Func) {
    case SUPLA_CHANNELFNC_CONTROLLINGTHEROLLERSHUTTER:
    case SUPLA_CHANNELFNC_TERRACE_AWNING:
    case SUPLA_CHANNELFNC_ROLLER_GARAGE_DOOR:
    case SUPLA_CHANNELFNC_CURTAIN:
    case SUPLA_CHANNELFNC_PROJECTOR_SCREEN:
    case SUPLA_CHANNELFNC_CONTROLLINGTHEROOFWINDOW: {
      if (result->ConfigType == 0 &&
          result->ConfigSize == sizeof(TChannelConfig_RollerShutter)) {
        auto newConfig =
            reinterpret_cast<TChannelConfig_RollerShutter *>(result->Config);
        if (newConfig->OpeningTimeMS >= 0 && newConfig->ClosingTimeMS >= 0) {
          setOpenCloseTime(newConfig->ClosingTimeMS, newConfig->OpeningTimeMS);
        }
        if (!inMove()) {
          setTargetPosition(STOP_REQUEST);
        }
        if (rsConfig.buttonsUpsideDown != 0) {
          if (newConfig->ButtonsUpsideDown > 0) {
            rsConfig.buttonsUpsideDown = newConfig->ButtonsUpsideDown;
          } else {
            setChannelConfigNeeded = true;
          }
        }
        if (rsConfig.motorUpsideDown != 0) {
          if (newConfig->MotorUpsideDown > 0) {
            rsConfig.motorUpsideDown = newConfig->MotorUpsideDown;
          } else {
            setChannelConfigNeeded = true;
          }
        }
        if (rsConfig.timeMargin != 0) {
          if (newConfig->TimeMargin != 0) {
            rsConfig.timeMargin = newConfig->TimeMargin;
          } else {
            setChannelConfigNeeded = true;
          }
        }
        rsConfig.visualizationType = newConfig->VisualizationType;
        if (rsConfig.buttonsUpsideDown > 2) {
          rsConfig.buttonsUpsideDown = 1;
        }
        if (rsConfig.motorUpsideDown > 2) {
          rsConfig.motorUpsideDown = 1;
        }
        if (rsConfig.timeMargin < -1) {
          rsConfig.timeMargin = -1;
        }
        if (rsConfig.timeMargin > 101) {
          rsConfig.timeMargin = 101;
        }
        // cleanup tilt config for standard RS functions
        tiltConfig.clear();
        saveConfig();
        printConfig();
      }
      break;
    }
    case SUPLA_CHANNELFNC_CONTROLLINGTHEFACADEBLIND:
    case SUPLA_CHANNELFNC_VERTICAL_BLIND: {
      if (result->ConfigType == 0 &&
          result->ConfigSize == sizeof(TChannelConfig_FacadeBlind)) {
        auto newConfig =
            reinterpret_cast<TChannelConfig_FacadeBlind *>(result->Config);
        if (newConfig->OpeningTimeMS >= 0 && newConfig->ClosingTimeMS >= 0) {
          setOpenCloseTime(newConfig->ClosingTimeMS, newConfig->OpeningTimeMS);
        }
        if (newConfig->TiltingTimeMS >= 0) {
          setTiltingTime(newConfig->TiltingTimeMS, false);
        }
        tiltConfig.tilt0Angle = newConfig->Tilt0Angle;
        tiltConfig.tilt100Angle = newConfig->Tilt100Angle;
        tiltConfig.tiltControlType = newConfig->TiltControlType;

        if (!inMove()) {
          setTargetPosition(STOP_REQUEST);
        }
        if (rsConfig.buttonsUpsideDown != 0) {
          if (newConfig->ButtonsUpsideDown > 0) {
            rsConfig.buttonsUpsideDown = newConfig->ButtonsUpsideDown;
          } else {
            setChannelConfigNeeded = true;
          }
        }
        if (rsConfig.motorUpsideDown != 0) {
          if (newConfig->MotorUpsideDown > 0) {
            rsConfig.motorUpsideDown = newConfig->MotorUpsideDown;
          } else {
            setChannelConfigNeeded = true;
          }
        }
        if (rsConfig.timeMargin != 0) {
          if (newConfig->TimeMargin != 0) {
            rsConfig.timeMargin = newConfig->TimeMargin;
          } else {
            setChannelConfigNeeded = true;
          }
        }
        rsConfig.visualizationType = newConfig->VisualizationType;
        if (rsConfig.buttonsUpsideDown > 2) {
          rsConfig.buttonsUpsideDown = 1;
        }
        if (rsConfig.motorUpsideDown > 2) {
          rsConfig.motorUpsideDown = 1;
        }
        if (rsConfig.timeMargin < -1) {
          rsConfig.timeMargin = -1;
        }
        if (rsConfig.timeMargin > 101) {
          rsConfig.timeMargin = 101;
        }
        if (isTiltConfigured()) {
          if (currentTilt == UNKNOWN_POSITION) {
            setCurrentPosition(getCurrentPosition(), 0);
          }
          if (tiltConfig.tiltControlType ==
                  SUPLA_TILT_CONTROL_TYPE_TILTS_ONLY_WHEN_FULLY_CLOSED &&
              getCurrentPosition() < 100) {
            setCurrentPosition(getCurrentPosition(), 0);
          }
        } else {
          setCurrentPosition(getCurrentPosition(), UNKNOWN_POSITION);
        }
        saveConfig();
        printConfig();
      }
      break;
    }

    default: {
      SUPLA_LOG_WARNING("RS[%d] Ignoring unsupported channel function %d",
                        getChannelNumber(),
                        result->Func);
      break;
    }
  }
  return (setChannelConfigNeeded
              ? Supla::ApplyConfigResult::SetChannelConfigNeeded
              : Supla::ApplyConfigResult::Success);
}

void RollerShutterInterface::onLoadConfig(SuplaDeviceClass *) {
  auto cfg = Supla::Storage::ConfigInstance();
  bool print = false;
  if (cfg) {
    loadFunctionFromConfig();
    loadConfigChangeFlag();

    char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    // RollerShutter config is common for all roller shutter and facade blind
    // functions
    generateKey(key, Supla::ConfigTag::RollerShutterTag);
    RollerShutterConfig storedConfig = {};
    if (cfg->getBlob(key,
                     reinterpret_cast<char *>(&storedConfig),
                     sizeof(RollerShutterConfig))) {
      if (rsConfig.motorUpsideDown != 0 && storedConfig.motorUpsideDown != 0) {
        rsConfig.motorUpsideDown = storedConfig.motorUpsideDown;
      }
      if (rsConfig.buttonsUpsideDown != 0 &&
          storedConfig.buttonsUpsideDown != 0) {
        rsConfig.buttonsUpsideDown = storedConfig.buttonsUpsideDown;
      }
      if (rsConfig.timeMargin != 0 && storedConfig.timeMargin != 0) {
        rsConfig.timeMargin = storedConfig.timeMargin;
      }
      rsConfig.visualizationType = storedConfig.visualizationType;
      print = true;
    } else {
      SUPLA_LOG_DEBUG("RS[%d] using default config", getChannelNumber());
    }
    if (isTiltFunctionEnabled()) {
      generateKey(key, Supla::ConfigTag::TiltConfigTag);
      if (cfg->getBlob(
              key, reinterpret_cast<char *>(&tiltConfig), sizeof(TiltConfig))) {
        print = true;
      } else {
        SUPLA_LOG_DEBUG("FB[%d] using default config", getChannelNumber());
      }
    }

    if (print) {
      printConfig();
    }
  }
}

void RollerShutterInterface::printConfig() const {
  SUPLA_LOG_INFO(
      "RS[%d] rsConfig: motor: %s (%d), button: %s (%d), time "
      "margin: %d, visualization: %d",
      getChannelNumber(),
      rsConfig.motorUpsideDown == 2 ? "upside down" : "normal",
      rsConfig.motorUpsideDown,
      rsConfig.buttonsUpsideDown == 2 ? "upside down" : "normal",
      rsConfig.buttonsUpsideDown,
      rsConfig.timeMargin,
      rsConfig.visualizationType);
  if (isTiltFunctionEnabled()) {
    SUPLA_LOG_INFO(
        "FB[%d] tiltConfig: tiltingTime: %d, tilt0Angle: %d, tilt100Angle: "
        "%d, "
        "tiltControlType: %s (%d)",
        getChannelNumber(),
        tiltConfig.tiltingTime,
        tiltConfig.tilt0Angle,
        tiltConfig.tilt100Angle,
        tiltConfig.tiltControlType == SUPLA_TILT_CONTROL_TYPE_UNKNOWN
            ? "UNKNOWN"
        : tiltConfig.tiltControlType ==
                SUPLA_TILT_CONTROL_TYPE_CHANGES_POSITION_WHILE_TILTING
            ? "CHANGES_POSITION_WHILE_TILTING"
        : tiltConfig.tiltControlType ==
                SUPLA_TILT_CONTROL_TYPE_TILTS_ONLY_WHEN_FULLY_CLOSED
            ? "TILTS_ONLY_WHEN_FULLY_CLOSED"
        : tiltConfig.tiltControlType ==
                SUPLA_TILT_CONTROL_TYPE_STANDS_IN_POSITION_WHILE_TILTING
            ? "STANDS_IN_POSITION_WHILE_TILTING"
            : "UNKNOWN",
        tiltConfig.tiltControlType);
  }
}

void RollerShutterInterface::saveConfig() {
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    generateKey(key, Supla::ConfigTag::RollerShutterTag);
    if (cfg->setBlob(key,
                     reinterpret_cast<char *>(&rsConfig),
                     sizeof(RollerShutterConfig))) {
      cfg->saveWithDelay(2000);
    }
    if (isTiltFunctionEnabled()) {
      generateKey(key, Supla::ConfigTag::TiltConfigTag);
      if (cfg->setBlob(
              key, reinterpret_cast<char *>(&tiltConfig), sizeof(TiltConfig))) {
        cfg->saveWithDelay(2000);
      }
    }
  }
}

void RollerShutterInterface::setRsStorageSaveDelay(int delayMs) {
  rsStorageSaveDelay = delayMs;
}

void RollerShutterInterface::fillChannelConfig(void *channelConfig,
                                               int *size,
                                               uint8_t configType) {
  if (size) {
    *size = 0;
  } else {
    return;
  }

  if (channelConfig == nullptr) {
    return;
  }

  if (configType != SUPLA_CONFIG_TYPE_DEFAULT) {
    return;
  }

  switch (channel.getDefaultFunction()) {
    case SUPLA_CHANNELFNC_CONTROLLINGTHEROLLERSHUTTER:
    case SUPLA_CHANNELFNC_TERRACE_AWNING:
    case SUPLA_CHANNELFNC_ROLLER_GARAGE_DOOR:
    case SUPLA_CHANNELFNC_CURTAIN:
    case SUPLA_CHANNELFNC_PROJECTOR_SCREEN:
    case SUPLA_CHANNELFNC_CONTROLLINGTHEROOFWINDOW: {
      SUPLA_LOG_DEBUG("RS[%d] fill channel config for RS functions",
                      channel.getChannelNumber());

      auto config =
          reinterpret_cast<TChannelConfig_RollerShutter *>(channelConfig);
      *size = sizeof(TChannelConfig_RollerShutter);
      config->ButtonsUpsideDown = rsConfig.buttonsUpsideDown;
      config->MotorUpsideDown = rsConfig.motorUpsideDown;
      config->TimeMargin = rsConfig.timeMargin;
      config->VisualizationType = rsConfig.visualizationType;
      config->OpeningTimeMS = openingTimeMs;
      config->ClosingTimeMS = closingTimeMs;
      break;
    }
    case SUPLA_CHANNELFNC_CONTROLLINGTHEFACADEBLIND:
    case SUPLA_CHANNELFNC_VERTICAL_BLIND: {
      SUPLA_LOG_DEBUG("RS[%d] fill channel config for FB functions",
                      channel.getChannelNumber());

      auto config =
          reinterpret_cast<TChannelConfig_FacadeBlind *>(channelConfig);
      *size = sizeof(TChannelConfig_FacadeBlind);
      config->ButtonsUpsideDown = rsConfig.buttonsUpsideDown;
      config->MotorUpsideDown = rsConfig.motorUpsideDown;
      config->TimeMargin = rsConfig.timeMargin;
      config->VisualizationType = rsConfig.visualizationType;
      config->OpeningTimeMS = openingTimeMs;
      config->ClosingTimeMS = closingTimeMs;
      config->TiltingTimeMS = tiltConfig.tiltingTime;
      config->Tilt0Angle = tiltConfig.tilt0Angle;
      config->Tilt100Angle = tiltConfig.tilt100Angle;
      config->TiltControlType = tiltConfig.tiltControlType;
      break;
    }
    default:
      SUPLA_LOG_WARNING("RS[%d] fill channel config for unknown function %d",
                        channel.getChannelNumber(),
                        channel.getDefaultFunction());
      return;
  }
}

void RollerShutterInterface::setRsConfigMotorUpsideDownEnabled(bool enable) {
  if (enable) {
    if (rsConfig.motorUpsideDown == 0) {
      rsConfig.motorUpsideDown = 1;
    }
  } else {
    rsConfig.motorUpsideDown = 0;
  }
}

void RollerShutterInterface::setRsConfigButtonsUpsideDownEnabled(bool enable) {
  if (enable) {
    if (rsConfig.buttonsUpsideDown == 0) {
      rsConfig.buttonsUpsideDown = 1;
    }
  } else {
    rsConfig.buttonsUpsideDown = 0;
  }
}

void RollerShutterInterface::setRsConfigTimeMarginEnabled(bool enable) {
  if (enable) {
    if (rsConfig.timeMargin == 0) {
      rsConfig.timeMargin = -1;
    }
  } else {
    rsConfig.timeMargin = 0;
  }
}

uint32_t RollerShutterInterface::getTimeMarginValue(uint32_t fullTime) const {
  if (fullTime == 0) {
    return RS_DEFAULT_OPERATION_TIMEOUT_MS;
  }
  if (rsConfig.timeMargin <= 0) {
    // case for -1 (device specific) and 0 (not used)
    return fullTime * 0.3;
  }
  if (rsConfig.timeMargin == 1) {
    return 50;
  }
  uint32_t margin = fullTime * ((rsConfig.timeMargin - 1) / 100.0);
  if (margin < 50) {
    return 50;
  }
  return margin;
}

void RollerShutterInterface::fillSuplaChannelNewValue(
    TSD_SuplaChannelNewValue *value) {
  if (value == nullptr) {
    return;
  }

  value->DurationMS = ((closingTimeMs / 100) & 0xFFFF) |
                      (((openingTimeMs / 100) & 0xFFFF) << 16);
}

uint8_t RollerShutterInterface::getMotorUpsideDown() const {
  return rsConfig.motorUpsideDown;
}

uint8_t RollerShutterInterface::getButtonsUpsideDown() const {
  return rsConfig.buttonsUpsideDown;
}

int8_t RollerShutterInterface::getTimeMargin() const {
  return rsConfig.timeMargin;
}

bool RollerShutterInterface::isAutoCalibrationSupported() const {
  return false;
  // TODO(klew): implement
}

void RollerShutterInterface::setRsConfigMotorUpsideDownValue(uint8_t value) {
  if (getMotorUpsideDown() != 0 && value > 0 && value < 3) {
    rsConfig.motorUpsideDown = value;
    saveConfig();
  }
}

void RollerShutterInterface::setRsConfigButtonsUpsideDownValue(uint8_t value) {
  if (getButtonsUpsideDown() != 0 && value > 0 && value < 3) {
    rsConfig.buttonsUpsideDown = value;
    saveConfig();
  }
}

void RollerShutterInterface::setRsConfigTimeMarginValue(int8_t value) {
  if (value >= -1 && value <= 100) {
    rsConfig.timeMargin = value;
    saveConfig();
  }
}

void RollerShutterInterface::purgeConfig() {
  Supla::ChannelElement::purgeConfig();
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    generateKey(key, Supla::ConfigTag::RollerShutterTag);
    cfg->eraseKey(key);

    generateKey(key, Supla::ConfigTag::TiltConfigTag);
    cfg->eraseKey(key);
  }
}

bool RollerShutterInterface::isTimeSettingAvailable() const {
  return (channel.getFlags() & SUPLA_CHANNEL_FLAG_TIME_SETTING_NOT_AVAILABLE) ==
         0;
}

bool RollerShutterInterface::isCalibrationFailed() const {
  return flags & RS_FLAG_CALIBRATION_FAILED;
}

bool RollerShutterInterface::isCalibrationLost() const {
  return flags & RS_FLAG_CALIBRATION_LOST;
}

bool RollerShutterInterface::isMotorProblem() const {
  return flags & RS_FLAG_MOTOR_PROBLEM;
}

bool RollerShutterInterface::getCalibrate() const {
  return flags & RS_FLAG_CALIBRATE;
}

void RollerShutterInterface::setCalibrate(bool value) {
  flags = value ? flags | RS_FLAG_CALIBRATE : flags & ~RS_FLAG_CALIBRATE;
}

void RollerShutterInterface::setCalibrationFailed(bool value) {
  flags = value ? flags | RS_FLAG_CALIBRATION_FAILED
                : flags & ~RS_FLAG_CALIBRATION_FAILED;
}

void RollerShutterInterface::setCalibrationLost(bool value) {
  flags = value ? flags | RS_FLAG_CALIBRATION_LOST
                : flags & ~RS_FLAG_CALIBRATION_LOST;
}

void RollerShutterInterface::setMotorProblem(bool value) {
  flags =
      value ? flags | RS_FLAG_MOTOR_PROBLEM : flags & ~RS_FLAG_MOTOR_PROBLEM;
}

void RollerShutterInterface::setupButtonActions(Supla::Control::Button *button,
                                                bool upDirection,
                                                bool asInternal) {
  if (!button) {
    return;
  }

  if (upDirection) {
    button->onInit();  // make sure button was initialized
    if (button->isMonostable()) {
      if (!isTiltFunctionsSupported()) {
        button->addAction(asInternal ? Supla::INTERNAL_BUTTON_MOVE_UP_OR_STOP
                                     : Supla::MOVE_UP_OR_STOP,
                          this,
                          Supla::CONDITIONAL_ON_PRESS);
      } else {
        button->addAction(asInternal ? Supla::INTERNAL_BUTTON_MOVE_UP_OR_STOP
                                     : Supla::MOVE_UP_OR_STOP,
                          this,
                          Supla::ON_CLICK_1);
        button->addAction(
            asInternal ? Supla::INTERNAL_BUTTON_MOVE_UP : Supla::MOVE_UP,
            this,
            Supla::ON_HOLD);
        button->addAction(Supla::STOP, this, Supla::ON_HOLD_RELEASE);
      }
    } else if (button->isBistable()) {
      button->addAction(asInternal ? Supla::INTERNAL_BUTTON_MOVE_UP_OR_STOP
                                   : Supla::MOVE_UP_OR_STOP,
                        this,
                        Supla::CONDITIONAL_ON_CHANGE);
    } else if (button->isCentral()) {
      button->addAction(Supla::OPEN, this, Supla::ON_PRESS);
    }
  } else {             // down direction
    button->onInit();  // make sure button was initialized
    if (button->isMonostable()) {
      if (!isTiltFunctionsSupported()) {
        button->addAction(asInternal ? Supla::INTERNAL_BUTTON_MOVE_DOWN_OR_STOP
                                     : Supla::MOVE_DOWN_OR_STOP,
                          this,
                          Supla::CONDITIONAL_ON_PRESS);
      } else {
        button->addAction(asInternal ? Supla::INTERNAL_BUTTON_MOVE_DOWN_OR_STOP
                                     : Supla::MOVE_DOWN_OR_STOP,
                          this,
                          Supla::ON_CLICK_1);
        button->addAction(
            asInternal ? Supla::INTERNAL_BUTTON_MOVE_DOWN : Supla::MOVE_DOWN,
            this,
            Supla::ON_HOLD);
        button->addAction(Supla::STOP, this, Supla::ON_HOLD_RELEASE);
      }
    } else if (button->isBistable()) {
      button->addAction(asInternal ? Supla::INTERNAL_BUTTON_MOVE_DOWN_OR_STOP
                                   : Supla::MOVE_DOWN_OR_STOP,
                        this,
                        Supla::CONDITIONAL_ON_CHANGE);
    } else if (button->isCentral()) {
      button->addAction(Supla::CLOSE, this, Supla::ON_PRESS);
    }
  }
}

void RollerShutterInterface::addTiltFunctions() {
  channel.setFuncList(channel.getFuncList() |
                      SUPLA_BIT_FUNC_CONTROLLINGTHEFACADEBLIND |
                      SUPLA_BIT_FUNC_VERTICAL_BLIND);
}

bool RollerShutterInterface::isTiltFunctionsSupported() const {
  return channel.getFuncList() & (SUPLA_BIT_FUNC_CONTROLLINGTHEFACADEBLIND |
                                  SUPLA_BIT_FUNC_VERTICAL_BLIND);
}

bool RollerShutterInterface::isTiltFunctionEnabled() const {
  auto function = channel.getDefaultFunction();
  return function == SUPLA_CHANNELFNC_CONTROLLINGTHEFACADEBLIND ||
         function == SUPLA_CHANNELFNC_VERTICAL_BLIND;
}

void Supla::Control::TiltConfig::clear() {
  tiltingTime = 0;
  tilt0Angle = 0;
  tilt100Angle = 0;
  tiltControlType = SUPLA_TILT_CONTROL_TYPE_UNKNOWN;
}

bool RollerShutterInterface::isTiltConfigured() const {
  return isTiltFunctionEnabled() && tiltConfig.tiltingTime > 0 &&
         tiltConfig.tiltControlType != 0;
}

bool RollerShutterInterface::isTopReached() const {
  bool posTop = (currentPosition == 0);
  bool tiltTop = !isTiltFunctionEnabled();
  if (!tiltTop) {
    tiltTop = isTiltConfigured() && (currentTilt == 0);
  }
  return posTop && tiltTop;
}

bool RollerShutterInterface::isBottomReached() const {
  bool posBottom = (currentPosition == 10000);
  bool tiltBottom = !isTiltFunctionEnabled();
  if (!tiltBottom) {
    tiltBottom = isTiltConfigured() && (currentTilt == 10000);
  }
  return posBottom && tiltBottom;
}

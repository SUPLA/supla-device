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

#include "roller_shutter.h"

#include <supla/log_wrapper.h>

#include <supla/storage/storage.h>
#include <supla/time.h>
#include <supla/io.h>
#include <supla/control/button.h>
#include <supla/storage/config.h>
#include <supla/storage/config_tags.h>

#include "../actions.h"
#include "supla/local_action.h"

namespace Supla {
namespace Control {

int16_t RollerShutter::rsStorageSaveDelay = 5000;

#define RS_DEFAULT_OPERATION_TIMEOUT_MS 60000

#pragma pack(push, 1)
struct RollerShutterStateData {
  uint32_t closingTimeMs;
  uint32_t openingTimeMs;
  int8_t currentPosition;  // 0 - closed; 100 - opened
};
#pragma pack(pop)

RollerShutter::RollerShutter(Supla::Io *io,
                             int pinUp,
                             int pinDown,
                             bool highIsOn)
    : RollerShutter(pinUp, pinDown, highIsOn) {
  this->io = io;
}

RollerShutter::RollerShutter(int pinUp, int pinDown, bool highIsOn)
    : pinUp(pinUp), pinDown(pinDown), highIsOn(highIsOn) {
  channel.setType(SUPLA_CHANNELTYPE_RELAY);
  channel.setDefault(SUPLA_CHANNELFNC_CONTROLLINGTHEROLLERSHUTTER);
  channel.setFuncList(SUPLA_BIT_FUNC_CONTROLLINGTHEROLLERSHUTTER |
                      SUPLA_BIT_FUNC_CONTROLLINGTHEROOFWINDOW |
                      SUPLA_BIT_FUNC_TERRACE_AWNING |
                      SUPLA_BIT_FUNC_ROLLER_GARAGE_DOOR |
                      SUPLA_BIT_FUNC_CURTAIN |
                      SUPLA_BIT_FUNC_PROJECTOR_SCREEN);
  channel.setFlag(SUPLA_CHANNEL_FLAG_RS_SBS_AND_STOP_ACTIONS);
  channel.setFlag(SUPLA_CHANNEL_FLAG_CALCFG_RECALIBRATE);
  channel.setFlag(SUPLA_CHANNEL_FLAG_RUNTIME_CHANNEL_CONFIG_UPDATE);
}

bool RollerShutter::isFunctionSupported(int32_t channelFunction) const {
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
      return getChannel()->getFuncList() &
             SUPLA_BIT_FUNC_ROLLER_GARAGE_DOOR;
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

void RollerShutter::onInit() {
  Supla::Io::digitalWrite(
      channel.getChannelNumber(), pinUp, highIsOn ? LOW : HIGH, io);
  Supla::Io::digitalWrite(
      channel.getChannelNumber(), pinDown, highIsOn ? LOW : HIGH, io);
  Supla::Io::pinMode(channel.getChannelNumber(), pinUp, OUTPUT, io);
  Supla::Io::pinMode(channel.getChannelNumber(), pinDown, OUTPUT, io);

  setupButtonActions();
}

void RollerShutter::setupButtonActions() {
  if (upButton) {
    upButton->onInit();  // make sure button was initialized
    if (upButton->isMonostable()) {
      upButton->addAction(Supla::INTERNAL_BUTTON_MOVE_UP_OR_STOP,
                          this,
                          Supla::CONDITIONAL_ON_PRESS);
    } else if (upButton->isBistable()) {
      upButton->addAction(Supla::INTERNAL_BUTTON_MOVE_UP_OR_STOP,
                          this,
                          Supla::CONDITIONAL_ON_CHANGE);
    }
  }

  if (downButton) {
    downButton->onInit();  // make sure button was initialized
    if (downButton->isMonostable()) {
      downButton->addAction(Supla::INTERNAL_BUTTON_MOVE_DOWN_OR_STOP,
                            this,
                            Supla::CONDITIONAL_ON_PRESS);
    } else if (downButton->isBistable()) {
      downButton->addAction(Supla::INTERNAL_BUTTON_MOVE_DOWN_OR_STOP,
                            this,
                            Supla::CONDITIONAL_ON_CHANGE);
    }
  }
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

int32_t RollerShutter::handleNewValueFromServer(
    TSD_SuplaChannelNewValue *newValue) {
  uint32_t newClosingTime = (newValue->DurationMS & 0xFFFF) * 100;
  uint32_t newOpeningTime = ((newValue->DurationMS >> 16) & 0xFFFF) * 100;

  setOpenCloseTime(newClosingTime, newOpeningTime);

  int8_t task = newValue->value[0];
  int8_t tilt = newValue->value[1];
  SUPLA_LOG_DEBUG("RS[%d] new value from server: position/task %d, tilt %d",
      channel.getChannelNumber(), task, tilt);
  // TODO(klew): add tilt support
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
      } else if (currentPosition < 50) {
        moveDown();
      } else {
        moveUp();
      }
      break;
    }

    default: {
      if (task >= 10 && task <= 110) {
        setTargetPosition(task - 10);
      }
      break;
    }
  }
  return -1;
}

void RollerShutter::setOpenCloseTime(uint32_t newClosingTimeMs,
                                     uint32_t newOpeningTimeMs) {
  if (newClosingTimeMs != closingTimeMs || newOpeningTimeMs != openingTimeMs) {
    closingTimeMs = newClosingTimeMs;
    openingTimeMs = newOpeningTimeMs;
    setCalibrationNeeded();
    SUPLA_LOG_DEBUG(
        "RS[%d] new time settings received. Opening time: %d ms; "
        "closing time: %d ms. Starting calibration...",
        channel.getChannelNumber(),
        openingTimeMs,
        closingTimeMs);
  }
}

void RollerShutter::handleAction(int event, int action) {
  (void)(event);
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
      setTargetPosition(comfortDownValue);
      break;
    }

    case COMFORT_UP_POSITION: {
      setTargetPosition(comfortUpValue);
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
      } else if (currentPosition < 50) {
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
  }
}

void RollerShutter::close() {
  setTargetPosition(100);
}

void RollerShutter::open() {
  setTargetPosition(0);
}

void RollerShutter::moveDown() {
  setTargetPosition(MOVE_DOWN_POSITION);
}

void RollerShutter::moveUp() {
  setTargetPosition(MOVE_UP_POSITION);
}

void RollerShutter::stop() {
  setTargetPosition(STOP_POSITION);
}

void RollerShutter::setTargetPosition(int newPosition) {
  targetPosition = newPosition;
  newTargetPositionAvailable = true;

  // Negative targetPosition is either unknown or stop command, so we
  // ignore it
  if (targetPosition == MOVE_UP_POSITION) {
    lastDirection = UP_DIR;
  } else if (targetPosition == MOVE_DOWN_POSITION) {
    lastDirection = DOWN_DIR;
  } else if (targetPosition >= 0) {
    if (targetPosition < currentPosition) {
      lastDirection = UP_DIR;
    } else if (targetPosition > currentPosition) {
      lastDirection = DOWN_DIR;
    }
  }
}

bool RollerShutter::lastDirectionWasOpen() const {
  return lastDirection == UP_DIR;
}

bool RollerShutter::lastDirectionWasClose() const {
  return lastDirection == DOWN_DIR;
}

bool RollerShutter::inMove() {
  return currentDirection != STOP_DIR;
}

void RollerShutter::stopMovement() {
  switchOffRelays();
  currentDirection = STOP_DIR;
  doNothingTime = millis();
  // Schedule save in 5 s after stop movement of roller shutter
  Supla::Storage::ScheduleSave(rsStorageSaveDelay);
}

void RollerShutter::relayDownOn() {
  Supla::Io::digitalWrite(channel.getChannelNumber(),
                          rsConfig.motorUpsideDown == 2 ? pinUp : pinDown,
                          highIsOn ? HIGH : LOW,
                          io);
}

void RollerShutter::relayUpOn() {
  Supla::Io::digitalWrite(channel.getChannelNumber(),
                          rsConfig.motorUpsideDown == 2 ? pinDown : pinUp,
                          highIsOn ? HIGH : LOW,
                          io);
}

void RollerShutter::relayDownOff() {
  Supla::Io::digitalWrite(channel.getChannelNumber(),
                          rsConfig.motorUpsideDown == 2 ? pinUp : pinDown,
                          highIsOn ? LOW : HIGH,
                          io);
}

void RollerShutter::relayUpOff() {
  Supla::Io::digitalWrite(channel.getChannelNumber(),
                          rsConfig.motorUpsideDown == 2 ? pinDown : pinUp,
                          highIsOn ? LOW : HIGH,
                          io);
}

void RollerShutter::startClosing() {
  lastMovementStartTime = millis();
  currentDirection = DOWN_DIR;
  relayUpOff();  // just to make sure
  relayDownOn();
}

void RollerShutter::startOpening() {
  lastMovementStartTime = millis();
  currentDirection = UP_DIR;
  relayDownOff();  // just to make sure
  relayUpOn();
}

void RollerShutter::switchOffRelays() {
  relayUpOff();
  relayDownOff();
}

void RollerShutter::triggerCalibration() {
  setCalibrationNeeded();
  setTargetPosition(0);
}

void RollerShutter::setCalibrationNeeded() {
  calibrate = true;
  currentPosition = UNKNOWN_POSITION;
}

bool RollerShutter::isCalibrationRequested() const {
  return calibrate && openingTimeMs != 0 && closingTimeMs != 0;
}

bool RollerShutter::isCalibrated() const {
  return calibrate == false && openingTimeMs != 0 && closingTimeMs != 0;
}

void RollerShutter::onTimer() {
  if (doNothingTime != 0 && millis() - doNothingTime <
      500) {  // doNothingTime time is used when we change
              // direction of roller - to stop for a moment
              // before enabling opposite direction
    return;
  }
  doNothingTime = 0;

  if (targetPosition == STOP_POSITION && inMove()) {
    stopMovement();
    calibrationTime = 0;
    SUPLA_LOG_DEBUG("RS[%d]: Stop movement", channel.getChannelNumber());
  }
  if (targetPosition == STOP_POSITION) {
    newTargetPositionAvailable = false;
  }

  if (isCalibrationRequested()) {
    if (targetPosition != STOP_POSITION) {
      // If calibrationTime is not set, then it means we should start
      // calibration
      if (calibrationTime == 0) {
        // If roller shutter wasn't in move when calibration is requested, we
        // select direction based on requested targetPosition
        operationTimeoutMs = 0;
        if (targetPosition > 50 || targetPosition == MOVE_DOWN_POSITION) {
          if (currentDirection == UP_DIR) {
            stopMovement();
          } else if (currentDirection == STOP_DIR ||
                     currentDirection == DOWN_DIR) {
            SUPLA_LOG_DEBUG("RS[%d]: Calibration: closing",
                            channel.getChannelNumber());
            calibrationTime = closingTimeMs;
            if (calibrationTime == 0) {
              operationTimeoutMs = RS_DEFAULT_OPERATION_TIMEOUT_MS;
            }
            startClosing();
          }
        } else {
          if (currentDirection == DOWN_DIR) {
            stopMovement();
          } else if (currentDirection == STOP_DIR ||
                     currentDirection == UP_DIR) {
            SUPLA_LOG_DEBUG("RS[%d]: Calibration: opening",
                            channel.getChannelNumber());
            calibrationTime = openingTimeMs;
            if (calibrationTime == 0) {
              operationTimeoutMs = RS_DEFAULT_OPERATION_TIMEOUT_MS;
            }
            startOpening();
          }
        }
        //
        // Time used for calibaration is 10% higher then requested by user
        calibrationTime *= 1.1;
        if (calibrationTime > 0) {
          SUPLA_LOG_DEBUG("RS[%d]: Calibration time: %d",
                          channel.getChannelNumber(),
                          calibrationTime);
        }
      }

      if (calibrationTime != 0 &&
          millis() - lastMovementStartTime > calibrationTime) {
        SUPLA_LOG_DEBUG("RS[%d]: Calibration done", channel.getChannelNumber());
        calibrationTime = 0;
        calibrate = false;
        if (currentDirection == UP_DIR) {
          currentPosition = 0;
        } else {
          currentPosition = 100;
        }
        if (targetPosition < 0) {
          setTargetPosition(STOP_POSITION);
        }
        stopMovement();
      }
    }
  } else if (isCalibrated()) {
    if (!newTargetPositionAvailable && currentDirection != STOP_DIR) {
      // no new command available and it is moving, just handle movement/status
      if (currentDirection == UP_DIR && currentPosition > 0) {
        int movementDistance = lastPositionBeforeMovement;
        uint32_t timeRequired =
            (1.0 * openingTimeMs * movementDistance / 100.0);
        float fractionOfMovemendDone =
            (1.0 * (millis() - lastMovementStartTime) / timeRequired);
        if (fractionOfMovemendDone > 1) {
          fractionOfMovemendDone = 1;
        }
        currentPosition = lastPositionBeforeMovement -
                          movementDistance * fractionOfMovemendDone;
        if (targetPosition >= 0 && currentPosition <= targetPosition) {
          if (targetPosition == 0) {
            operationTimeoutMs = getTimeMarginValue(openingTimeMs);
            lastMovementStartTime = millis();
            SUPLA_LOG_DEBUG("RS[%d]: operation timeout: %d",
                            channel.getChannelNumber(),
                            operationTimeoutMs);
          } else {
            stopMovement();
          }
        }
      } else if (currentDirection == DOWN_DIR && currentPosition < 100) {
        int movementDistance = 100 - lastPositionBeforeMovement;
        uint32_t timeRequired =
            (1.0 * closingTimeMs * movementDistance / 100.0);
        float fractionOfMovemendDone =
            (1.0 * (millis() - lastMovementStartTime) / timeRequired);
        if (fractionOfMovemendDone > 1) {
          fractionOfMovemendDone = 1;
        }
        currentPosition = lastPositionBeforeMovement +
                          movementDistance * fractionOfMovemendDone;
        if (targetPosition >= 0 && currentPosition >= targetPosition) {
          if (targetPosition == 100) {
            operationTimeoutMs = getTimeMarginValue(closingTimeMs);
            lastMovementStartTime = millis();
            SUPLA_LOG_DEBUG("RS[%d]: operation timeout: %d",
                            channel.getChannelNumber(),
                            operationTimeoutMs);
          } else {
            stopMovement();
          }
        }
      }

      if (currentPosition > 100) {
        currentPosition = 100;
      } else if (currentPosition < 0) {
        currentPosition = 0;
      }

    } else if (newTargetPositionAvailable && targetPosition != STOP_POSITION) {
      // new target state was set, let's handle it
      int newDirection = STOP_DIR;
      if (targetPosition == MOVE_UP_POSITION) {
        newDirection = UP_DIR;
        operationTimeoutMs = openingTimeMs + getTimeMarginValue(openingTimeMs);
        SUPLA_LOG_DEBUG("RS[%d]: Set new direction: UP, operation timeout: %d",
                        channel.getChannelNumber(),
                        operationTimeoutMs);
      } else if (targetPosition == MOVE_DOWN_POSITION) {
        newDirection = DOWN_DIR;
        operationTimeoutMs = closingTimeMs + getTimeMarginValue(closingTimeMs);
        SUPLA_LOG_DEBUG(
            "RS[%d]: Set new direction: DOWN, operation timeout: %d",
            channel.getChannelNumber(),
            operationTimeoutMs);
      } else {
        operationTimeoutMs = 0;
        int newMovementValue = targetPosition - currentPosition;
        // 0 - 100 = -100 (move down); 50 -
        // 20 = 30 (move up 30%), etc
        if (newMovementValue > 0) {
          newDirection = DOWN_DIR;  // move down
        } else if (newMovementValue < 0) {
          newDirection = UP_DIR;  // move up
        }
      }
      // If new direction is the same as current move, then keep movin`
      if (newDirection == currentDirection) {
        newTargetPositionAvailable = false;
      } else if (currentDirection == STOP_DIR) {  // else start moving
        newTargetPositionAvailable = false;
        lastPositionBeforeMovement = currentPosition;
        if (newDirection == DOWN_DIR) {
          startClosing();
        } else {
          startOpening();
        }
      } else {  // else stop before changing direction
        stopMovement();
      }
    }
  } else {
    // RS is not calibrated
    currentPosition = UNKNOWN_POSITION;
    if (newTargetPositionAvailable) {
      int newDirection = STOP_DIR;
      operationTimeoutMs = RS_DEFAULT_OPERATION_TIMEOUT_MS;
      if (targetPosition == MOVE_UP_POSITION) {
        newDirection = UP_DIR;
      } else if (targetPosition == MOVE_DOWN_POSITION) {
        newDirection = DOWN_DIR;
      } else {
        // 0 - 100 = -100 (move down); 50 -
        // 20 = 30 (move up 30%), etc
        if (targetPosition == 0) {
          newDirection = UP_DIR;  // move up
        } else if (targetPosition == 100) {
          newDirection = DOWN_DIR;  // move down
        }
      }
      // If new direction is the same as current move, then keep movin`
      if (newDirection == currentDirection) {
        newTargetPositionAvailable = false;
      } else if (currentDirection == STOP_DIR) {  // else start moving
        newTargetPositionAvailable = false;
        lastPositionBeforeMovement = currentPosition;
        if (newDirection == DOWN_DIR) {
          startClosing();
        } else {
          startOpening();
        }
      } else {  // else stop before changing direction
        stopMovement();
        operationTimeoutMs = 0;
      }
    }
  }

  if (operationTimeoutMs != 0 &&
      millis() - lastMovementStartTime > operationTimeoutMs) {
    setTargetPosition(STOP_POSITION);
    operationTimeoutMs = 0;
    SUPLA_LOG_DEBUG("RS[%d]: Operation timeout", channel.getChannelNumber());
  }


  // if (newCurrentPosition != currentPosition) {
  // currentPosition = newCurrentPosition;
  TDSC_RollerShutterValue value = {};
  value.position = currentPosition;
  if (calibrationTime != 0) {
    value.flags |= RS_VALUE_FLAG_CALIBRATION_IN_PROGRESS;
  }
  channel.setNewValue(value);
}

void RollerShutter::configComfortUpValue(uint8_t position) {
  comfortUpValue = position;
  if (comfortUpValue > 100) {
    comfortUpValue = 100;
  }
}

void RollerShutter::configComfortDownValue(uint8_t position) {
  comfortDownValue = position;
  if (comfortDownValue > 100) {
    comfortDownValue = 100;
  }
}

void RollerShutter::onLoadState() {
  RollerShutterStateData data = RollerShutterStateData();
  if (Supla::Storage::ReadState((unsigned char *)&data, sizeof(data))) {
    closingTimeMs = data.closingTimeMs;
    openingTimeMs = data.openingTimeMs;
    currentPosition = data.currentPosition;
    if (currentPosition >= 0) {
      calibrate = false;
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

void RollerShutter::onSaveState() {
  RollerShutterStateData data;
  data.closingTimeMs = closingTimeMs;
  data.openingTimeMs = openingTimeMs;
  data.currentPosition = currentPosition;

  Supla::Storage::WriteState((unsigned char *)&data, sizeof(data));
}

int RollerShutter::getCurrentPosition() const {
  return currentPosition;
}

int RollerShutter::getCurrentDirection() const {
  return currentDirection;
}

uint32_t RollerShutter::getClosingTimeMs() const {
  return closingTimeMs;
}

uint32_t RollerShutter::getOpeningTimeMs() const {
  return openingTimeMs;
}

void RollerShutter::attach(Supla::Control::Button *up,
    Supla::Control::Button *down) {
  upButton = up;
  downButton = down;
}

int RollerShutter::handleCalcfgFromServer(TSD_DeviceCalCfgRequest *request) {
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

uint8_t RollerShutter::applyChannelConfig(TSD_ChannelConfig *result, bool) {
  SUPLA_LOG_DEBUG(
      "RS[%d]:applyChannelConfig, func %d, configtype %d, configsize %d",
      getChannelNumber(),
      result->Func,
      result->ConfigType,
      result->ConfigSize);

  if (result->ConfigSize == 0) {
    return SUPLA_CONFIG_RESULT_TRUE;
  }

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
          setTargetPosition(STOP_POSITION);
        }
        if (rsConfig.buttonsUpsideDown != 0) {
          rsConfig.buttonsUpsideDown = newConfig->ButtonsUpsideDown;
        }
        if (rsConfig.motorUpsideDown != 0) {
          rsConfig.motorUpsideDown = newConfig->MotorUpsideDown;
        }
        if (rsConfig.timeMargin != 0) {
          rsConfig.timeMargin = newConfig->TimeMargin;
        }
        rsConfig.visualizationType = newConfig->VisualizationType;
        if (rsConfig.buttonsUpsideDown > 2) {
          rsConfig.buttonsUpsideDown = 1;
        }
        if (rsConfig.motorUpsideDown > 2) {
          rsConfig.motorUpsideDown = 1;
        }
        if (rsConfig.timeMargin < -1 || rsConfig.timeMargin == 0) {
          rsConfig.timeMargin = -1;
        }
        if (rsConfig.timeMargin > 101) {
          rsConfig.timeMargin = 101;
        }
        saveConfig();
        printConfig();
        return SUPLA_CONFIG_RESULT_TRUE;
      }
      break;
    }

    default: {
      SUPLA_LOG_WARNING("RS[%d]: Ignoring unsupported channel function %d",
                        getChannelNumber(), result->Func);
      break;
    }
  }
  return SUPLA_CONFIG_RESULT_TRUE;
}

void RollerShutter::onLoadConfig(SuplaDeviceClass *) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    loadFunctionFromConfig();

    char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    generateKey(key, Supla::ConfigTag::RollerShutterTag);
    RollerShutterConfig storedConfig = {};
    if (cfg->getBlob(key,
                     reinterpret_cast<char *>(&storedConfig),
                     sizeof(RollerShutterConfig))) {
      if (rsConfig.motorUpsideDown != 0) {
        rsConfig.motorUpsideDown = storedConfig.motorUpsideDown;
      }
      if (rsConfig.buttonsUpsideDown != 0) {
        rsConfig.buttonsUpsideDown = storedConfig.buttonsUpsideDown;
      }
      if (rsConfig.timeMargin != 0) {
        rsConfig.timeMargin = storedConfig.timeMargin;
      }
      rsConfig.visualizationType = storedConfig.visualizationType;
      printConfig();
    } else {
      SUPLA_LOG_DEBUG("RS[%d]: using default config", getChannelNumber());
    }
  }
}

void RollerShutter::printConfig() const {
  SUPLA_LOG_INFO(
      "RS[%d]: rsConfig: motor: %s (%d), button: %s (%d), time "
      "margin: %d, visualization: %d",
      getChannelNumber(),
      rsConfig.motorUpsideDown == 2 ? "upside down" : "normal",
      rsConfig.motorUpsideDown,
      rsConfig.buttonsUpsideDown == 2 ? "upside down" : "normal",
      rsConfig.buttonsUpsideDown,
      rsConfig.timeMargin,
      rsConfig.visualizationType);
}

void RollerShutter::saveConfig() {
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    generateKey(key, Supla::ConfigTag::RollerShutterTag);
    if (cfg->setBlob(key,
                     reinterpret_cast<char *>(&rsConfig),
                     sizeof(RollerShutterConfig))) {
      cfg->saveWithDelay(2000);
    }
  }
}

void RollerShutter::setRsStorageSaveDelay(int delayMs) {
  rsStorageSaveDelay = delayMs;
}

void RollerShutter::fillChannelConfig(void *channelConfig, int *size) {
  if (size) {
    *size = 0;
  } else {
    return;
  }
  if (channelConfig == nullptr) {
    return;
  }

  switch (channel.getDefaultFunction()) {
    case SUPLA_CHANNELFNC_CONTROLLINGTHEROLLERSHUTTER:
    case SUPLA_CHANNELFNC_TERRACE_AWNING:
    case SUPLA_CHANNELFNC_ROLLER_GARAGE_DOOR:
    case SUPLA_CHANNELFNC_CURTAIN:
    case SUPLA_CHANNELFNC_PROJECTOR_SCREEN:
    case SUPLA_CHANNELFNC_CONTROLLINGTHEROOFWINDOW: {
      SUPLA_LOG_DEBUG(
          "Relay[%d]: fill channel config for RS functions",
          channel.getChannelNumber());

      auto config = reinterpret_cast<TChannelConfig_RollerShutter *>(
          channelConfig);
      *size = sizeof(TChannelConfig_RollerShutter);
      config->ButtonsUpsideDown = rsConfig.buttonsUpsideDown;
      config->MotorUpsideDown = rsConfig.motorUpsideDown;
      config->TimeMargin = rsConfig.timeMargin;
      config->VisualizationType = rsConfig.visualizationType;
      config->OpeningTimeMS = openingTimeMs;
      config->ClosingTimeMS = closingTimeMs;
      break;
    }
    default:
      SUPLA_LOG_WARNING(
          "RS[%d]: fill channel config for unknown function %d",
          channel.getChannelNumber(),
          channel.getDefaultFunction());
      return;
  }
}

void RollerShutter::setRsConfigMotorUpsideDownEnabled(bool enable) {
  if (enable) {
    if (rsConfig.motorUpsideDown == 0) {
      rsConfig.motorUpsideDown = 1;
    }
  } else {
    rsConfig.motorUpsideDown = 0;
  }
}

void RollerShutter::setRsConfigButtonsUpsideDownEnabled(bool enable) {
  if (enable) {
    if (rsConfig.buttonsUpsideDown == 0) {
      rsConfig.buttonsUpsideDown = 1;
    }
  } else {
    rsConfig.buttonsUpsideDown = 0;
  }
}

void RollerShutter::setRsConfigTimeMarginEnabled(bool enable) {
  if (enable) {
    if (rsConfig.timeMargin == 0) {
      rsConfig.timeMargin = -1;
    }
  } else {
    rsConfig.timeMargin = 0;
  }
}

uint32_t RollerShutter::getTimeMarginValue(uint32_t fullTime) const {
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

void RollerShutter::fillSuplaChannelNewValue(TSD_SuplaChannelNewValue *value) {
  if (value == nullptr) {
    return;
  }

  value->DurationMS = ((closingTimeMs / 100) & 0xFFFF) |
                      (((openingTimeMs / 100) & 0xFFFF) << 16);
}

uint8_t RollerShutter::getMotorUpsideDown() const {
  return rsConfig.motorUpsideDown;
}

uint8_t RollerShutter::getButtonsUpsideDown() const {
  return rsConfig.buttonsUpsideDown;
}

int8_t RollerShutter::getTimeMargin() const {
  return rsConfig.timeMargin;
}

bool RollerShutter::isAutoCalibrationSupported() const {
  return false;
  // TODO(klew): implement
}

void RollerShutter::setRsConfigMotorUpsideDownValue(uint8_t value) {
  if (getMotorUpsideDown() != 0 && value > 0 && value < 3) {
    rsConfig.motorUpsideDown = value;
    saveConfig();
  }
}

void RollerShutter::setRsConfigButtonsUpsideDownValue(uint8_t value) {
  if (getButtonsUpsideDown() != 0 && value > 0 && value < 3) {
    rsConfig.buttonsUpsideDown = value;
    saveConfig();
  }
}

void RollerShutter::setRsConfigTimeMarginValue(int8_t value) {
  if (value >= -1 && value <= 100) {
    rsConfig.timeMargin = value;
    saveConfig();
  }
}

void RollerShutter::purgeConfig() {
  Supla::ChannelElement::purgeConfig();
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    generateKey(key, Supla::ConfigTag::RollerShutterTag);
    cfg->eraseKey(key);
  }
}

}  // namespace Control
}  // namespace Supla

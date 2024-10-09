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

#include "../actions.h"

namespace Supla {
namespace Control {

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
  channel.setFuncList(SUPLA_BIT_FUNC_CONTROLLINGTHEROLLERSHUTTER);
  channel.setFlag(SUPLA_CHANNEL_FLAG_RS_SBS_AND_STOP_ACTIONS);
  channel.setFlag(SUPLA_CHANNEL_FLAG_CALCFG_RECALIBRATE);
}

void RollerShutter::onInit() {
  Supla::Io::digitalWrite(
      channel.getChannelNumber(), pinUp, highIsOn ? LOW : HIGH, io);
  Supla::Io::digitalWrite(
      channel.getChannelNumber(), pinDown, highIsOn ? LOW : HIGH, io);
  Supla::Io::pinMode(channel.getChannelNumber(), pinUp, OUTPUT, io);
  Supla::Io::pinMode(channel.getChannelNumber(), pinDown, OUTPUT, io);

  if (upButton) {
    upButton->onInit();  // make sure button was initialized
    if (upButton->isMonostable()) {
      upButton->addAction(
          Supla::MOVE_UP_OR_STOP, this, Supla::CONDITIONAL_ON_PRESS);
    } else if (upButton->isBistable()) {
      upButton->addAction(
          Supla::MOVE_UP_OR_STOP, this, Supla::CONDITIONAL_ON_CHANGE);
    }
  }
  if (downButton) {
    downButton->onInit();  // make sure button was initialized
    if (downButton->isMonostable()) {
      downButton->addAction(
          Supla::MOVE_DOWN_OR_STOP, this, Supla::CONDITIONAL_ON_PRESS);
    } else if (downButton->isBistable()) {
      downButton->addAction(
          Supla::MOVE_DOWN_OR_STOP, this, Supla::CONDITIONAL_ON_CHANGE);
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

  char task = newValue->value[0];
  SUPLA_LOG_DEBUG("RS[%d] new value from server: %d",
      channel.getChannelNumber(), task);
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
    triggerCalibration();
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
    case MOVE_DOWN_OR_STOP: {
      if (inMove()) {
        stop();
      } else {
        moveDown();
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
  Supla::Storage::ScheduleSave(5000);
}

void RollerShutter::relayDownOn() {
  Supla::Io::digitalWrite(
      channel.getChannelNumber(), pinDown, highIsOn ? HIGH : LOW, io);
}

void RollerShutter::relayUpOn() {
  Supla::Io::digitalWrite(
      channel.getChannelNumber(), pinUp, highIsOn ? HIGH : LOW, io);
}

void RollerShutter::relayDownOff() {
  Supla::Io::digitalWrite(
      channel.getChannelNumber(), pinDown, highIsOn ? LOW : HIGH, io);
}

void RollerShutter::relayUpOff() {
  Supla::Io::digitalWrite(
      channel.getChannelNumber(), pinUp, highIsOn ? LOW : HIGH, io);
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
  calibrate = true;
  currentPosition = UNKNOWN_POSITION;
  setTargetPosition(0);
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

  if (operationTimeoutS != 0 &&
      millis() - lastMovementStartTime > operationTimeoutS * 1000) {
    setTargetPosition(STOP_POSITION);
    operationTimeoutS = 0;
    SUPLA_LOG_DEBUG("RS[%d]: Operation timeout", channel.getChannelNumber());
  }

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
        operationTimeoutS = 0;
        if (targetPosition > 50 || targetPosition == MOVE_DOWN_POSITION) {
          if (currentDirection == UP_DIR) {
            stopMovement();
          } else if (currentDirection == STOP_DIR) {
            SUPLA_LOG_DEBUG("RS[%d]: Calibration: closing",
                            channel.getChannelNumber());
            calibrationTime = closingTimeMs;
            if (calibrationTime == 0) {
              operationTimeoutS = 60;
            }
            startClosing();
          }
        } else {
          if (currentDirection == DOWN_DIR) {
            stopMovement();
          } else if (currentDirection == STOP_DIR) {
            SUPLA_LOG_DEBUG("RS[%d]: Calibration: opening",
                            channel.getChannelNumber());
            calibrationTime = openingTimeMs;
            if (calibrationTime == 0) {
              operationTimeoutS = 60;
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
        stopMovement();
      }
    }
  } else if (isCalibrated()) {
    if (!newTargetPositionAvailable &&
        currentDirection !=
            STOP_DIR) {  // no new command available and it is moving,
                         // just handle roller movement/status
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
          stopMovement();
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
          stopMovement();
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
        operationTimeoutS = 60;
      } else if (targetPosition == MOVE_DOWN_POSITION) {
        newDirection = DOWN_DIR;
        operationTimeoutS = 60;
      } else {
        operationTimeoutS = 0;
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
      operationTimeoutS = 60;
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
        operationTimeoutS = 0;
      }
    }
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
  return SUPLA_CALCFG_RESULT_FALSE;
}

}  // namespace Control
}  // namespace Supla

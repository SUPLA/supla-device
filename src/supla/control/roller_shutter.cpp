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

void RollerShutter::onInit() {
  if (pinUp >= 0) {
    Supla::Io::digitalWrite(
        channel.getChannelNumber(), pinUp, highIsOn ? LOW : HIGH, io);
    Supla::Io::pinMode(channel.getChannelNumber(), pinUp, OUTPUT, io);
  }
  if (pinDown >= 0) {
    Supla::Io::digitalWrite(
        channel.getChannelNumber(), pinDown, highIsOn ? LOW : HIGH, io);
    Supla::Io::pinMode(channel.getChannelNumber(), pinDown, OUTPUT, io);
  }

  RollerShutterInterface::onInit();
}

void RollerShutter::stopMovement() {
  switchOffRelays();
  currentDirection = Directions::STOP_DIR;
  doNothingTime = millis();
  // Schedule save in 5 s after stop movement of roller shutter
  Supla::Storage::ScheduleSave(rsStorageSaveDelay);
}

void RollerShutter::relayDownOn() {
  if (pinUp >= 0 && pinDown >= 0) {
    Supla::Io::digitalWrite(channel.getChannelNumber(),
                            rsConfig.motorUpsideDown == 2 ? pinUp : pinDown,
                            highIsOn ? HIGH : LOW,
                            io);
  }
}

void RollerShutter::relayUpOn() {
  if (pinUp >= 0 && pinDown >= 0) {
    Supla::Io::digitalWrite(channel.getChannelNumber(),
                            rsConfig.motorUpsideDown == 2 ? pinDown : pinUp,
                            highIsOn ? HIGH : LOW,
                            io);
  }
}

void RollerShutter::relayDownOff() {
  if (pinUp >= 0 && pinDown >= 0) {
    Supla::Io::digitalWrite(channel.getChannelNumber(),
                            rsConfig.motorUpsideDown == 2 ? pinUp : pinDown,
                            highIsOn ? LOW : HIGH,
                            io);
  }
}

void RollerShutter::relayUpOff() {
  if (pinUp >= 0 && pinDown >= 0) {
    Supla::Io::digitalWrite(channel.getChannelNumber(),
                            rsConfig.motorUpsideDown == 2 ? pinDown : pinUp,
                            highIsOn ? LOW : HIGH,
                            io);
  }
}

void RollerShutter::startClosing() {
  lastMovementStartTime = millis();
  currentDirection = Directions::DOWN_DIR;
  relayUpOff();  // just to make sure
  relayDownOn();
}

void RollerShutter::startOpening() {
  lastMovementStartTime = millis();
  currentDirection = Directions::UP_DIR;
  relayDownOff();  // just to make sure
  relayUpOn();
}

void RollerShutter::switchOffRelays() {
  relayUpOff();
  relayDownOff();
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
          if (currentDirection == Directions::UP_DIR) {
            stopMovement();
          } else if (currentDirection == Directions::STOP_DIR ||
                     currentDirection == Directions::DOWN_DIR) {
            SUPLA_LOG_DEBUG("RS[%d]: Calibration: closing",
                            channel.getChannelNumber());
            calibrationTime = closingTimeMs;
            if (calibrationTime == 0) {
              operationTimeoutMs = RS_DEFAULT_OPERATION_TIMEOUT_MS;
            }
            startClosing();
          }
        } else {
          if (currentDirection == Directions::DOWN_DIR) {
            stopMovement();
          } else if (currentDirection == Directions::STOP_DIR ||
                     currentDirection == Directions::UP_DIR) {
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
        setCalibrate(false);
        if (currentDirection == Directions::UP_DIR) {
          setCurrentPosition(0);
        } else {
          setCurrentPosition(100);
        }
        if (targetPosition < 0) {
          setTargetPosition(STOP_POSITION);
        }
        stopMovement();
      }
    }
  } else if (isCalibrated()) {
    if (!newTargetPositionAvailable &&
        currentDirection != Directions::STOP_DIR) {
      // no new command available and it is moving, just handle movement/status
      if (currentDirection == Directions::UP_DIR && currentPosition > 0) {
        int movementDistance = lastPositionBeforeMovement;
        uint32_t timeRequired =
            (1.0 * openingTimeMs * movementDistance / 100.0);
        float fractionOfMovemendDone =
            (1.0 * (millis() - lastMovementStartTime) / timeRequired);
        if (fractionOfMovemendDone > 1) {
          fractionOfMovemendDone = 1;
        }
        setCurrentPosition(lastPositionBeforeMovement -
                          movementDistance * fractionOfMovemendDone);
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
      } else if (currentDirection == Directions::DOWN_DIR &&
                 currentPosition < 100) {
        int movementDistance = 100 - lastPositionBeforeMovement;
        uint32_t timeRequired =
            (1.0 * closingTimeMs * movementDistance / 100.0);
        float fractionOfMovemendDone =
            (1.0 * (millis() - lastMovementStartTime) / timeRequired);
        if (fractionOfMovemendDone > 1) {
          fractionOfMovemendDone = 1;
        }
        setCurrentPosition(lastPositionBeforeMovement +
                          movementDistance * fractionOfMovemendDone);
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
    } else if (newTargetPositionAvailable && targetPosition != STOP_POSITION) {
      // new target state was set, let's handle it
      Directions newDirection = Directions::STOP_DIR;
      if (targetPosition == MOVE_UP_POSITION) {
        newDirection = Directions::UP_DIR;
        operationTimeoutMs = openingTimeMs + getTimeMarginValue(openingTimeMs);
        SUPLA_LOG_DEBUG("RS[%d]: Set new direction: UP, operation timeout: %d",
                        channel.getChannelNumber(),
                        operationTimeoutMs);
      } else if (targetPosition == MOVE_DOWN_POSITION) {
        newDirection = Directions::DOWN_DIR;
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
          newDirection = Directions::DOWN_DIR;  // move down
        } else if (newMovementValue < 0) {
          newDirection = Directions::UP_DIR;  // move up
        }
      }
      // If new direction is the same as current move, then keep movin`
      if (newDirection == currentDirection) {
        newTargetPositionAvailable = false;
      } else if (currentDirection ==
                 Directions::STOP_DIR) {  // else start moving
        newTargetPositionAvailable = false;
        lastPositionBeforeMovement = currentPosition;
        if (newDirection == Directions::DOWN_DIR) {
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
      Directions newDirection = Directions::STOP_DIR;
      operationTimeoutMs = RS_DEFAULT_OPERATION_TIMEOUT_MS;
      if (targetPosition == MOVE_UP_POSITION) {
        newDirection = Directions::UP_DIR;
      } else if (targetPosition == MOVE_DOWN_POSITION) {
        newDirection = Directions::DOWN_DIR;
      } else {
        // 0 - 100 = -100 (move down); 50 -
        // 20 = 30 (move up 30%), etc
        if (targetPosition == 0) {
          newDirection = Directions::UP_DIR;  // move up
        } else if (targetPosition == 100) {
          newDirection = Directions::DOWN_DIR;  // move down
        }
      }
      // If new direction is the same as current move, then keep movin`
      if (newDirection == currentDirection) {
        newTargetPositionAvailable = false;
      } else if (currentDirection ==
                 Directions::STOP_DIR) {  // else start moving
        newTargetPositionAvailable = false;
        lastPositionBeforeMovement = currentPosition;
        if (newDirection == Directions::DOWN_DIR) {
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
}

}  // namespace Control
}  // namespace Supla

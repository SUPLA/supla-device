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

namespace Supla {
namespace Control {

RollerShutter::RollerShutter(Supla::Io::Base *io,
                             int pinUp,
                             int pinDown,
                             bool highIsOn,
                             bool tiltFunctionsEnabled)
    : RollerShutter(pinUp, pinDown, highIsOn, tiltFunctionsEnabled) {
  this->io = io;
}

RollerShutter::RollerShutter(int pinUp,
                             int pinDown,
                             bool highIsOn,
                             bool tiltFunctionsEnabled)
    : RollerShutterInterface(tiltFunctionsEnabled),
      pinUp(pinUp),
      pinDown(pinDown),
      highIsOn(highIsOn) {
  channel.setFlag(SUPLA_CHANNEL_FLAG_RS_SBS_AND_STOP_ACTIONS);
  channel.setFlag(SUPLA_CHANNEL_FLAG_CALCFG_RECALIBRATE);
}

void RollerShutter::onInit() {
  initGpio(pinUp);
  initGpio(pinDown);

  RollerShutterInterface::onInit();
}

void RollerShutter::setPinUp(int pin) {
  pinUp = pin;
  initGpio(pinUp);
}

void RollerShutter::setPinDown(int pin) {
  pinDown = pin;
  initGpio(pinDown);
}

void RollerShutter::initGpio(int gpio) {
  if (gpio >= 0) {
    Supla::Io::digitalWrite(
        channel.getChannelNumber(), gpio, highIsOn ? LOW : HIGH, io);
    Supla::Io::pinMode(channel.getChannelNumber(), gpio, OUTPUT, io);
  }
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
                            pinDown,
                            highIsOn ? HIGH : LOW,
                            io);
  }
}

void RollerShutter::relayUpOn() {
  if (pinUp >= 0 && pinDown >= 0) {
    Supla::Io::digitalWrite(channel.getChannelNumber(),
                            pinUp,
                            highIsOn ? HIGH : LOW,
                            io);
  }
}

void RollerShutter::relayDownOff() {
  if (pinUp >= 0 && pinDown >= 0) {
    Supla::Io::digitalWrite(channel.getChannelNumber(),
                            pinDown,
                            highIsOn ? LOW : HIGH,
                            io);
  }
}

void RollerShutter::relayUpOff() {
  if (pinUp >= 0 && pinDown >= 0) {
    Supla::Io::digitalWrite(channel.getChannelNumber(),
                            pinUp,
                            highIsOn ? LOW : HIGH,
                            io);
  }
}

void RollerShutter::startClosing() {
  lastMovementStartTime = millis();
  currentDirection = Directions::DOWN_DIR;
  if (rsConfig.motorUpsideDown == 2) {
    relayDownOff();
    relayUpOn();
  } else {
    relayUpOff();  // just to make sure
    relayDownOn();
  }
}

void RollerShutter::startOpening() {
  lastMovementStartTime = millis();
  currentDirection = Directions::UP_DIR;
  if (rsConfig.motorUpsideDown == 2) {
    relayUpOff();
    relayDownOn();
  } else {
    relayDownOff();  // just to make sure
    relayUpOn();
  }
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

  if ((targetPosition == STOP_POSITION && inMove()) ||
      targetPosition == STOP_REQUEST) {
    targetPosition = STOP_POSITION;
    stopMovement();
    stopCalibration();
    SUPLA_LOG_DEBUG("RS[%d]: Stop movement", channel.getChannelNumber());
  }
  if (targetPosition == STOP_POSITION) {
    newTargetPositionAvailable = false;
  }

  if (isCalibrationRequested()) {
    if (targetPosition != STOP_POSITION) {
      if (!isCalibrationInProgress()) {
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
            startCalibration(closingTimeMs);
            if (!isCalibrationInProgress()) {
              // can happen, when closingTimeMs is 0
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
            startCalibration(openingTimeMs);
            if (!isCalibrationInProgress()) {
              // can happen, when openingTimeMs is 0
              operationTimeoutMs = RS_DEFAULT_OPERATION_TIMEOUT_MS;
            }
            startOpening();
          }
        }

        if (isCalibrationInProgress()) {
          SUPLA_LOG_DEBUG("RS[%d]: Calibration time: %d",
                          channel.getChannelNumber(),
                          calibrationTime);
        }
      }

      if (isCalibrationInProgress() &&
          millis() - lastMovementStartTime > calibrationTime) {
        SUPLA_LOG_DEBUG("RS[%d]: Calibration done", channel.getChannelNumber());
        stopCalibration();
        setCalibrate(false);
        if (currentDirection == Directions::UP_DIR) {
          setCurrentPosition(0, 0);
        } else {
          setCurrentPosition(100, 100);
        }
        if (targetPosition < 0) {
          setTargetPosition(STOP_REQUEST);
        }
        stopMovement();
      }
    }
  } else if (isCalibrated()) {
    if (!newTargetPositionAvailable &&
        currentDirection != Directions::STOP_DIR) {
      calculateCurrentPositionAndTilt();
      // no new command available and it is moving, just handle movement/status
      if (currentDirection == Directions::UP_DIR &&
          (currentPosition > 0 || currentTilt > 0)) {
        // check if target position is reached
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
                 (currentPosition < 100 ||
                  (currentTilt < 100 && currentTilt >= 0))) {
        // check if target position is reached
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
        lastTiltBeforeMovement = currentTilt;
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
    if (isTiltFunctionEnabled()) {
      currentTilt = UNKNOWN_POSITION;
    }

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
        lastTiltBeforeMovement = currentTilt;
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
    setTargetPosition(STOP_REQUEST);
    operationTimeoutMs = 0;
    SUPLA_LOG_DEBUG("RS[%d]: Operation timeout", channel.getChannelNumber());
  }
}

void RollerShutter::calculateCurrentPositionAndTilt() {
  if (currentDirection == Directions::UP_DIR && isTopReached()) {
    return;
  }
  if (currentDirection == Directions::DOWN_DIR && isBottomReached()) {
    return;
  }

  const bool upDir = (currentDirection == Directions::UP_DIR);

  int newTilt = UNKNOWN_POSITION;

  uint32_t fullTiltChangeTime = tiltConfig.tiltingTime;
  uint32_t fullPositionChangeTime = upDir ? openingTimeMs : closingTimeMs;

  switch (tiltConfig.tiltControlType) {
    case SUPLA_TILT_CONTROL_TYPE_STANDS_IN_POSITION_WHILE_TILTING:
    case SUPLA_TILT_CONTROL_TYPE_TILTS_ONLY_WHEN_FULLY_CLOSED: {
      fullPositionChangeTime -= tiltConfig.tiltingTime;
      break;
    }
  }

  const int positionDistance =
    upDir ? lastPositionBeforeMovement : 100 - lastPositionBeforeMovement;
  int tiltingDistance =
    upDir ? lastTiltBeforeMovement : 100 - lastTiltBeforeMovement;

  uint32_t positionChangeTimeRequired =
      (1.0 * fullPositionChangeTime * positionDistance / 100.0);

  uint32_t tiltChangeTimeRequired =
      (1.0 * fullTiltChangeTime * tiltingDistance / 100.0);

  const uint32_t movementTimeElapsed = millis() - lastMovementStartTime;
  uint32_t positionChangeTimeElapsed = movementTimeElapsed;
  uint32_t tiltChangeTimeElapsed = movementTimeElapsed;
  switch (tiltConfig.tiltControlType) {
    case SUPLA_TILT_CONTROL_TYPE_STANDS_IN_POSITION_WHILE_TILTING: {
      if (movementTimeElapsed <= tiltChangeTimeRequired) {
        positionChangeTimeElapsed = 0;
      } else {
        positionChangeTimeElapsed =
            movementTimeElapsed - tiltChangeTimeRequired;
      }
      break;
    }
    case SUPLA_TILT_CONTROL_TYPE_TILTS_ONLY_WHEN_FULLY_CLOSED: {
      if (lastPositionBeforeMovement < 100) {
        // first we move position
        if (movementTimeElapsed <= positionChangeTimeRequired) {
          tiltChangeTimeElapsed = 0;
        } else {
          tiltChangeTimeElapsed =
              movementTimeElapsed - positionChangeTimeRequired;
        }
      } else {
        // first we tilt
        if (movementTimeElapsed <= tiltChangeTimeRequired) {
          positionChangeTimeElapsed = 0;
        } else {
          positionChangeTimeElapsed =
              movementTimeElapsed - tiltChangeTimeRequired;
        }
      }
      break;
    }
  }

  if (isTiltConfigured()) {
    newTilt = lastTiltBeforeMovement;

    if (tiltChangeTimeRequired && tiltChangeTimeElapsed) {
      float fractionOfTiltMovementDone =
          (1.0 * tiltChangeTimeElapsed / tiltChangeTimeRequired);
      if (fractionOfTiltMovementDone > 1) {
        // elapsed time is more than required time, so we set 1 == 100%
        fractionOfTiltMovementDone = 1;
      }

      if (upDir) {
        newTilt -= tiltingDistance * fractionOfTiltMovementDone;
      } else {
        newTilt += tiltingDistance * fractionOfTiltMovementDone;
      }
    }
  }

  int newPosition = lastPositionBeforeMovement;
  if (positionChangeTimeRequired && positionChangeTimeElapsed) {
    float fractionOfPositionMovementDone =
        (1.0 * positionChangeTimeElapsed / positionChangeTimeRequired);
    if (fractionOfPositionMovementDone > 1) {
      // elapsed time is more than required time, so we set 1 == 100%
      fractionOfPositionMovementDone = 1;
    }

    if (upDir) {
      newPosition -= positionDistance * fractionOfPositionMovementDone;
    } else {
      newPosition += positionDistance * fractionOfPositionMovementDone;
    }
  }

  setCurrentPosition(newPosition, newTilt);
}

}  // namespace Control
}  // namespace Supla

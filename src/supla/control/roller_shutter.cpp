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
  SUPLA_LOG_DEBUG("RS[%d] Stop movement", channel.getChannelNumber());
  switchOffRelays();
  currentDirection = Directions::STOP_DIR;
  doNothingTime = millis();
  // Schedule save in 5 s after stop movement of roller shutter
  Supla::Storage::ScheduleSave(rsStorageSaveDelay, 1000);
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
    SUPLA_LOG_DEBUG("RS[%d] Stop requested", channel.getChannelNumber());
    targetPosition = STOP_POSITION;
    stopMovement();
    stopCalibration();
//    SUPLA_LOG_DEBUG("RS[%d] Stop movement", channel.getChannelNumber());
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
            SUPLA_LOG_DEBUG("RS[%d] Calibration: closing",
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
            SUPLA_LOG_DEBUG("RS[%d] Calibration: opening",
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
          SUPLA_LOG_DEBUG("RS[%d] Calibration time: %d",
                          channel.getChannelNumber(),
                          calibrationTime);
        }
      }

      if (isCalibrationInProgress() &&
          millis() - lastMovementStartTime > calibrationTime) {
        SUPLA_LOG_DEBUG("RS[%d] Calibration done", channel.getChannelNumber());
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
      if (currentDirection == Directions::UP_DIR) {
        // check if target position is reached
        if (targetPosition >= 0 &&
            currentPosition <= static_cast<int>(targetPosition) * 100) {
          if (targetTilt >= 0 && targetTilt > getCurrentTilt()) {
            stopMovement();
            setTargetPosition(UNKNOWN_POSITION, targetTilt);
          } else if (targetPosition == 0 &&
                     (targetTilt == UNKNOWN_POSITION ||
                      (targetTilt == 0 && getCurrentTilt() == 0))) {
            targetPosition = UNKNOWN_POSITION;
            targetTilt = UNKNOWN_POSITION;
            operationTimeoutMs = getTimeMarginValue(openingTimeMs);
            lastMovementStartTime = millis();
            SUPLA_LOG_DEBUG("RS[%d] operation timeout: %d",
                            channel.getChannelNumber(),
                            operationTimeoutMs);
          } else if (targetTilt == UNKNOWN_POSITION ||
                     currentTilt <= static_cast<int>(targetTilt) * 100) {
            stopMovement();
          }
        } else if (targetPosition == UNKNOWN_POSITION && targetTilt >= 0 &&
                   currentTilt <= static_cast<int>(targetTilt) * 100) {
          stopMovement();
        }
      } else if (currentDirection == Directions::DOWN_DIR) {
        // check if target position is reached
        if (targetPosition >= 0 &&
            currentPosition >= static_cast<int>(targetPosition) * 100) {
          if (targetTilt >= 0 && targetTilt < getCurrentTilt()) {
            stopMovement();
            setTargetPosition(UNKNOWN_POSITION, targetTilt);
          } else if (targetPosition == 100 &&
                     (targetTilt == UNKNOWN_POSITION ||
                      (targetTilt == 100 && getCurrentTilt() == 100))) {
            targetPosition = UNKNOWN_POSITION;
            targetTilt = UNKNOWN_POSITION;
            operationTimeoutMs = getTimeMarginValue(closingTimeMs);
            lastMovementStartTime = millis();
            SUPLA_LOG_DEBUG("RS[%d] operation timeout: %d",
                            channel.getChannelNumber(),
                            operationTimeoutMs);
          } else if (targetTilt == UNKNOWN_POSITION ||
                     currentTilt >= static_cast<int>(targetTilt) * 100) {
            stopMovement();
          }
        } else if (targetPosition == UNKNOWN_POSITION && targetTilt >= 0 &&
                   currentTilt >= static_cast<int>(targetTilt) * 100) {
          stopMovement();
        }
      }
    } else if (newTargetPositionAvailable && targetPosition != STOP_POSITION) {
      // new target state was set, let's handle it
      Directions newDirection = Directions::STOP_DIR;
      if (targetPosition == MOVE_UP_POSITION) {
        newDirection = Directions::UP_DIR;
        operationTimeoutMs = openingTimeMs + getTimeMarginValue(openingTimeMs);
        SUPLA_LOG_DEBUG("RS[%d] Set new direction: UP, operation timeout: %d",
                        channel.getChannelNumber(),
                        operationTimeoutMs);
      } else if (targetPosition == MOVE_DOWN_POSITION) {
        newDirection = Directions::DOWN_DIR;
        operationTimeoutMs = closingTimeMs + getTimeMarginValue(closingTimeMs);
        SUPLA_LOG_DEBUG(
            "RS[%d] Set new direction: DOWN, operation timeout: %d",
            channel.getChannelNumber(),
            operationTimeoutMs);
      } else {
        operationTimeoutMs = 0;
        int newMovementValue = targetPosition != UNKNOWN_POSITION
                                   ? targetPosition - getCurrentPosition()
                                   : 0;
        int newTiltingValue =
            targetTilt != UNKNOWN_POSITION ? targetTilt - getCurrentTilt() : 0;
        // 0 - 100 = -100 (move down); 50 -
        // 20 = 30 (move up 30%), etc
        SUPLA_LOG_DEBUG("RS[%d] New movement value: %d, new tilting value: %d",
                        channel.getChannelNumber(), newMovementValue,
                        newTiltingValue);
        if (newMovementValue > 0) {
          newDirection = Directions::DOWN_DIR;  // move down
        } else if (newMovementValue < 0) {
          newDirection = Directions::UP_DIR;  // move up
        } else if (newTiltingValue > 0) {
          newDirection = Directions::DOWN_DIR;  // tilt down
        } else if (newTiltingValue < 0) {
          newDirection = Directions::UP_DIR;  // tilt up
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
        // in not calibrated state we don't know current possition, so we
        // move up/down only for fully closed/open target
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
    SUPLA_LOG_DEBUG("RS[%d] Operation timeout", channel.getChannelNumber());
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
    upDir ? lastPositionBeforeMovement : 10000 - lastPositionBeforeMovement;
  int tiltingDistance =
    upDir ? lastTiltBeforeMovement : 10000 - lastTiltBeforeMovement;

  uint32_t positionChangeTimeRequired =
      (1.0 * fullPositionChangeTime * positionDistance / 10000.0);

  uint32_t tiltChangeTimeRequired =
      (1.0 * fullTiltChangeTime * tiltingDistance / 10000.0);

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
      if (lastPositionBeforeMovement < 10000) {
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

  if (newPosition < UNKNOWN_POSITION) {
    newPosition = 0;
  }
  if (newPosition > 10000) {
    newPosition = 10000;
  }
  if (newTilt < UNKNOWN_POSITION) {
    newTilt = 0;
  }
  if (newTilt > 10000) {
    newTilt = 10000;
  }

  currentPosition = newPosition;
  currentTilt = newTilt;
}

void RollerShutter::setTargetPosition(int newPosition, int newTilt) {
  if (isTiltConfigured() && newTilt > UNKNOWN_POSITION &&
      newPosition > UNKNOWN_POSITION && isCalibrated() &&
      tiltConfig.tiltControlType ==
          SUPLA_TILT_CONTROL_TYPE_CHANGES_POSITION_WHILE_TILTING) {
    SUPLA_LOG_DEBUG("RS[%d] new target position before adjustment %d, tilt %d",
                    channel.getChannelNumber(),
                    newPosition,
                    newTilt);
    if (newTilt > 100) {
      newTilt = 100;
    }
    if (newPosition > 100) {
      newPosition = 100;
    }

    // When both tilt and position is set we have to adjust target position
    // so after tilting it will be in requested position.
    // So first we calculate what would be the actual position after tilting
    // to the requested tilt:
    // If final position will be == to newPosition then we will just perform
    // tilting.
    // If the position will be < newPosition then we have to move the
    // position down to the requested position + time required for tilt up.
    // If the position will be > newPosition then we have to move the
    // position up to the requested position + time required for tilt down.

    // if deltaTilt is < 0, then we should move up
    int32_t deltaTilt = newTilt - static_cast<int32_t>(getCurrentTilt());
    int32_t tiltingTime =
        deltaTilt * static_cast<int32_t>(tiltConfig.tiltingTime) / 100;
    int32_t positionAfterTilting = getCurrentPosition();

    if (tiltingTime < 0) {  // UP
      positionAfterTilting +=
          100 * tiltingTime / static_cast<int32_t>(openingTimeMs);
    } else {  // DOWN or nothing
      positionAfterTilting +=
          100 * tiltingTime / static_cast<int32_t>(closingTimeMs);
    }

    SUPLA_LOG_DEBUG(
        "RS[%d] currentTilt %d, deltaTilt %d, tiltingTime %d, "
        "positionAfterTilting %d, cfg tiltingTime %d",
        channel.getChannelNumber(),
        getCurrentTilt(),
        deltaTilt,
        tiltingTime,
        positionAfterTilting,
        tiltConfig.tiltingTime);

    if (positionAfterTilting > newPosition) {
      uint32_t tiltDownRequiredTime =
          newTilt * tiltConfig.tiltingTime / 100;
      uint32_t posChangeForTiltDown =
          10000 * tiltDownRequiredTime / closingTimeMs;
      SUPLA_LOG_DEBUG("RS[%d] tiltDownRequiredTime %d, posChangeForTiltDown %d",
                      channel.getChannelNumber(),
                      tiltDownRequiredTime,
                      posChangeForTiltDown);
      newPosition -= (posChangeForTiltDown + 50) / 100;
    } else if (positionAfterTilting < newPosition) {
      uint32_t tiltUpRequiredTime =
          (100 - newTilt) * tiltConfig.tiltingTime / 100;
      uint32_t posChangeForTiltUp =
          10000 * tiltUpRequiredTime / openingTimeMs;
          SUPLA_LOG_DEBUG("RS[%d] tiltUpRequiredTime %d, posChangeForTiltUp %d",
                          channel.getChannelNumber(),
                          tiltUpRequiredTime,
                          posChangeForTiltUp);
      newPosition += (posChangeForTiltUp + 50) / 100;
    }
    if (newPosition < 0) {
      newPosition = 0;
    }
    if (newPosition > 100) {
      newPosition = 100;
    }
  }
  RollerShutterInterface::setTargetPosition(newPosition, newTilt);
}

}  // namespace Control
}  // namespace Supla

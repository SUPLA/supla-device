// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "cmd_roller_shutter.h"

#include <supla/linux_command.h>
#include <supla/log_wrapper.h>
#include <supla/payload/payload.h>
#include <supla/time.h>

#include <chrono>
#include <mutex>  // NOLINT(build/c++11)
#include <string>

#include <nlohmann/json.hpp>

Supla::Control::CmdRollerShutter::CmdRollerShutter(
    Supla::Parser::Parser *parser)
    : Supla::Sensor::SensorParsed<Supla::Control::RollerShutter>(parser) {
  addTiltFunctions();
}

void Supla::Control::CmdRollerShutter::relayUpOn() {
  runMotorCommand(cmdUpOn, "CmdRollerShutter.cmd_up_on", true);
}

bool Supla::Control::CmdRollerShutter::isOutputReady() const {
  return !cmdUpOn.empty() && !cmdUpOff.empty() &&
      !cmdDownOn.empty() && !cmdDownOff.empty();
}

void Supla::Control::CmdRollerShutter::relayDownOn() {
  runMotorCommand(cmdDownOn, "CmdRollerShutter.cmd_down_on", true);
}

void Supla::Control::CmdRollerShutter::relayUpOff() {
  runMotorCommand(cmdUpOff, "CmdRollerShutter.cmd_up_off", false);
}

void Supla::Control::CmdRollerShutter::relayDownOff() {
  runMotorCommand(cmdDownOff, "CmdRollerShutter.cmd_down_off", false);
}

void Supla::Control::CmdRollerShutter::runMotorCommand(
    const std::string &command, const char *context, bool turnOn) {
  if (command.empty() || (turnOn && motorCommandFailed)) {
    return;
  }
  if (!Supla::Linux::executeCommandWithTimeout(
          command, context, std::chrono::milliseconds(motorCommandTimeoutMs))) {
    motorCommandFailed = true;
    setMotorProblem(true);
  } else if (turnOn) {
    setMotorProblem(false);
  }
}

void Supla::Control::CmdRollerShutter::setMotorCommandTimeoutMs(
    uint32_t timeoutMs) {
  motorCommandTimeoutMs = timeoutMs;
}

void Supla::Control::CmdRollerShutter::setCmdUpOn(
    const std::string &newCmdUpOn) {
  cmdUpOn = newCmdUpOn;
}

void Supla::Control::CmdRollerShutter::setCmdDownOn(
    const std::string &newCmdDownOn) {
  cmdDownOn = newCmdDownOn;
}

void Supla::Control::CmdRollerShutter::setCmdUpOff(
    const std::string &newCmdUpOff) {
  cmdUpOff = newCmdUpOff;
}

void Supla::Control::CmdRollerShutter::setCmdDownOff(
    const std::string &newCmdDownOff) {
  cmdDownOff = newCmdDownOff;
}

void Supla::Control::CmdRollerShutter::onInit() {
  Supla::Control::RollerShutter::onInit();
  if (statePayload) {
    captureState();
  }
}

void Supla::Control::CmdRollerShutter::onTimer() {
  motorCommandFailed = false;
  Supla::Control::RollerShutter::onTimer();
  if (motorCommandFailed) {
    if (currentDirection != Directions::STOP_DIR) {
      stopMovement();
    }
    setNotCalibrated();
    targetPosition = STOP_POSITION;
    targetTilt = UNKNOWN_POSITION;
    newTargetPositionAvailable = false;
  }
  if (statePayload) {
    captureState();
  }
}

void Supla::Control::CmdRollerShutter::iterateAlways() {
  Supla::Control::RollerShutter::iterateAlways();
  publishState();

  if (parser && (millis() - lastReadTime > 100)) {
    if (setOfflineIfSourceDisconnected()) {
      lastReadTime = millis();
      return;
    }
    refreshParserSource(false);
    lastReadTime = millis();
    setChannelStateOnline(!isOffline());
  }
}

void Supla::Control::CmdRollerShutter::setStateOutput(
    Payload::Payload *payload, uint32_t intervalMs) {
  statePayload = payload;
  outputIntervalMs = intervalMs;
}

void Supla::Control::CmdRollerShutter::captureState() {
  const State state = {
      getCurrentPosition(), getCurrentTilt(), getCurrentDirection(),
      getTargetPosition(), getTargetTilt(),
      isTiltFunctionEnabled(), isCalibrated(), isCalibrationRequested(),
      isCalibrationInProgress(), isCalibrationFailed(), isCalibrationLost(),
  };
  std::lock_guard<std::mutex> lock(stateMutex);
  latestState = state;
}

void Supla::Control::CmdRollerShutter::publishState() {
  if (!statePayload) {
    return;
  }
  const uint32_t now = millis();
  if (lastPublishAttempt && now - *lastPublishAttempt < outputIntervalMs) {
    return;
  }

  State state;
  {
    // The timer runs motor commands outside this snapshot lock.
    std::lock_guard<std::mutex> lock(stateMutex);
    if (!latestState) {
      return;
    }
    state = *latestState;
  }
  if (publishedState && publishedState->fields() == state.fields()) {
    return;
  }
  lastPublishAttempt = now;
  if (statePayload->publishState({
          {"position", state.position},
          {"tilt", state.tilt},
          {"direction", state.direction},
          {"target_position", state.targetPosition},
          {"target_tilt", state.targetTilt},
          {"tilt_enabled", state.tiltEnabled},
          {"calibrated", state.calibrated},
          {"calibration_requested", state.calibrationRequested},
          {"calibration_in_progress", state.calibrationInProgress},
          {"calibration_failed", state.calibrationFailed},
          {"calibration_lost", state.calibrationLost},
      })) {
    publishedState = state;
  } else {
    publishedState.reset();
  }
}

// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "cmd_roller_shutter.h"

#include <supla/linux_command.h>
#include <supla/log_wrapper.h>
#include <supla/time.h>

#include <string>

Supla::Control::CmdRollerShutter::CmdRollerShutter(
    Supla::Parser::Parser *parser)
    : Supla::Sensor::SensorParsed<Supla::Control::RollerShutter>(parser) {
  addTiltFunctions();
}

void Supla::Control::CmdRollerShutter::relayUpOn() {
  if (cmdUpOn.length() > 0) {
    Supla::Linux::executeCommand(cmdUpOn, "CmdRollerShutter.cmd_up_on");
  }
}

void Supla::Control::CmdRollerShutter::relayDownOn() {
  if (cmdDownOn.length() > 0) {
    Supla::Linux::executeCommand(cmdDownOn, "CmdRollerShutter.cmd_down_on");
  }
}

void Supla::Control::CmdRollerShutter::relayUpOff() {
  if (cmdUpOff.length() > 0) {
    Supla::Linux::executeCommand(cmdUpOff, "CmdRollerShutter.cmd_up_off");
  }
}

void Supla::Control::CmdRollerShutter::relayDownOff() {
  if (cmdDownOff.length() > 0) {
    Supla::Linux::executeCommand(cmdDownOff, "CmdRollerShutter.cmd_down_off");
  }
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

void Supla::Control::CmdRollerShutter::iterateAlways() {
  Supla::Control::RollerShutter::iterateAlways();

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

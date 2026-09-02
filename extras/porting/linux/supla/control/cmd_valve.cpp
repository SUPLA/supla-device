// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "cmd_valve.h"

#include <supla/linux_command.h>
#include <supla/log_wrapper.h>
#include <supla/time.h>

#include <string>

Supla::Control::CmdValve::CmdValve(Supla::Parser::Parser *parser)
    : Supla::Sensor::SensorParsed<Supla::Control::ValveBase>(parser) {
}


void Supla::Control::CmdValve::onInit() {
  ValveBase::onInit();
  handleGetChannelState(nullptr);
}

void Supla::Control::CmdValve::setValueOnDevice(uint8_t openLevel) {
  SUPLA_LOG_DEBUG("CmdValve[%d]: openLevel %d", getChannelNumber(), openLevel);
  // we support only open/close at the moment
  if (openLevel > 0) {
    if (cmdOpen.length() > 0) {
      const std::string context =
          "CmdValve[" + std::to_string(getChannelNumber()) + "].cmd_open";
      Supla::Linux::executeCommand(cmdOpen, context.c_str());
    }
  } else {
    if (cmdClose.length() > 0) {
      const std::string context =
          "CmdValve[" + std::to_string(getChannelNumber()) + "].cmd_close";
      Supla::Linux::executeCommand(cmdClose, context.c_str());
    }
  }
}

uint8_t Supla::Control::CmdValve::getValueOpenStateFromDevice() {
  int result = 0;
  if (parser) {
    result = getStateValue();
    if (result > 0) {
      result = 100;
    } else if (result == 0) {
     result = 0;
    }
  }

  setLastState(result);

  return result;
}

void Supla::Control::CmdValve::setCmdOpen(const std::string &newCmdOpen) {
  cmdOpen = newCmdOpen;
}

void Supla::Control::CmdValve::setCmdClose(const std::string &newCmdClose) {
  cmdClose = newCmdClose;
}

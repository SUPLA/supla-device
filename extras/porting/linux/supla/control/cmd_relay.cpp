// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "cmd_relay.h"

#include <supla/linux_command.h>
#include <supla/log_wrapper.h>
#include <supla/time.h>

#include <string>

Supla::Control::CmdRelay::CmdRelay(Supla::Parser::Parser *parser,
                                   _supla_int_t functions)
    : Supla::Sensor::SensorParsed<Supla::Control::VirtualRelay>(parser) {
  channel.setFuncList(functions);
}


void Supla::Control::CmdRelay::onInit() {
  VirtualRelay::onInit();
  registerActions();
  handleGetChannelState(nullptr);
}

void Supla::Control::CmdRelay::turnOn(_supla_int_t duration) {
  Supla::Control::VirtualRelay::turnOn(duration);

  if (cmdOn.length() > 0) {
    const std::string context =
        "CmdRelay[" + std::to_string(getChannelNumber()) + "].cmd_on";
    Supla::Linux::executeCommand(cmdOn, context.c_str());
  }
}

void Supla::Control::CmdRelay::turnOff(_supla_int_t duration) {
  Supla::Control::VirtualRelay::turnOff(duration);

  if (cmdOff.length() > 0) {
    const std::string context =
        "CmdRelay[" + std::to_string(getChannelNumber()) + "].cmd_off";
    Supla::Linux::executeCommand(cmdOff, context.c_str());
  }
}

void Supla::Control::CmdRelay::setCmdOn(const std::string &newCmdOn) {
  cmdOn = newCmdOn;
}

void Supla::Control::CmdRelay::setCmdOff(const std::string &newCmdOff) {
  cmdOff = newCmdOff;
}

bool Supla::Control::CmdRelay::isOn() {
  bool newState = false;

  int result = 0;
  if (parser) {
    result = getStateValue(false);
    if (result == 1) {
      newState = true;
    } else if (result != -1) {
      result = 0;
    }
  } else {
    newState = Supla::Control::VirtualRelay::isOn();
    result = newState ? 1 : 0;
  }

  setLastState(result);

  return newState;
}

void Supla::Control::CmdRelay::iterateAlways() {
  Supla::Control::VirtualRelay::iterateAlways();

  if (parser && (millis() - lastReadTime > 100)) {
    if (setOfflineIfSourceDisconnected()) {
      lastReadTime = millis();
      return;
    }
    refreshParserSource(false);
    lastReadTime = millis();
    setNewChannelValue(true);
    setChannelStateOnline(!isOffline());
  }
}

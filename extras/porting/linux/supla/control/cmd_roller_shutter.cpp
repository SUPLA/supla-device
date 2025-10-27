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

#include "cmd_roller_shutter.h"

#include <supla/log_wrapper.h>
#include <supla/time.h>

#include <cstdio>

Supla::Control::CmdRollerShutter::CmdRollerShutter(
    Supla::Parser::Parser *parser)
    : Supla::Sensor::SensorParsed<Supla::Control::RollerShutter>(parser) {
  addTiltFunctions();
}

void Supla::Control::CmdRollerShutter::relayUpOn() {
  if (cmdUpOn.length() > 0) {
    auto p = popen(cmdUpOn.c_str(), "r");
    pclose(p);
  }
}

void Supla::Control::CmdRollerShutter::relayDownOn() {
  if (cmdDownOn.length() > 0) {
    auto p = popen(cmdDownOn.c_str(), "r");
    pclose(p);
  }
}

void Supla::Control::CmdRollerShutter::relayUpOff() {
  if (cmdUpOff.length() > 0) {
    auto p = popen(cmdUpOff.c_str(), "r");
    pclose(p);
  }
}

void Supla::Control::CmdRollerShutter::relayDownOff() {
  if (cmdDownOff.length() > 0) {
    auto p = popen(cmdDownOff.c_str(), "r");
    pclose(p);
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
    refreshParserSource();
    lastReadTime = millis();
    if (isOffline()) {
      channel.setStateOffline();
    } else {
      channel.setStateOnline();
    }
  }
}

bool Supla::Control::CmdRollerShutter::isOffline() {
  if (useOfflineOnInvalidState && parser) {
    if (getStateValue() == -1) {
      return true;
    }
  }
  return false;
}

void Supla::Control::CmdRollerShutter::setUseOfflineOnInvalidState(
    bool useOfflineOnInvalidState) {
  this->useOfflineOnInvalidState = useOfflineOnInvalidState;
  SUPLA_LOG_INFO("useOfflineOnInvalidState = %d", useOfflineOnInvalidState);
}



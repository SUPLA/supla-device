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

#include "cmd_valve.h"

#include <supla/log_wrapper.h>
#include <supla/time.h>

#include <cstdio>

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
      auto p = popen(cmdOpen.c_str(), "r");
      pclose(p);
    }
  } else {
    if (cmdClose.length() > 0) {
      auto p = popen(cmdClose.c_str(), "r");
      pclose(p);
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




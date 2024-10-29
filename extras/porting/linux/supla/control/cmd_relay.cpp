/*
 * Copyright (C) AC SOFTWARE SP. Z O.O
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "cmd_relay.h"

#include <supla/log_wrapper.h>
#include <supla/sensor/binary_parsed.h>
#include <supla/time.h>

#include <cstdio>

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
  channel.setNewValue(isOn());

  if (cmdOn.length() > 0) {
    auto p = popen(cmdOn.c_str(), "r");
    pclose(p);
  }
}

void Supla::Control::CmdRelay::turnOff(_supla_int_t duration) {
  Supla::Control::VirtualRelay::turnOff(duration);
  channel.setNewValue(isOn());

  if (cmdOff.length() > 0) {
    auto p = popen(cmdOff.c_str(), "r");
    pclose(p);
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
    result = getStateValue();
    if (result == 1) {
      newState = true;
    } else if (result != -1) {
     result = 0;
    }
  } else {
    newState = Supla::Control::VirtualRelay::isOn();
  }

  setLastState(result);

  return newState;
}

void Supla::Control::CmdRelay::iterateAlways() {
  Supla::Control::Relay::iterateAlways();

  if (parser && (millis() - lastReadTime > 100)) {
    lastReadTime = millis();
    channel.setNewValue(isOn());
    if (isOffline()) {
      channel.setOffline();
    } else {
      channel.setOnline();
    }
  }
}

bool Supla::Control::CmdRelay::isOffline() {
  if (useOfflineOnInvalidState && parser) {
    if (getStateValue() == -1) {
      return true;
    }
  }
  return false;
  //    return Supla::Control::VirtualRelay::isOffline();
}

void Supla::Control::CmdRelay::setUseOfflineOnInvalidState(
    bool useOfflineOnInvalidState) {
  this->useOfflineOnInvalidState = useOfflineOnInvalidState;
  SUPLA_LOG_INFO("useOfflineOnInvalidState = %d", useOfflineOnInvalidState);
}


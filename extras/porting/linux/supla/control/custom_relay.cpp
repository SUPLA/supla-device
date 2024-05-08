/*
 Copyright (c) 2024. Lorem ipsum dolor sit amet, consectetur adipiscing elit.
 Morbi non lorem porttitor neque feugiat blandit. Ut vitae ipsum eget quam lacinia accumsan.
 Etiam sed turpis ac ipsum condimentum fringilla. Maecenas magna.
 Proin dapibus sapien vel ante. Aliquam erat volutpat. Pellentesque sagittis ligula eget metus.
 Vestibulum commodo. Ut rhoncus gravida arcu.
 */

#include "custom_relay.h"

#include <supla/log_wrapper.h>
#include <supla/sensor/binary_parsed.h>
#include <supla/time.h>

#include <cstdio>

Supla::Control::CustomRelay::CustomRelay(Supla::Parser::Parser *parser,
                                   _supla_int_t functions)
    : Supla::Sensor::SensorParsed<Supla::Control::VirtualRelay>(parser) {
  channel.setFuncList(functions);
}

void Supla::Control::CustomRelay::onInit() {
  VirtualRelay::onInit();
  registerActions();
}

void Supla::Control::CustomRelay::turnOn(_supla_int_t duration) {
  Supla::Control::VirtualRelay::turnOn(duration);
  channel.setNewValue(isOn());

  if (cmdOn.length() > 0) {
    auto p = popen(cmdOn.c_str(), "r");
    pclose(p);
  }
}

void Supla::Control::CustomRelay::turnOff(_supla_int_t duration) {
  Supla::Control::VirtualRelay::turnOff(duration);
  channel.setNewValue(isOn());

  if (cmdOff.length() > 0) {
    auto p = popen(cmdOff.c_str(), "r");
    pclose(p);
  }
}

void Supla::Control::CustomRelay::setCmdOn(const std::string &newCmdOn) {
  cmdOn = newCmdOn;
}

void Supla::Control::CustomRelay::setCmdOff(const std::string &newCmdOff) {
  cmdOff = newCmdOff;
}

void Supla::Control::CustomRelay::setTurnOn(const std::string &newSetOn) {
  setOn = newSetOn;
}

void Supla::Control::CustomRelay::setTurnOff(const std::string &newSetOff) {
  setOff = newSetOff;
}

bool Supla::Control::CustomRelay::isOn() {
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

void Supla::Control::CustomRelay::iterateAlways() {
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

bool Supla::Control::CustomRelay::isOffline() {
  if (useOfflineOnInvalidState && parser) {
    if (getStateValue() == -1) {
      return true;
    }
  }
  return false;
  //    return Supla::Control::VirtualRelay::isOffline();
}

void Supla::Control::CustomRelay::setUseOfflineOnInvalidState(
    bool useOfflineOnInvalidState) {
  this->useOfflineOnInvalidState = useOfflineOnInvalidState;
  SUPLA_LOG_INFO("useOfflineOnInvalidState = %d", useOfflineOnInvalidState);
}

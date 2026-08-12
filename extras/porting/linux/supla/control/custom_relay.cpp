// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "custom_relay.h"

#include <supla/log_wrapper.h>
#include <supla/sensor/binary_parsed.h>
#include <supla/payload/payload.h>
#include <supla/time.h>

#include <cstdio>

Supla::Control::CustomRelay::CustomRelay(
    Supla::Parser::Parser *parser,
    Supla::Payload::Payload *payload,
    _supla_int_t functions)
    : Supla::Sensor::SensorParsed<Supla::Control::CustomVirtualRelay>(parser),
      Supla::Payload::ControlPayload<Supla::Control::CustomVirtualRelay>(
          payload) {
  channel.setFuncList(functions);
}

void Supla::Control::CustomRelay::onInit() {
  VirtualRelay::onInit();
  registerActions();
  handleGetChannelState(nullptr);
}

void Supla::Control::CustomRelay::turnOn(_supla_int_t duration) {
  Supla::Control::VirtualRelay::turnOn(duration);
  channel.setNewValue(isOn());

  payload->turnOn(parameter2Key[Supla::Payload::State], setOnValue);
}

void Supla::Control::CustomRelay::turnOff(_supla_int_t duration) {
  Supla::Control::VirtualRelay::turnOff(duration);
  channel.setNewValue(isOn());

  payload->turnOff(parameter2Key[Supla::Payload::State], setOffValue);
}

bool Supla::Control::CustomRelay::isOn() {
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
  }

  setLastState(result);

  return newState;
}

void Supla::Control::CustomRelay::iterateAlways() {
  Supla::Control::Relay::iterateAlways();

  if (parser && (millis() - lastReadTime > 100)) {
    if (setOfflineIfSourceDisconnected()) {
      lastReadTime = millis();
      return;
    }
    refreshParserSource(false);
    lastReadTime = millis();
    channel.setNewValue(isOn());
    setChannelStateOnline(!isOffline());
  }
}

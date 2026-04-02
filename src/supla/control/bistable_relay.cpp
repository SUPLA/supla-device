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

#include "bistable_relay.h"

#include <supla/log_wrapper.h>
#include <supla/time.h>
#include <supla/io.h>
#include <supla/storage/storage.h>
#include <supla/channels/channel.h>
#include <supla/control/relay.h>
#include <supla/definitions.h>
#include <supla-common/proto.h>

namespace Supla {
namespace Control {

#define STATE_ON_INIT_KEEP 2

namespace {

Supla::Io::IoPin MakeOutputPin(Supla::Io::Base *io,
                               int pin,
                               bool highIsOn) {
  Supla::Io::IoPin outputPin(pin, io);
  outputPin.setActiveHigh(highIsOn);
  outputPin.setMode(OUTPUT);
  return outputPin;
}

Supla::Io::IoPin MakeStatusInputPin(Supla::Io::Base *io,
                                    int statusPin,
                                    bool statusPullUp,
                                    bool statusHighIsOn) {
  Supla::Io::IoPin inputPin(statusPin, io);
  inputPin.setPullUp(statusPullUp);
  inputPin.setActiveHigh(statusHighIsOn);
  inputPin.setMode(INPUT);
  return inputPin;
}

}  // namespace

BistableRelay::BistableRelay(Supla::Io::IoPin outputPin,
                             Supla::Io::IoPin statusPin,
                             _supla_int_t functions)
    : Relay(outputPin, functions),
      statusInputPin(statusPin) {
  statusInputPin.setMode(INPUT);
  stateOnInit = STATE_ON_INIT_KEEP;
  setMinimumAllowedDurationMs(1000);
}

BistableRelay::BistableRelay(Supla::Io::Base *io,
                             int pin,
                             int statusPin,
                             bool statusPullUp,
                             bool statusHighIsOn,
                             bool highIsOn,
                             _supla_int_t functions)
    : BistableRelay(MakeOutputPin(io, pin, highIsOn),
                    MakeStatusInputPin(io,
                                       statusPin,
                                       statusPullUp,
                                       statusHighIsOn),
                    functions) {
}

BistableRelay::BistableRelay(int pin,
                             int statusPin,
                             bool statusPullUp,
                             bool statusHighIsOn,
                             bool highIsOn,
                             _supla_int_t functions)
    : BistableRelay(MakeOutputPin(nullptr, pin, highIsOn),
                    MakeStatusInputPin(nullptr,
                                       statusPin,
                                       statusPullUp,
                                       statusHighIsOn),
                    functions) {
}

BistableRelay::BistableRelay(Supla::Io::Base *ioOut,
                             Supla::Io::Base *ioState,
                             int pin,
                             int statusPin,
                             bool statusPullUp,
                             bool statusHighIsOn,
                             bool highIsOn,
                             _supla_int_t functions)
    : BistableRelay(MakeOutputPin(ioOut, pin, highIsOn),
                    MakeStatusInputPin(ioState,
                                       statusPin,
                                       statusPullUp,
                                       statusHighIsOn),
                    functions) {
}

void BistableRelay::onInit() {
  statusInputPin.pinMode(channel.getChannelNumber());
  channel.setNewValue(isOn());
  outputPin.pinMode(channel.getChannelNumber());
  outputPin.writeInactive(channel.getChannelNumber());

  busy = true;
  Supla::Control::Relay::onInit();
  busy = false;
  if (!skipInitialStateSetting) {
    if (stateOnInit == STATE_ON_INIT_ON ||
        stateOnInit == STATE_ON_INIT_RESTORED_ON) {
      turnOn();
    } else if (stateOnInit == STATE_ON_INIT_OFF ||
               stateOnInit == STATE_ON_INIT_RESTORED_OFF) {
      turnOff();
    }
  }
}

void BistableRelay::iterateAlways() {
  Relay::iterateAlways();

  if (millis() - lastReadTime > 100) {
    lastReadTime = millis();
    bool currentState = isOn();
    if (!isStatusUnknown() && currentState != channel.getValueBool()) {
      channel.setNewValue(currentState);
      if (lastCommandTurnOn && !currentState) {
        durationMs = 0;
        durationTimestamp = 0;
        lastCommandTurnOn = false;
      } else if (!lastCommandTurnOn && currentState) {
        lastCommandTurnOn = true;
        applyDuration(0, true);
      }
    }
  }

  if (busy && millis() - disarmTimeMs > 200) {
    busy = false;
    outputPin.writeInactive(channel.getChannelNumber());
  }
}

int32_t BistableRelay::handleNewValueFromServer(
    TSD_SuplaChannelNewValue *newValue) {
  // ignore new requests if we are in the middle of state change
  if (busy) {
    return 0;
  } else {
    return Relay::handleNewValueFromServer(newValue);
  }
}

void BistableRelay::turnOn(_supla_int_t duration) {
  if (busy) {
    return;
  }

  applyDuration(duration, true);

  if (isStatusUnknown() || !isOn()) {
    internalToggle();
  }
  lastCommandTurnOn = true;
}

void BistableRelay::turnOff(_supla_int_t duration) {
  if (busy) {
    return;
  }

  applyDuration(duration, false);

  if (isStatusUnknown()) {
    internalToggle();
  } else if (isOn()) {
    internalToggle();
  }
  lastCommandTurnOn = false;
}

bool BistableRelay::isOn() {
  if (isStatusUnknown()) {
    return false;
  }
  return statusInputPin.readActive(channel.getChannelNumber());
}

bool BistableRelay::isStatusUnknown() {
  return statusInputPin.getPin() < 0;
}

void BistableRelay::internalToggle() {
  SUPLA_LOG_INFO("BistableRelay[%d] toggle relay", channel.getChannelNumber());
  busy = true;
  disarmTimeMs = millis();
  outputPin.writeActive(channel.getChannelNumber());

  // Schedule save in 5 s after state change
  Supla::Storage::ScheduleSave(5000, 2000);
}

void BistableRelay::toggle(_supla_int_t duration) {
  if (isOn() || isStatusUnknown()) {
    turnOff(duration);
  } else {
    turnOn(duration);
  }
}

}  // namespace Control
}  // namespace Supla

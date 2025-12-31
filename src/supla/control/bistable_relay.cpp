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

BistableRelay::BistableRelay(Supla::Io::Base *io,
                             int pin,
                             int statusPin,
                             bool statusPullUp,
                             bool statusHighIsOn,
                             bool highIsOn,
                             _supla_int_t functions)
    : BistableRelay(io,
                    io,
                    pin,
                    statusPin,
                    statusPullUp,
                    statusHighIsOn,
                    highIsOn,
                    functions) {
}

BistableRelay::BistableRelay(Supla::Io::Base *ioOut,
                Supla::Io::Base *ioState,
                int pin,
                int statusPin,
                bool statusPullUp,
                bool statusHighIsOn,
                bool highIsOn,
                _supla_int_t functions) :
    Relay(ioOut, pin, highIsOn, functions),
    ioState(ioState),
    statusPin(statusPin),
    statusPullUp(statusPullUp),
    statusHighIsOn(statusHighIsOn) {
  stateOnInit = STATE_ON_INIT_KEEP;
  setMinimumAllowedDurationMs(1000);
}

BistableRelay::BistableRelay(int pin,
                             int statusPin,
                             bool statusPullUp,
                             bool statusHighIsOn,
                             bool highIsOn,
                             _supla_int_t functions)
    : BistableRelay(nullptr,
                    nullptr,
                    pin,
                    statusPin,
                    statusPullUp,
                    statusHighIsOn,
                    highIsOn,
                    functions) {
}

void BistableRelay::onInit() {
  if (statusPin >= 0) {
    Supla::Io::pinMode(channel.getChannelNumber(),
                       statusPin,
                       statusPullUp ? INPUT_PULLUP : INPUT, ioState);
    channel.setNewValue(isOn());
  } else {
    channel.setNewValue(false);
  }

  Supla::Io::pinMode(channel.getChannelNumber(), pin, OUTPUT, io);
  Supla::Io::digitalWrite(channel.getChannelNumber(), pin, pinOffValue(), io);

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

  if (statusPin >= 0 && (millis() - lastReadTime > 100)) {
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
    Supla::Io::digitalWrite(channel.getChannelNumber(), pin, pinOffValue(), io);
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
  return Supla::Io::digitalRead(channel.getChannelNumber(),
                                statusPin,
                                ioState) == (statusHighIsOn ? HIGH : LOW);
}

bool BistableRelay::isStatusUnknown() {
  return (statusPin < 0);
}

void BistableRelay::internalToggle() {
  SUPLA_LOG_INFO("BistableRelay[%d] toggle relay", channel.getChannelNumber());
  busy = true;
  disarmTimeMs = millis();
  Supla::Io::digitalWrite(channel.getChannelNumber(), pin, pinOnValue(), io);

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

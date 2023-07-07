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

#include "virtual_relay.h"

#include <supla/log_wrapper.h>

#include "../time.h"

Supla::Control::VirtualRelay::VirtualRelay(_supla_int_t functions)
    : Relay(-1, true, functions) {
}

void Supla::Control::VirtualRelay::onInit() {
  uint32_t duration = durationMs;
  if (stateOnInit == STATE_ON_INIT_ON ||
      stateOnInit == STATE_ON_INIT_RESTORED_ON) {
    turnOn(duration);
  } else {
    turnOff(duration);
  }
}

void Supla::Control::VirtualRelay::turnOn(_supla_int_t duration) {
  SUPLA_LOG_INFO(
      "Relay[%d] turn ON (duration %d ms)",
      channel.getChannelNumber(),
      duration);
  durationMs = duration;
  if (keepTurnOnDurationMs) {
    durationMs = storedTurnOnDurationMs;
  }
  if (durationMs != 0) {
    durationTimestamp = millis();
  } else {
    durationTimestamp = 0;
  }

  state = true;

  channel.setNewValue(state);
  // Schedule save in 5 s after state change
  Supla::Storage::ScheduleSave(5000);
}

void Supla::Control::VirtualRelay::turnOff(_supla_int_t duration) {
  SUPLA_LOG_INFO(
      "Relay[%d] turn OFF (duration %d ms)",
      channel.getChannelNumber(),
      duration);
  durationMs = duration;
  if (durationMs != 0) {
    durationTimestamp = millis();
  } else {
    durationTimestamp = 0;
  }
  state = false;

  channel.setNewValue(state);
  // Schedule save in 5 s after state change
  Supla::Storage::ScheduleSave(5000);
}

bool Supla::Control::VirtualRelay::isOn() {
  return state;
}

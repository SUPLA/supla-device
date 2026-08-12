// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "virtual_relay.h"

#include <supla/log_wrapper.h>
#include <supla/storage/storage.h>

#include "../time.h"

Supla::Control::VirtualRelay::VirtualRelay(_supla_int_t functions)
    : Relay(-1, true, functions) {
}

void Supla::Control::VirtualRelay::turnOn(_supla_int_t duration) {
  state = true;
  Supla::Control::Relay::turnOn(duration);
}

void Supla::Control::VirtualRelay::turnOff(_supla_int_t duration) {
  state = false;
  Supla::Control::Relay::turnOff(duration);
}

bool Supla::Control::VirtualRelay::isOn() {
  return state;
}

void Supla::Control::VirtualRelay::setNewChannelValue(bool value) {
  (void)value;
  // parameter value is ignored. We use isOn() instead
  channel.setNewValue(isOn());
}

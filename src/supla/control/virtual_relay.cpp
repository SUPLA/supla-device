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

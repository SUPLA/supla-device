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

#include "binary.h"

#include <supla/time.h>

#include "../io.h"

Supla::Sensor::Binary::Binary(Supla::Io *io,
                              int pin,
                              bool pullUp,
                              bool invertLogic)
    : Binary(pin, pullUp, invertLogic) {
  this->io = io;
}

Supla::Sensor::Binary::Binary(int pin, bool pullUp, bool invertLogic)
    : pin(pin), pullUp(pullUp), invertLogic(invertLogic) {
}

bool Supla::Sensor::Binary::getValue() {
  auto value =
      Supla::Io::digitalRead(channel.getChannelNumber(), pin, io) == LOW ? false
                                                                         : true;
  value = !invertLogic ? value : !value;
  return value;
}

void Supla::Sensor::Binary::onInit() {
  Supla::Io::pinMode(
      channel.getChannelNumber(), pin, pullUp ? INPUT_PULLUP : INPUT, io);
  channel.setNewValue(getValue());
}


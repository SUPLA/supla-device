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

#include "custom_channel.h"
#include <supla/network/network.h>

#include <sstream>
#include <string>
#include <iostream>


Supla::CustomChannel::CustomChannel(
    Supla::Parser::Parser *parser)
    : SensorParsed(parser) {
}

void Supla::CustomChannel::onInit() {
}

void Supla::CustomChannel::setValue(std::string input) {
  // parse hex: 01 02 03 04 05 06 07 08 to uint8_t[8]
  std::istringstream iss(input);
  uint8_t bytes[8] = {};
  int value = 0;
  for (int i = 0; i < 8 && iss >> std::hex >> value; i++) {
    bytes[i] = static_cast<uint8_t>(value);
  }
  getChannel()->setNewValue(reinterpret_cast<const char *>(bytes));
  Supla::Network::printData("CustomChannel new value:", bytes, 8);
}

Supla::Channel *Supla::CustomChannel::getChannel() {
  return &channel;
}


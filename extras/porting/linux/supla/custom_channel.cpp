// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

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


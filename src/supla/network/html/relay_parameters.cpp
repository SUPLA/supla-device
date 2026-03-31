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

#ifndef ARDUINO_ARCH_AVR
#include "relay_parameters.h"

#include <stdint.h>
#include <string.h>
#include <supla/clock/clock.h>
#include <supla/control/relay.h>
#include <supla/log_wrapper.h>
#include <supla/network/html_element.h>
#include <supla/network/web_sender.h>
#include <supla/storage/config.h>
#include <supla/storage/config_tags.h>
#include <supla/storage/storage.h>
#include <supla/tools.h>

using Supla::Html::RelayParameters;

RelayParameters::RelayParameters(Supla::Control::Relay* relay)
    : HtmlElement(HTML_SECTION_FORM), relay(relay) {
}

RelayParameters::~RelayParameters() {
}

void RelayParameters::send(Supla::WebSender* sender) {
  if (relay && relay->getChannelNumber() >= 0) {
    uint32_t value = relay->getOvercurrentThreshold();
    char key[16] = {};
    Supla::Config::generateKey(key,
                               relay->getChannelNumber(),
                               Supla::ConfigTag::RelayOvercurrentThreshold);

    sender->labeledField(key, "Overcurrent protection [A]", [&]() {
      sender->numberInput(
          key,
          {
              .min = 0,
              .max =
                  fixed(static_cast<int>(relay->getOvercurrentMaxAllowed()), 2),
              .value = fixed(static_cast<int>(value), 2),
              .step = fixed(1, 2),
          });
    });
  }
}

bool RelayParameters::handleResponse(const char* key, const char* value) {
  char expectedKey[16] = {};
  Supla::Config::generateKey(expectedKey,
                             relay->getChannelNumber(),
                             Supla::ConfigTag::RelayOvercurrentThreshold);

  if (strcmp(key, expectedKey) == 0) {
    uint32_t param = floatStringToInt(value, 2);
    relay->setOvercurrentThreshold(param);
    return true;
  }

  return false;
}

#endif  // ARDUINO_ARCH_AVR

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

#include "relay_parameters.h"

#include <stdint.h>
#include <supla/network/html_element.h>
#include <supla/storage/config_tags.h>
#include <supla/control/relay.h>

#include <string.h>
#include <supla/network/web_sender.h>
#include <supla/storage/config.h>
#include <supla/storage/storage.h>
#include <supla/tools.h>
#include <supla/log_wrapper.h>
#include <supla/clock/clock.h>

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

    // form-field BEGIN
    sender->send("<div class=\"form-field\">");
    sender->sendLabelFor(key,
                         "Overcurrent protection [A]");
    sender->send("<input type=\"number\" min=\"0\" max=\"");
    sender->send(relay->getOvercurrentMaxAllowed(), 2);
    sender->send("\" step=\"0.01\" ");
    sender->sendNameAndId(key);
    sender->send(" value=\"");
    sender->send(value, 2);
    sender->send("\">");
    sender->send("</div>");
    // form-field END
  }
}

bool RelayParameters::handleResponse(const char* key, const char* value) {
  char expectedKey[16] = {};
  Supla::Config::generateKey(expectedKey, relay->getChannelNumber(),
                             Supla::ConfigTag::RelayOvercurrentThreshold);

  if (strcmp(key, expectedKey) == 0) {
    uint32_t param = floatStringToInt(value, 2);
    relay->setOvercurrentThreshold(param);
    return true;
  }

  return false;
}



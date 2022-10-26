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

#include <stdio.h>
#include <string.h>
#include <supla/network/web_sender.h>
#include <supla/storage/config.h>
#include <supla/storage/storage.h>
#include <supla/tools.h>
#include <supla/element.h>
#include <esp_ds18b20.h>
#include <supla/log_wrapper.h>

#include "ds18b20_parameters.h"

#define DS_ADDRESS_SIZE 8

namespace Supla {

namespace Html {

DS18B20Parameters::DS18B20Parameters(int channel)
    : HtmlElement(HTML_SECTION_FORM), channel(channel) {
}

DS18B20Parameters::~DS18B20Parameters() {
}

void DS18B20Parameters::send(Supla::WebSender* sender) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    char tmp[100] = {};
    char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    Config::generateKey(key, channel, "address");

    uint8_t dsAddress[DS_ADDRESS_SIZE] = {};
    cfg->getBlob(key, reinterpret_cast<char*>(dsAddress), DS_ADDRESS_SIZE);

    sender->send(
        "<h3>External thermometer #"
        );

    snprintf(tmp, sizeof(tmp), "%d", channel);
    sender->send(tmp);
    sender->send(
        "</h3><p>Assigned address: "
        );

    if (dsAddress[0]) {
      snprintf(
          tmp, sizeof(tmp),
          "%02X%02X%02X%02X%02X%02X%02X%02X",
          dsAddress[0],
          dsAddress[1],
          dsAddress[2],
          dsAddress[3],
          dsAddress[4],
          dsAddress[5],
          dsAddress[6],
          dsAddress[7]);
      sender->send(tmp);
    } else {
      sender->send("Not configured");
    }
    sender->send(
        "<br>Value: "
        );

    auto element = Supla::Element::getElementByChannelNumber(channel);
    if (element && element->getChannel()) {
      auto channelPtr = element->getChannel();
      snprintf(tmp, sizeof(tmp), "%.1f", channelPtr->getValueDouble());
      sender->send(tmp);
    } else {
      sender->send("---");
    }

    sender->send("</p>");

    sender->send("<div class=\"form-field right-checkbox\">");
    sender->sendLabelFor(key, "Reset assignement?");
    sender->send("<label>");
    sender->send("<div class=\"switch\">");
    sender->send("<input type=\"checkbox\" value=\"on\"");
    sender->sendNameAndId(key);
    sender->send(">");
    sender->send("<span class=\"slider\"></span>");
    sender->send("</div>");
    sender->send("</label>");
    sender->send("</div>");
  }
}

bool DS18B20Parameters::handleResponse(const char* key, const char* value) {
  char myKey[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
  Config::generateKey(myKey, channel, "address");
  if (strcmp(key, myKey) == 0) {
    bool reset = (strcmp(value, "on") == 0);
    if (reset) {
      auto cfg = Supla::Storage::ConfigInstance();
      uint8_t dsAddress[DS_ADDRESS_SIZE] = {};
      cfg->getBlob(key, reinterpret_cast<char*>(dsAddress), DS_ADDRESS_SIZE);
      Supla::Sensor::DS18B20::clearAssignedAddress(dsAddress);
      SUPLA_LOG_DEBUG("DS18B20[%d]: resetting address assignement", channel);
    }
    return true;
  }
  return false;
}

};  // namespace Html
};  // namespace Supla

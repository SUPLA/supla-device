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

#include <string.h>
#include <supla/network/web_sender.h>
#include <supla/storage/config.h>
#include <supla/storage/storage.h>
#include <supla/tools.h>
#include <supla/device/remote_device_config.h>
#include <supla/device/status_led.h>

#include "status_led_parameters.h"

namespace Supla {

namespace Html {

StatusLedParameters::StatusLedParameters() : HtmlElement(HTML_SECTION_FORM) {
}

StatusLedParameters::~StatusLedParameters() {
}

void StatusLedParameters::send(Supla::WebSender* sender) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    int8_t value = 0;
    cfg->getInt8("statusled", &value);

    // form-field BEGIN
    sender->send("<div class=\"form-field\">");
    const char key[] = "led";
    sender->sendLabelFor(key, "Status LED");
    sender->send("<div>");
    sender->send(
        "<select ");
    sender->sendNameAndId(key);
    sender->send(">"
        "<option value=\"0\"");
    sender->send(selected(value == 0));
    sender->send(
        ">ON - WHEN CONNECTED</option>"
        "<option value=\"1\"");
    sender->send(selected(value == 1));
    sender->send(
        ">OFF - WHEN CONNECTED</option>"
        "<option value=\"2\"");
    sender->send(selected(value == 2));
    sender->send(
        ">ALWAYS OFF</option></select>");
    sender->send("</div>");
    sender->send("</div>");
    // form-field END
  }
}

bool StatusLedParameters::handleResponse(const char* key, const char* value) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (strcmp(key, "led") == 0) {
    int led = stringToUInt(value);
    int8_t valueInCfg = -1;
    cfg->getInt8(Supla::Device::StatusLedCfgTag, &valueInCfg);
    if (valueInCfg != led) {
      switch (led) {
        default:
        case 0: {
          cfg->setInt8(Supla::Device::StatusLedCfgTag, 0);
          break;
        }
        case 1: {
          cfg->setInt8(Supla::Device::StatusLedCfgTag, led);
          break;
        }
        case 2: {
          cfg->setInt8(Supla::Device::StatusLedCfgTag, led);
          break;
        }
      }
      cfg->setDeviceConfigChangeFlag();
    }
    return true;
  }
  return false;
}

};  // namespace Html
};  // namespace Supla

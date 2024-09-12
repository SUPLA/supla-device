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

#include "power_status_led_parameters.h"

#include <string.h>
#include <supla/network/web_sender.h>
#include <supla/storage/config.h>
#include <supla/storage/storage.h>
#include <supla/tools.h>
#include <supla/device/remote_device_config.h>
#include <supla/element.h>
#include <supla/storage/config_tags.h>

using Supla::Html::PowerStatusLedParameters;

PowerStatusLedParameters::PowerStatusLedParameters()
    : HtmlElement(HTML_SECTION_FORM) {
}

PowerStatusLedParameters::~PowerStatusLedParameters() {
}

void PowerStatusLedParameters::send(Supla::WebSender* sender) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    int8_t value = 0;
    cfg->getInt8(Supla::ConfigTag::PowerStatusLedCfgTag, &value);

    // form-field BEGIN
    sender->send("<div class=\"form-field\">");
    sender->sendLabelFor(Supla::ConfigTag::PowerStatusLedCfgTag,
                         "Power Status LED");
    sender->send("<div>");
    sender->send(
        "<select ");
    sender->sendNameAndId(Supla::ConfigTag::PowerStatusLedCfgTag);
    sender->send(">"
        "<option value=\"0\"");
    sender->send(selected(value == 0));
    sender->send(
        ">Enabled</option>"
        "<option value=\"1\"");
    sender->send(selected(value == 1));
    sender->send(
        ">Disabled</option></select>");
    sender->send("</div>");
    sender->send("</div>");
    // form-field END
  }
}

bool PowerStatusLedParameters::handleResponse(const char* key,
                                              const char* value) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (strcmp(key, Supla::ConfigTag::PowerStatusLedCfgTag) == 0) {
    int led = stringToUInt(value);
    int8_t valueInCfg = -1;
    cfg->getInt8(Supla::ConfigTag::PowerStatusLedCfgTag, &valueInCfg);
    if (valueInCfg != led) {
      switch (led) {
        default:
        case 0: {
          cfg->setInt8(Supla::ConfigTag::PowerStatusLedCfgTag, 0);
          break;
        }
        case 1: {
          cfg->setInt8(Supla::ConfigTag::PowerStatusLedCfgTag, led);
          break;
        }
      }
      cfg->setDeviceConfigChangeFlag();
      Supla::Element::NotifyElementsAboutConfigChange(
          SUPLA_DEVICE_CONFIG_FIELD_POWER_STATUS_LED);
    }
    return true;
  }
  return false;
}

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

#include "screen_delay_parameters.h"

#include <string.h>
#include <supla/network/web_sender.h>
#include <supla/storage/config.h>
#include <supla/storage/storage.h>
#include <supla/tools.h>
#include <supla/log_wrapper.h>
#include <supla/element.h>
#include <supla/storage/config_tags.h>

using Supla::Html::ScreenDelayParameters;

ScreenDelayParameters::ScreenDelayParameters()
    : HtmlElement(HTML_SECTION_FORM) {
}

ScreenDelayParameters::~ScreenDelayParameters() {
}

void ScreenDelayParameters::send(Supla::WebSender* sender) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    int32_t value = 0;  // default value
    cfg->getInt32(Supla::ConfigTag::ScreenDelayCfgTag, &value);
    if (value < 0) {
      value = 0;
    }
    if (value > 65535) {
      value = 65535;
    }

    // form-field BEGIN
    sender->send("<div class=\"form-field\">");
    sender->sendLabelFor(Supla::ConfigTag::ScreenDelayCfgTag,
                         "Turn screen off after [sec]");
    sender->send(
        "<input type=\"number\" min=\"0\" max=\"65535\" step=\"1\" ");
    sender->sendNameAndId(Supla::ConfigTag::ScreenDelayCfgTag);
    sender->send(" value=\"");
    sender->send(value, 0);
    sender->send("\">");
    sender->send("</div>");
    // form-field END
  }
}

bool ScreenDelayParameters::handleResponse(const char* key,
                                                const char* value) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (strcmp(key, Supla::ConfigTag::ScreenDelayCfgTag) == 0) {
    int32_t param = stringToInt(value);
    if (param < 0) {
      param = 0;
    }
    if (param > 65535) {
      param = 65535;
    }

    int32_t currentValue = 0;
    cfg->getInt32(Supla::ConfigTag::ScreenDelayCfgTag, &currentValue);
    if (currentValue < 0) {
      currentValue = 0;
    }
    if (currentValue > 65535) {
      currentValue = 65535;
    }

    if (param != currentValue) {
      cfg->setInt32(Supla::ConfigTag::ScreenDelayCfgTag, param);
      cfg->setDeviceConfigChangeFlag();
      Supla::Element::NotifyElementsAboutConfigChange(
          SUPLA_DEVICE_CONFIG_FIELD_HOME_SCREEN_OFF_DELAY);
    }
    return true;
  }
  return false;
}


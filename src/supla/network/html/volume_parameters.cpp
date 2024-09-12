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

#include "volume_parameters.h"

#include <string.h>
#include <supla/network/web_sender.h>
#include <supla/storage/config.h>
#include <supla/storage/storage.h>
#include <supla/tools.h>
#include <supla/log_wrapper.h>
#include <supla/clock/clock.h>
#include <supla/storage/config_tags.h>

using Supla::Html::VolumeParameters;

VolumeParameters::VolumeParameters()
    : HtmlElement(HTML_SECTION_FORM) {
}

VolumeParameters::~VolumeParameters() {
}

void VolumeParameters::send(Supla::WebSender* sender) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    uint8_t value = 0;  // default value
    cfg->getUInt8(Supla::ConfigTag::VolumeCfgTag, &value);
    if (value > 100) {
      value = 100;
    }

    // form-field BEGIN
    sender->send("<div class=\"form-field\">");
    sender->sendLabelFor(Supla::ConfigTag::VolumeCfgTag,
                         "Button volume");
    sender->send(
        "<input type=\"range\" min=\"0\" max=\"100\" step=\"1\" "
        "class=\"range-slider\" ");
    sender->sendNameAndId(Supla::ConfigTag::VolumeCfgTag);
    sender->send(" value=\"");
    sender->send(value, 0);
    sender->send("\">");
    sender->send("</div>");
    // form-field END
  }
}

bool VolumeParameters::handleResponse(const char* key,
                                                const char* value) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (strcmp(key, Supla::ConfigTag::VolumeCfgTag) == 0) {
    int32_t param = stringToInt(value);
    if (param < 0) {
      param = 0;
    }
    if (param > 100) {
      param = 100;
    }

    uint8_t currentValue = 0;
    cfg->getUInt8(Supla::ConfigTag::VolumeCfgTag, &currentValue);
    if (currentValue > 100) {
      currentValue = 100;
    }

    if (param != currentValue) {
      cfg->setUInt8(Supla::ConfigTag::VolumeCfgTag,
                    static_cast<uint8_t>(param));
      cfg->setDeviceConfigChangeFlag();
      Supla::Element::NotifyElementsAboutConfigChange(
          SUPLA_DEVICE_CONFIG_FIELD_BUTTON_VOLUME);
    }
    return true;
  }
  return false;
}


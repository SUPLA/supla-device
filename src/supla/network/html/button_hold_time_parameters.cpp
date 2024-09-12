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

#include "button_hold_time_parameters.h"

#include <string.h>
#include <supla/network/web_sender.h>
#include <supla/storage/config.h>
#include <supla/storage/storage.h>
#include <supla/tools.h>
#include <supla/log_wrapper.h>
#include <supla/storage/config_tags.h>

using Supla::Html::ButtonHoldTimeParameters;

ButtonHoldTimeParameters::ButtonHoldTimeParameters()
    : HtmlElement(HTML_SECTION_FORM) {
}

void ButtonHoldTimeParameters::send(Supla::WebSender* sender) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    uint32_t value = 700;  // default value
    cfg->getUInt32(Supla::ConfigTag::BtnHoldTag, &value);

    // form-field BEGIN
    sender->send("<div class=\"form-field\">");
    sender->sendLabelFor(Supla::ConfigTag::BtnHoldTag,
                         "Hold detection time [s]");
    sender->send("<input type=\"number\" min=\"0.2\" max=\"10\" step=\"0.1\" ");
    sender->sendNameAndId(Supla::ConfigTag::BtnHoldTag);
    sender->send(" value=\"");
    sender->send(value, 3);
    sender->send("\">");
    sender->send("</div>");
    // form-field END
  }
}

bool ButtonHoldTimeParameters::handleResponse(const char* key,
                                                const char* value) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (strcmp(key, Supla::ConfigTag::BtnHoldTag) == 0) {
    uint32_t param = floatStringToInt(value, 3);
    if (param >= 200 && param <= 10000) {
      cfg->setUInt32(Supla::ConfigTag::BtnHoldTag, param);
    }
    return true;
  }
  return false;
}


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
#include <supla/device/sw_update.h>
#include <supla/network/web_sender.h>
#include <supla/storage/config.h>
#include <supla/storage/storage.h>
#include <supla/tools.h>

#include "custom_sw_update.h"

namespace Supla {

namespace Html {

CustomSwUpdate::CustomSwUpdate() : HtmlElement(HTML_SECTION_FORM) {
}

CustomSwUpdate::~CustomSwUpdate() {
}

void CustomSwUpdate::send(Supla::WebSender* sender) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    char url[SUPLA_MAX_URL_LENGTH] = {};

    // form-field BEGIN
    sender->send("<div class=\"form-field\">");
    const char key[] = "swupdateurl";
    sender->sendLabelFor(key, "Update server address");
    sender->send("<input ");
    sender->sendNameAndId(key);
    if (cfg->getSwUpdateServer(url)) {
      sender->send(" value=\"");
      sender->send(url);
    }
    sender->send("\">");
    sender->send("</div>");
    // form-field END
  }
}

bool CustomSwUpdate::handleResponse(const char* key, const char* value) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (strcmp(key, "swupdateurl") == 0) {
    if (strlen(value) > 0) {
      cfg->setSwUpdateServer(value);
    }
    return true;
  }
  return false;
}

};  // namespace Html
};  // namespace Supla

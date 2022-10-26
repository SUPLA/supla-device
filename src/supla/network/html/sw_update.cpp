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

#include "sw_update.h"

namespace Supla {

namespace Html {

SwUpdate::SwUpdate() : HtmlElement(HTML_SECTION_FORM) {
}

SwUpdate::~SwUpdate() {
}

void SwUpdate::send(Supla::WebSender* sender) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    bool update = (cfg->getDeviceMode() == DEVICE_MODE_SW_UPDATE);

    // form-field BEGIN
    sender->send("<div class=\"form-field\">");
    const char key[] = "upd";
    sender->sendLabelFor(key, "Firmware update");
    sender->send("<div>");
    sender->send(
        "<select ");
    sender->sendNameAndId(key);
    sender->send("><option value=\"0\"");
    sender->send(selected(!update));
    sender->send(
        ">NO</option>"
        "<option value=\"1\"");
    sender->send(selected(update));
    sender->send(
        ">YES</option></select>");
    sender->send("</div>");
    sender->send("</div>");
    // form-field END
  }
}

bool SwUpdate::handleResponse(const char* key, const char* value) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (strcmp(key, "upd") == 0) {
    int update = stringToUInt(value);
    switch (update) {
      default:
      case 0: {
        cfg->setDeviceMode(DEVICE_MODE_NORMAL);
        cfg->setSwUpdateBeta(false);
        break;
      }
      case 1: {
        cfg->setDeviceMode(DEVICE_MODE_SW_UPDATE);
        cfg->setSwUpdateBeta(false);
        break;
      }
    }
    return true;
  }
  return false;
}

};  // namespace Html
};  // namespace Supla

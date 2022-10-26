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

#include "sw_update_beta.h"

namespace Supla {

namespace Html {

SwUpdateBeta::SwUpdateBeta() : HtmlElement(HTML_SECTION_BETA_FORM) {
}

SwUpdateBeta::~SwUpdateBeta() {
}

void SwUpdateBeta::send(Supla::WebSender* sender) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    bool update = (cfg->getDeviceMode() == DEVICE_MODE_SW_UPDATE);
    bool beta = cfg->isSwUpdateBeta();

    // form-field BEGIN
    sender->send("<div class=\"form-field\">");
    const char key[] = "updbeta";
    sender->sendLabelFor(key, "Firmware update");
    sender->send("<div>");
    sender->send("<select ");
    sender->sendNameAndId(key);
    sender->send(">"
        "<option value=\"0\"");
    sender->send(selected(!update));
    sender->send(
        ">NO</option>"
        "<option value=\"1\"");
    sender->send(selected(update && !beta));
    sender->send(
        ">YES</option>"
        "<option value=\"2\"");
    sender->send(selected(update && beta));
    sender->send(
        ">YES - BETA</option></select>");
    sender->send(
        "<div class=\"hint\">"
        "Warning: beta SW versions may contain bugs and your device may not "
        "work properly.</div>");
    sender->send("</div>");
    sender->send("</div>");
    // form-field END
  }
}

bool SwUpdateBeta::handleResponse(const char* key, const char* value) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (strcmp(key, "updbeta") == 0) {
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
      case 2: {
        cfg->setDeviceMode(DEVICE_MODE_SW_UPDATE);
        cfg->setSwUpdateBeta(true);
        break;
      }
    }
    return true;
  }
  return false;
}

};  // namespace Html
};  // namespace Supla

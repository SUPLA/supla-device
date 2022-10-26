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
#include <supla/network/network.h>
#include <supla/network/web_sender.h>
#include <supla/storage/storage.h>

#include "wifi_parameters.h"

namespace Supla {

namespace Html {

WifiParameters::WifiParameters() : HtmlElement(HTML_SECTION_NETWORK) {
}
WifiParameters::~WifiParameters() {
}
void WifiParameters::send(Supla::WebSender* sender) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    sender->send("<h3>Wi-Fi Settings</h3>");
    char buf[256] = {};

    // form-field START
    sender->send("<div class=\"form-field\">");
    const char key[] = "sid";
    sender->sendLabelFor(key, "Network name");
    sender->send("<input ");
    sender->sendNameAndId(key);
    if (cfg->getWiFiSSID(buf)) {
      sender->send("value=\"");
      sender->send(buf);
    }
    sender->send("\">");
    sender->send("</div>");
    // form-field END

    // form-field START
    sender->send("<div class=\"form-field\">");
    const char keyPass[] = "wpw";
    sender->sendLabelFor(keyPass, "Password");
    sender->send("<input type=\"password\" ");
    sender->sendNameAndId(keyPass);
    sender->send(">");
    sender->send("</div>");
    // form-field END
  }
}

bool WifiParameters::handleResponse(const char* key, const char* value) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (strcmp(key, "sid") == 0) {
    cfg->setWiFiSSID(value);
    return true;
  } else if (strcmp(key, "wpw") == 0) {
    if (strlen(value) > 0) {
      cfg->setWiFiPassword(value);
    }
    return true;
  }

  return false;
}

};  // namespace Html
};  // namespace Supla

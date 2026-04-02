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

#ifndef ARDUINO_ARCH_AVR
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
    sender->tag("h3").body("Wi-Fi Settings");
    char buf[256] = {};

    if (Supla::Network::GetNetIntfCount() > 1) {
      const char wifiEn[] = "wifi_en";  // HTML field
      uint8_t wifiDisabled = 0;
      cfg->getUInt8(Supla::WifiDisableTag, &wifiDisabled);

      sender->labeledField(
          wifiEn,
          "Enable Wi-Fi",
          [&]() {
        auto switchLabel = sender->tag("label");
        switchLabel.body([&]() {
          auto sw = sender->tag("span");
          sw.attr("class", "switch");
          sw.body([&]() {
            sender->checkboxInput(wifiEn, wifiEn, wifiDisabled == 0);

            auto slider = sender->tag("span");
            slider.attr("class", "slider");
            slider.body("");
          });
        });
      },
          "form-field right-checkbox");
    }

    const char key[] = "sid";
    sender->labeledField(key, "Network name", [&]() {
      if (cfg->getWiFiSSID(buf)) {
        sender->textInput(key, key, buf);
      } else {
        sender->textInput(key, key);
      }
    });

    const char keyPass[] = "wpw";
    sender->labeledField(keyPass, "Password", [&]() {
      sender->passwordInput(keyPass, keyPass);
    });
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
  } else if (strcmp(key, "wifi_en") == 0) {
    checkboxFound = true;
    uint8_t wifiDisVale = (strncmp(value, "on", 3) == 0 ? 0 : 1);
    cfg->setUInt8(Supla::WifiDisableTag, wifiDisVale);
    return true;
  }

  return false;
}

void WifiParameters::onProcessingEnd() {
  if (!checkboxFound && Supla::Network::GetNetIntfCount() > 1) {
    // checkbox doesn't send value when it is not checked, so on processing end
    // we check if it was found earlier, and if not, then we process it as "off"
    handleResponse("wifi_en", "off");
  }
  checkboxFound = false;
}

};  // namespace Html
};  // namespace Supla

#endif  // ARDUINO_ARCH_AVR

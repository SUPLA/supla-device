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

#ifndef ARDUINO_ARCH_AVR
#include "ethernet_parameters.h"

#include <string.h>
#include <supla/network/network.h>
#include <supla/network/web_sender.h>
#include <supla/storage/storage.h>

using Supla::Html::EthernetParameters;

EthernetParameters::EthernetParameters() : HtmlElement(HTML_SECTION_NETWORK) {
}
EthernetParameters::~EthernetParameters() {
}
void EthernetParameters::send(Supla::WebSender* sender) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    const char ethEn[] = "eth_en";    // HTML field
    uint8_t ethDisabled = 0;
    cfg->getUInt8(Supla::EthDisableTag, &ethDisabled);

    sender->tag("h3").body("Ethernet Settings");
    sender->labeledField(
        ethEn,
        "Enable Ethernet",
        [&]() {
          sender->tag("label").body([&]() {
            sender->tag("span").attr("class", "switch").body([&]() {
              auto input = sender->voidTag("input");
              input.attr("type", "checkbox");
              input.attr("value", "on");
              input.attrIf("checked", ethDisabled == 0);
              input.attr("name", ethEn);
              input.attr("id", ethEn);
              input.finish();
              sender->tag("span").attr("class", "slider").body([]() {});
            });
          });
        },
        "form-field right-checkbox");
  }
}

bool EthernetParameters::handleResponse(const char* key, const char* value) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (strcmp(key, "eth_en") == 0) {
    checkboxFound = true;
    uint8_t ethDisVale = (strncmp(value, "on", 3) == 0 ? 0 : 1);
    cfg->setUInt8(Supla::EthDisableTag, ethDisVale);
    return true;
  }

  return false;
}

void EthernetParameters::onProcessingEnd() {
  if (!checkboxFound) {
    // checkbox doesn't send value when it is not checked, so on processing end
    // we check if it was found earlier, and if not, then we process it as "off"
    handleResponse("eth_en", "off");
  }
  checkboxFound = false;
}

#endif  // ARDUINO_ARCH_AVR

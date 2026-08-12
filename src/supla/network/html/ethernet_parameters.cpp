// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef ARDUINO_ARCH_AVR
#include "ethernet_parameters.h"

#include <string.h>
#include <supla/network/network.h>
#include <supla/network/web_sender.h>
#include <supla/storage/config_tags.h>
#include <supla/storage/storage.h>

using Supla::Html::EthernetParameters;

EthernetParameters::EthernetParameters()
    : HtmlElement(HTML_SECTION_NETWORK),
      netifParameters(Supla::ConfigTag::EthNetifCfgTag, "eth") {
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

    netifParameters.send(sender);
  }
}

bool EthernetParameters::handleResponse(const char* key, const char* value) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (!cfg) {
    return netifParameters.handleResponse(key, value);
  }
  if (strcmp(key, "eth_en") == 0) {
    checkboxFound = true;
    uint8_t ethDisVale = (strncmp(value, "on", 3) == 0 ? 0 : 1);
    cfg->setUInt8(Supla::EthDisableTag, ethDisVale);
    return true;
  }

  return netifParameters.handleResponse(key, value);
}

void EthernetParameters::onProcessingEnd() {
  if (!checkboxFound) {
    // checkbox doesn't send value when it is not checked, so on processing end
    // we check if it was found earlier, and if not, then we process it as "off"
    handleResponse("eth_en", "off");
  }
  checkboxFound = false;
  netifParameters.onProcessingEnd();
}

#endif  // ARDUINO_ARCH_AVR

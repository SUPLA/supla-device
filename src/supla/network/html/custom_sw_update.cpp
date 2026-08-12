// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef ARDUINO_ARCH_AVR
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
    const char key[] = "swupdateurl";
    if (cfg->getSwUpdateServer(url)) {
      sender->labeledField(key, "Update server address", [&]() {
        sender->textInput(key, key, url);
      });
    } else {
      sender->labeledField(key, "Update server address", [&]() {
        sender->textInput(key, key);
      });
    }
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

#endif  // ARDUINO_ARCH_AVR

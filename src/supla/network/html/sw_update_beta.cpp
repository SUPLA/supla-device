// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef ARDUINO_ARCH_AVR
#include "sw_update_beta.h"

#include <string.h>
#include <supla/device/sw_update.h>
#include <supla/network/web_sender.h>
#include <supla/storage/config.h>
#include <supla/storage/storage.h>
#include <supla/tools.h>

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
    bool skipCert = cfg->isSwUpdateSkipCert();

    const char key[] = "updbeta";
    sender->labeledField(key, "Firmware update", [&]() {
      sender->tag("div").body([&]() {
        auto select = sender->selectTag(key, key);
        select.body([&]() {
          sender->selectOption(0, "NO", !update);
          sender->selectOption(1, "YES", update && !beta);
          sender->selectOption(2, "YES - BETA", update && beta);
          sender->selectOption(
              3,
              "YES - ONE-TIME RECOVERY MODE (SKIP CERTIFICATE)",
              update && skipCert);
        });
        sender->tag("div")
            .attr("class", "hint")
            .body("NO: keep firmware update disabled.");
        sender->tag("div")
            .attr("class", "hint")
            .body(
                "YES: normal OTA update with HTTPS certificate verification.");
        sender->tag("div")
            .attr("class", "hint")
            .body(
                "YES - BETA: install beta firmware. Beta versions may contain "
                "bugs "
                "and your device may not work properly.");
        sender->tag("div")
            .attr("class", "hint")
            .body(
                "YES - ONE-TIME RECOVERY MODE: use only when the OTA "
                "certificate "
                "has expired. This mode is cleared automatically after the "
                "update.");
      });
    });
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
        cfg->setSwUpdateSkipCert(false);
        cfg->setSwUpdateBeta(false);
        break;
      }
      case 1: {
        cfg->setDeviceMode(DEVICE_MODE_SW_UPDATE);
        cfg->setSwUpdateSkipCert(false);
        cfg->setSwUpdateBeta(false);
        break;
      }
      case 2: {
        cfg->setDeviceMode(DEVICE_MODE_SW_UPDATE);
        cfg->setSwUpdateSkipCert(false);
        cfg->setSwUpdateBeta(true);
        break;
      }
      case 3: {
        cfg->setDeviceMode(DEVICE_MODE_SW_UPDATE);
        cfg->setSwUpdateSkipCert(true);
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

#endif  // ARDUINO_ARCH_AVR

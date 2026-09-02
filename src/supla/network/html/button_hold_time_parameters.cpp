// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef ARDUINO_ARCH_AVR
#include "button_hold_time_parameters.h"

#include <string.h>
#include <supla/network/web_sender.h>
#include <supla/storage/config.h>
#include <supla/storage/storage.h>
#include <supla/tools.h>
#include <supla/storage/config_tags.h>

using Supla::Html::ButtonHoldTimeParameters;

ButtonHoldTimeParameters::ButtonHoldTimeParameters(uint32_t defaultHoldTime)
    : HtmlElement(HTML_SECTION_FORM), defaultHoldTime(defaultHoldTime) {
}

void ButtonHoldTimeParameters::send(Supla::WebSender* sender) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    uint32_t value = defaultHoldTime;  // default value
    cfg->getUInt32(Supla::ConfigTag::BtnHoldTag, &value);
    if (value < 200) {
      value = 200;
    } else if (value > 10000) {
      value = 10000;
    }

    sender->labeledField(
        Supla::ConfigTag::BtnHoldTag,
        "Hold detection time [s]",
        [&]() {
          sender->numberInput(
              Supla::ConfigTag::BtnHoldTag,
              {
                  .min = fixed(200, 3),
                  .max = fixed(10000, 3),
                  .value = fixed(static_cast<int>(value), 3),
                  .step = fixed(100, 3),
              });
        });
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

#endif  // ARDUINO_ARCH_AVR

// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef ARDUINO_ARCH_AVR
#include "power_status_led_parameters.h"

#include <string.h>
#include <supla/network/web_sender.h>
#include <supla/storage/config.h>
#include <supla/storage/storage.h>
#include <supla/tools.h>
#include <supla/device/remote_device_config.h>
#include <supla/element.h>
#include <supla/storage/config_tags.h>

using Supla::Html::PowerStatusLedParameters;

PowerStatusLedParameters::PowerStatusLedParameters()
    : HtmlElement(HTML_SECTION_FORM) {
}

PowerStatusLedParameters::~PowerStatusLedParameters() {
}

void PowerStatusLedParameters::send(Supla::WebSender* sender) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    int8_t value = 0;
    cfg->getInt8(Supla::ConfigTag::PowerStatusLedCfgTag, &value);
    sender->labeledField(Supla::ConfigTag::PowerStatusLedCfgTag,
                         "Power Status LED", [&]() {
                           auto select = sender->selectTag(
                               Supla::ConfigTag::PowerStatusLedCfgTag,
                               Supla::ConfigTag::PowerStatusLedCfgTag);
                           select.body([&]() {
                             sender->selectOption(0, "Enabled", value == 0);
                             sender->selectOption(1, "Disabled", value == 1);
                           });
                         });
  }
}

bool PowerStatusLedParameters::handleResponse(const char* key,
                                              const char* value) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (strcmp(key, Supla::ConfigTag::PowerStatusLedCfgTag) == 0) {
    int led = stringToUInt(value);
    int8_t valueInCfg = -1;
    cfg->getInt8(Supla::ConfigTag::PowerStatusLedCfgTag, &valueInCfg);
    if (valueInCfg != led) {
      switch (led) {
        default:
        case 0: {
          cfg->setInt8(Supla::ConfigTag::PowerStatusLedCfgTag, 0);
          break;
        }
        case 1: {
          cfg->setInt8(Supla::ConfigTag::PowerStatusLedCfgTag, led);
          break;
        }
      }
      cfg->setDeviceConfigChangeFlag();
      Supla::Element::NotifyElementsAboutConfigChange(
          SUPLA_DEVICE_CONFIG_FIELD_POWER_STATUS_LED);
    }
    return true;
  }
  return false;
}

#endif  // ARDUINO_ARCH_AVR

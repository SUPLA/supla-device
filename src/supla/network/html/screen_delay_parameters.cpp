// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef ARDUINO_ARCH_AVR
#include "screen_delay_parameters.h"

#include <string.h>
#include <supla/network/web_sender.h>
#include <supla/storage/config.h>
#include <supla/storage/storage.h>
#include <supla/tools.h>
#include <supla/log_wrapper.h>
#include <supla/element.h>
#include <supla/storage/config_tags.h>

using Supla::Html::ScreenDelayParameters;

ScreenDelayParameters::ScreenDelayParameters()
    : HtmlElement(HTML_SECTION_FORM) {
}

ScreenDelayParameters::~ScreenDelayParameters() {
}

void ScreenDelayParameters::send(Supla::WebSender* sender) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    int32_t value = 0;  // default value
    cfg->getInt32(Supla::ConfigTag::ScreenDelayCfgTag, &value);
    if (value < 0) {
      value = 0;
    }
    if (value > 65535) {
      value = 65535;
    }
    sender->labeledField(Supla::ConfigTag::ScreenDelayCfgTag,
                         "Turn screen off after [sec]", [&]() {
                           sender->numberInput(
                               Supla::ConfigTag::ScreenDelayCfgTag,
                               Supla::NumericInputSpec{
                                   .min = 0,
                                   .max = 65535,
                                   .value = value,
                                   .step = 1,
                               });
                         });
  }
}

bool ScreenDelayParameters::handleResponse(const char* key,
                                                const char* value) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (strcmp(key, Supla::ConfigTag::ScreenDelayCfgTag) == 0) {
    int32_t param = stringToInt(value);
    if (param < 0) {
      param = 0;
    }
    if (param > 65535) {
      param = 65535;
    }

    int32_t currentValue = 0;
    cfg->getInt32(Supla::ConfigTag::ScreenDelayCfgTag, &currentValue);
    if (currentValue < 0) {
      currentValue = 0;
    }
    if (currentValue > 65535) {
      currentValue = 65535;
    }

    if (param != currentValue) {
      cfg->setInt32(Supla::ConfigTag::ScreenDelayCfgTag, param);
      cfg->setDeviceConfigChangeFlag();
      Supla::Element::NotifyElementsAboutConfigChange(
          SUPLA_DEVICE_CONFIG_FIELD_HOME_SCREEN_OFF_DELAY);
    }
    return true;
  }
  return false;
}

#endif  // ARDUINO_ARCH_AVR

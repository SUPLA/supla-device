// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef ARDUINO_ARCH_AVR
#include "volume_parameters.h"

#include <string.h>
#include <supla/network/web_sender.h>
#include <supla/storage/config.h>
#include <supla/storage/storage.h>
#include <supla/tools.h>
#include <supla/log_wrapper.h>
#include <supla/clock/clock.h>
#include <supla/storage/config_tags.h>

using Supla::Html::VolumeParameters;

VolumeParameters::VolumeParameters()
    : HtmlElement(HTML_SECTION_FORM) {
}

VolumeParameters::~VolumeParameters() {
}

void VolumeParameters::send(Supla::WebSender* sender) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    uint8_t value = 0;  // default value
    cfg->getUInt8(Supla::ConfigTag::VolumeCfgTag, &value);
    if (value > 100) {
      value = 100;
    }

      sender->labeledField(
        Supla::ConfigTag::VolumeCfgTag,
        "Button volume",
        [&]() {
          sender->rangeInput(
              Supla::ConfigTag::VolumeCfgTag,
              {
                  .min = 0,
                  .max = 100,
                  .value = value,
                  .step = 1,
              },
              "range-slider");
        });
  }
}

bool VolumeParameters::handleResponse(const char* key,
                                                const char* value) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (strcmp(key, Supla::ConfigTag::VolumeCfgTag) == 0) {
    int32_t param = stringToInt(value);
    if (param < 0) {
      param = 0;
    }
    if (param > 100) {
      param = 100;
    }

    uint8_t currentValue = 0;
    cfg->getUInt8(Supla::ConfigTag::VolumeCfgTag, &currentValue);
    if (currentValue > 100) {
      currentValue = 100;
    }

    if (param != currentValue) {
      cfg->setUInt8(Supla::ConfigTag::VolumeCfgTag,
                    static_cast<uint8_t>(param));
      cfg->setDeviceConfigChangeFlag();
      Supla::Element::NotifyElementsAboutConfigChange(
          SUPLA_DEVICE_CONFIG_FIELD_BUTTON_VOLUME);
    }
    return true;
  }
  return false;
}

#endif  // ARDUINO_ARCH_AVR

// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef ARDUINO_ARCH_AVR

#include "thermal_protection_parameters.h"

#include <string.h>
#include <supla-common/proto.h>
#include <supla/element.h>
#include <supla/network/web_sender.h>
#include <supla/storage/config.h>
#include <supla/storage/config_tags.h>
#include <supla/storage/storage.h>
#include <supla/tools.h>

using Supla::Html::ThermalProtectionParameters;

namespace {
const char ThermalProtectionEnabledTag[] = "thermal_en";
const char ThermalProtectionThresholdTag[] = "thermal_thr";
}  // namespace

ThermalProtectionParameters::ThermalProtectionParameters(
    const Supla::Device::ThermalProtectionProperties &properties)
    : HtmlElement(HTML_SECTION_FORM), properties(properties) {
  this->properties.disableAllowed = properties.disableAllowed ? 1 : 0;
}

ThermalProtectionParameters::~ThermalProtectionParameters() {
}

void ThermalProtectionParameters::loadConfig() {
  if (configLoaded) {
    return;
  }

  originalConfig = {};
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    cfg->getBlob(Supla::ConfigTag::ThermalProtectionCfgTag,
                 reinterpret_cast<char *>(&originalConfig),
                 sizeof(originalConfig));
  }
  pendingConfig = originalConfig;
  if (!properties.disableAllowed) {
    pendingConfig.enabled = 1;
  }
  configLoaded = true;
}

void ThermalProtectionParameters::send(Supla::WebSender *sender) {
  configLoaded = false;
  loadConfig();

  if (properties.disableAllowed) {
    sender->formField(
        [&]() {
          sender->labelFor(ThermalProtectionEnabledTag,
                           "Enable thermal protection");
          auto label = sender->tag("label");
          label.body([&]() {
            auto switchSpan = sender->tag("span");
            switchSpan.attr("class", "switch").body([&]() {
              auto input = sender->voidTag("input");
              input.attr("type", "checkbox")
                  .attr("value", "on")
                  .attrIf("checked", pendingConfig.enabled != 0)
                  .attr("name", ThermalProtectionEnabledTag)
                  .attr("id", ThermalProtectionEnabledTag)
                  .finish();
              sender->tag("span").attr("class", "slider").body("");
            });
          });
        },
        "form-field right-checkbox");
  }

  sender->labeledField(
      ThermalProtectionThresholdTag,
      "Thermal protection threshold [°C]",
      [&]() {
        sender->numberInput(
            ThermalProtectionThresholdTag,
            ThermalProtectionThresholdTag,
            {
                .min = Supla::fixed(properties.minThreshold, 1),
                .max = Supla::fixed(properties.maxThreshold, 1),
                .value = Supla::fixed(pendingConfig.threshold, 1),
                .step = Supla::fixed(1, 1),
            });
      });
}

bool ThermalProtectionParameters::handleResponse(const char *key,
                                                 const char *value) {
  if (key == nullptr || value == nullptr) {
    return false;
  }

  if (strcmp(key, ThermalProtectionThresholdTag) == 0) {
    loadConfig();
    int32_t threshold = floatStringToInt(value, 1);
    if (threshold < properties.minThreshold) {
      threshold = properties.minThreshold;
    } else if (threshold > properties.maxThreshold) {
      threshold = properties.maxThreshold;
    }
    pendingConfig.threshold = static_cast<int16_t>(threshold);
    thresholdFound = true;
    return true;
  }

  if (strcmp(key, ThermalProtectionEnabledTag) == 0) {
    loadConfig();
    if (properties.disableAllowed) {
      pendingConfig.enabled = strcmp(value, "on") == 0 ? 1 : 0;
      enabledFound = true;
    }
    return true;
  }

  return false;
}

void ThermalProtectionParameters::onProcessingEnd() {
  if (!thresholdFound && !enabledFound) {
    configLoaded = false;
    return;
  }

  if (properties.disableAllowed && thresholdFound && !enabledFound) {
    pendingConfig.enabled = 0;
  }

  if (pendingConfig != originalConfig) {
    auto cfg = Supla::Storage::ConfigInstance();
    if (cfg && cfg->setBlob(Supla::ConfigTag::ThermalProtectionCfgTag,
                            reinterpret_cast<const char *>(&pendingConfig),
                            sizeof(pendingConfig))) {
      cfg->setDeviceConfigChangeFlag();
      Supla::Element::NotifyElementsAboutConfigChange(
          SUPLA_DEVICE_CONFIG_FIELD_THERMAL_PROTECTION);
    }
  }

  configLoaded = false;
  thresholdFound = false;
  enabledFound = false;
}

#endif  // ARDUINO_ARCH_AVR

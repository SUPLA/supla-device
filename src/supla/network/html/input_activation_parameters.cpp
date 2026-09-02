// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef ARDUINO_ARCH_AVR

#include "input_activation_parameters.h"

#include <string.h>

#include <supla-common/proto.h>
#include <supla/element.h>
#include <supla/network/web_sender.h>
#include <supla/storage/config.h>
#include <supla/storage/config_tags.h>
#include <supla/storage/storage.h>

using Supla::Html::InputActivationParameters;

namespace {

const char InputActivationModeTag[] = "input_act";
constexpr uint8_t InputActivationKnownModes =
SUPLA_DEVCFG_INPUT_ACTIVATION_GND |
SUPLA_DEVCFG_INPUT_ACTIVATION_VCC;

bool isValidMode(uint8_t mode, uint8_t availableModes) {
  return mode != 0 && (mode & (mode - 1)) == 0 &&
         (mode & ~InputActivationKnownModes) == 0 &&
         (mode & availableModes) != 0;
}

uint8_t knownAvailableModes(uint8_t availableModes) {
  return availableModes & InputActivationKnownModes;
}

int modeCount(uint8_t availableModes) {
  int count = 0;
  for (uint8_t mode = SUPLA_DEVCFG_INPUT_ACTIVATION_GND;
       mode <= SUPLA_DEVCFG_INPUT_ACTIVATION_VCC; mode <<= 1) {
    if (availableModes & mode) {
      count++;
    }
  }
  return count;
}

}  // namespace

InputActivationParameters::InputActivationParameters(
    const Supla::Device::InputActivationProperties &properties)
    : HtmlElement(HTML_SECTION_FORM), properties(properties) {
}

InputActivationParameters::~InputActivationParameters() {
}

void InputActivationParameters::loadConfig() {
  if (configLoaded) {
    return;
  }

  const uint8_t availableModes = knownAvailableModes(properties.availableModes);
  originalConfig = {};
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    cfg->getBlob(Supla::ConfigTag::InputActivationCfgTag,
                 reinterpret_cast<char *>(&originalConfig),
                 sizeof(originalConfig));
  }

  if (!isValidMode(originalConfig.mode, availableModes)) {
    originalConfig.mode = isValidMode(properties.defaultMode, availableModes)
                              ? properties.defaultMode
                              : 0;
  }
  pendingConfig = originalConfig;
  configLoaded = true;
}

void InputActivationParameters::send(Supla::WebSender *sender) {
  configLoaded = false;
  loadConfig();

  const uint8_t availableModes = knownAvailableModes(properties.availableModes);
  if (sender == nullptr || modeCount(availableModes) < 2 ||
      !isValidMode(pendingConfig.mode, availableModes)) {
    return;
  }

  sender->labeledField(
      InputActivationModeTag, "Input activation", [&]() {
        sender->selectInput(InputActivationModeTag, InputActivationModeTag,
                            [&]() {
                              if (availableModes &
                                  SUPLA_DEVCFG_INPUT_ACTIVATION_GND) {
                                sender->selectOption(
                                    SUPLA_DEVCFG_INPUT_ACTIVATION_GND,
                                    "GND — SIG",
                                    pendingConfig.mode ==
                                        SUPLA_DEVCFG_INPUT_ACTIVATION_GND);
                              }
                              if (availableModes &
                                  SUPLA_DEVCFG_INPUT_ACTIVATION_VCC) {
                                sender->selectOption(
                                    SUPLA_DEVCFG_INPUT_ACTIVATION_VCC,
                                    "VCC — SIG",
                                    pendingConfig.mode ==
                                        SUPLA_DEVCFG_INPUT_ACTIVATION_VCC);
                              }
                            });
      });
}

bool InputActivationParameters::handleResponse(const char *key,
                                                const char *value) {
  if (key == nullptr || value == nullptr ||
      strcmp(key, InputActivationModeTag) != 0) {
    return false;
  }

  uint8_t mode = 0;
  if (strcmp(value, "1") == 0) {
    mode = SUPLA_DEVCFG_INPUT_ACTIVATION_GND;
  } else if (strcmp(value, "2") == 0) {
    mode = SUPLA_DEVCFG_INPUT_ACTIVATION_VCC;
  } else {
    return false;
  }

  const uint8_t availableModes = knownAvailableModes(properties.availableModes);
  if (!isValidMode(mode, availableModes)) {
    return false;
  }

  loadConfig();
  pendingConfig.mode = mode;
  modeFound = true;
  return true;
}

void InputActivationParameters::onProcessingEnd() {
  if (modeFound && pendingConfig != originalConfig) {
    auto cfg = Supla::Storage::ConfigInstance();
    if (cfg && cfg->setBlob(
                   Supla::ConfigTag::InputActivationCfgTag,
                   reinterpret_cast<const char *>(&pendingConfig),
                   sizeof(pendingConfig))) {
      cfg->setDeviceConfigChangeFlag();
      Supla::Element::NotifyElementsAboutConfigChange(
          SUPLA_DEVICE_CONFIG_FIELD_INPUT_ACTIVATION);
    }
  }

  configLoaded = false;
  modeFound = false;
}

#endif  // ARDUINO_ARCH_AVR

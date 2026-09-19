// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef ARDUINO_ARCH_AVR
#include "button_action_trigger_config.h"

#include <string.h>
#include <supla/network/web_sender.h>
#include <supla/storage/config.h>
#include <supla/storage/storage.h>
#include <supla/tools.h>
#include <supla/storage/config_tags.h>

#include <stdio.h>

namespace Supla {

namespace Html {

ButtonActionTriggerConfig::ButtonActionTriggerConfig(int channelNumber,
    int buttonNumber, const char* labelPrefix) :
  HtmlElement(HTML_SECTION_FORM),
  channelNumber(channelNumber),
  buttonNumber(buttonNumber) {
  if (labelPrefix) {
    int size = strlen(labelPrefix);
    this->labelPrefix = new char[size + 1];
    if (this->labelPrefix) {
      snprintf(this->labelPrefix, size + 1, "%s", labelPrefix);
    }
  }
}

ButtonActionTriggerConfig::~ButtonActionTriggerConfig() {
  if (labelPrefix) {
    delete[] labelPrefix;
    labelPrefix = nullptr;
  }
}

void ButtonActionTriggerConfig::send(Supla::WebSender* sender) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    int32_t value = 0;
    int32_t localUnlock = 0;
    char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    Supla::Config::generateKey(key, channelNumber,
                             Supla::ConfigTag::BtnActionTriggerCfgTagPrefix);
    cfg->getInt32(key, &value);
    char localUnlockKey[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    Supla::Config::generateKey(
        localUnlockKey,
        channelNumber,
        Supla::ConfigTag::BtnActionTriggerLocalUnlockTagPrefix);
    cfg->getInt32(localUnlockKey, &localUnlock);

    char label[100] = {};
    if (labelPrefix) {
      snprintf(
          label, sizeof(label), "%s MQTT action trigger type", labelPrefix);
    } else {
      snprintf(label, sizeof(label), "IN%d MQTT action trigger type",
          buttonNumber);
    }

    sender->labeledField(key, label, [&]() {
      auto select = sender->selectTag(key, key);
      select.body([&]() {
        sender->selectOption(
            0,
            "Publish based on Supla Cloud config",
            value == 0);
        sender->selectOption(
            1,
            "Publish all triggers, don't disable local function",
            value == 1);
        sender->selectOption(
            2,
            "Publish all triggers, disable local function",
            value == 2);
      });
    });

    char unlockLabel[100] = {};
    if (labelPrefix) {
      snprintf(unlockLabel,
               sizeof(unlockLabel),
               "%s local unlock while locked",
               labelPrefix);
    } else {
      snprintf(unlockLabel,
               sizeof(unlockLabel),
               "IN%d local unlock while locked",
               buttonNumber);
    }
    sender->labeledField(localUnlockKey, unlockLabel, [&]() {
      auto select = sender->selectTag(localUnlockKey, localUnlockKey);
      select.body([&]() {
        sender->selectOption(0, "Disabled", localUnlock == 0);
        sender->selectOption(1, "Enabled", localUnlock != 0);
      });
    });
  }
}

bool ButtonActionTriggerConfig::handleResponse(const char* key,
                                               const char* value) {
  auto cfg = Supla::Storage::ConfigInstance();
  char keyRef[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
  Supla::Config::generateKey(keyRef, channelNumber,
      Supla::ConfigTag::BtnActionTriggerCfgTagPrefix);
  if (strcmp(key, keyRef) == 0) {
    int atType = stringToUInt(value);
    switch (atType) {
      default: {
        cfg->setInt32(keyRef, 0);
        break;
      }
      case 0:
      case 1:
      case 2: {
        cfg->setInt32(keyRef, atType);
        break;
      }
    }
    return true;
  }
  Supla::Config::generateKey(
      keyRef,
      channelNumber,
      Supla::ConfigTag::BtnActionTriggerLocalUnlockTagPrefix);
  if (strcmp(key, keyRef) == 0) {
    int unlock = stringToUInt(value);
    cfg->setInt32(keyRef, unlock == 0 ? 0 : 1);
    return true;
  }
  return false;
}

};  // namespace Html
};  // namespace Supla


#endif  // ARDUINO_ARCH_AVR

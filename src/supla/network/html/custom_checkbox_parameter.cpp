// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef ARDUINO_ARCH_AVR
#include "custom_checkbox_parameter.h"

#include <stdio.h>
#include <string.h>
#include <supla/network/web_sender.h>
#include <supla/storage/config.h>
#include <supla/storage/storage.h>
#include <supla/tools.h>
#include <supla/log_wrapper.h>

using Supla::Html::CustomCheckboxParameter;

CustomCheckboxParameter::CustomCheckboxParameter(const char* paramTag,
                                                 const char* paramLabel,
                                                 uint8_t defaultValue)
    : HtmlElement(HTML_SECTION_FORM), checkboxValue(defaultValue) {
  setTag(paramTag);
  setLabel(paramLabel);
}

void CustomCheckboxParameter::setTag(const char* tagValue) {
  if (tag != nullptr) {
    delete []tag;
    tag = nullptr;
  }
  if (tagValue == nullptr) {
    return;
  }

  auto size = strnlen(tagValue, SUPLA_CONFIG_MAX_KEY_SIZE);
  if (size >= SUPLA_CONFIG_MAX_KEY_SIZE) {
    size = SUPLA_CONFIG_MAX_KEY_SIZE - 1;
    SUPLA_LOG_WARNING("Tag too long: \"%s\"; truncating", tagValue);
  }
  if (size == 0) {
    return;
  }
  tag = new char[size + 1];
  memcpy(tag, tagValue, size);
  tag[size] = '\0';
}

void CustomCheckboxParameter::setLabel(const char *labelValue) {
  if (label != nullptr) {
    delete []label;
    label = nullptr;
  }
  if (labelValue == nullptr) {
    return;
  }

  auto size = strnlen(labelValue, MAX_LABEL_SIZE);
  if (size == 0) {
    return;
  }
  if (size < MAX_LABEL_SIZE) {
    label = new char[size + 1];
    strncpy(label, labelValue, size + 1);
  }
}

CustomCheckboxParameter::~CustomCheckboxParameter() {
  if (tag != nullptr) {
    delete []tag;
    tag = nullptr;
  }
  if (label != nullptr) {
    delete []label;
    label = nullptr;
  }
}

void CustomCheckboxParameter::send(Supla::WebSender* sender) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    cfg->getUInt8(tag, &checkboxValue);
    sender->formField(
        [&]() {
          sender->labelFor(tag, label);
          auto wrapper = sender->tag("label");
          wrapper.body([&]() {
            auto switchTag = sender->tag("span");
            switchTag.attr("class", "switch");
            switchTag.body([&]() {
              sender->checkboxInput(tag, tag, checkboxValue);
              sender->tag("span").attr("class", "slider").body("");
            });
          });
        },
        "form-field right-checkbox");
  }
}

bool CustomCheckboxParameter::handleResponse(
                                          const char* key, const char* value) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg && tag != nullptr &&
                           strncmp(key, tag, SUPLA_CONFIG_MAX_KEY_SIZE) == 0) {
    checkboxFound = true;
    uint8_t inFormValue = (strncmp(value, "on", 3) == 0 ? 1 : 0);
    cfg->setUInt8(tag, inFormValue);
    return true;
  }
  return false;
}

void CustomCheckboxParameter::onProcessingEnd() {
  if (!checkboxFound) {
    // checkbox doesn't send value when it is not checked, so on processing end
    // we check if it was found earlier, and if not, then we process it as "off"
    handleResponse(tag, "off");
  }
  checkboxFound = false;
}

#endif  // ARDUINO_ARCH_AVR

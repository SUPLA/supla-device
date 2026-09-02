// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef ARDUINO_ARCH_AVR
#include "custom_text_parameter.h"

#include <stdio.h>
#include <string.h>
#include <supla/network/web_sender.h>
#include <supla/storage/config.h>
#include <supla/storage/storage.h>
#include <supla/tools.h>
#include <supla/log_wrapper.h>

namespace Supla {

namespace Html {

CustomTextParameter::CustomTextParameter(const char *paramTag,
                                         const char *paramLabel,
                                         int maxSize)
    : HtmlElement(HTML_SECTION_FORM), maxSize(maxSize) {
  strncpy(tag, paramTag, sizeof(tag) - 1);

  int size = strlen(paramLabel);
  if (size < 500) {
    label = new char[size + 1];
    strncpy(label, paramLabel, size + 1);
  }
}

CustomTextParameter::~CustomTextParameter() {
  if (label != nullptr) {
    delete []label;
    label = nullptr;
  }
}

void CustomTextParameter::send(Supla::WebSender* sender) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    int size = cfg->getStringSize(tag);
    char *value = nullptr;
    if (size > 0) {
      value = new char[size + 1];
      memset(value, 0, size + 1);
      cfg->getString(tag, value, size);
    }
    sender->labeledField(tag, label, [&]() {
      sender->textInput(tag, tag, value, maxSize);
    });

    if (value) {
      delete[] value;
      value = nullptr;
    }
  }
}
bool CustomTextParameter::handleResponse(const char* key, const char* value) {
  if (strcmp(key, tag) == 0) {
    setParameterValue(value);
    return true;
  }
  return false;
}

bool CustomTextParameter::getParameterValue(char *buf, const int size) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    if (cfg->getString(tag, buf, size)) {
      return true;
    }
  }
  return false;
}

void CustomTextParameter::setParameterValue(const char *newValue) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    int size = strnlen(newValue, maxSize + 1);
    if (size > maxSize) {
      return;
    }
    cfg->setString(tag, newValue);
    cfg->saveWithDelay(1000);
  }
}

};  // namespace Html
};  // namespace Supla

#endif  // ARDUINO_ARCH_AVR

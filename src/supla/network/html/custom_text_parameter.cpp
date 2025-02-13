/*
 * Copyright (C) AC SOFTWARE SP. Z O.O
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

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
    // form-field BEGIN
    sender->send("<div class=\"form-field\">");
    sender->sendLabelFor(tag, label);
    sender->send("<input type=\"text\" maxlength=\"");
    sender->send(maxSize);
    sender->send("\"");
    sender->sendNameAndId(tag);
    if (value) {
      sender->send(" value=\"");
      sender->sendSafe(value);
      sender->send("\"");
      delete[] value;
      value = nullptr;
    }
    sender->send(">");
    sender->send("</div>");
    // form-field END
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


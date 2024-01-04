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
  auto size = strnlen(tagValue, SUPLA_CONFIG_MAX_KEY_SIZE);
  if (size >= SUPLA_CONFIG_MAX_KEY_SIZE) {
    size = SUPLA_CONFIG_MAX_KEY_SIZE - 1;
    SUPLA_LOG_WARNING("Tag too long: \"%s\"; truncating", tagValue);
  }
  if (tag != nullptr) {
    delete []tag;
    tag = nullptr;
  }
  if (tagValue == nullptr || size == 0) {
    return;
  }
  tag = new char[size + 1];
  strncpy(tag, tagValue, size + 1);
}

void CustomCheckboxParameter::setLabel(const char *labelValue) {
  auto size = strnlen(labelValue, MAX_LABEL_SIZE);
  if (label != nullptr) {
    delete []label;
    label = nullptr;
  }
  if (labelValue == nullptr || size == 0) {
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
    // form-field BEGIN
    sender->send("<div class=\"form-field right-checkbox\">");
    sender->sendLabelFor(tag, label);
    sender->send("<label>");
    sender->send("<span class=\"switch\">");
    sender->send("<input type=\"checkbox\" value=\"on\" ");
    sender->send(checked(checkboxValue));
    sender->sendNameAndId(tag);
    sender->send(">");
    sender->send("<span class=\"slider\"></span>");
    sender->send("</span>");
    sender->send("</label>");
    sender->send("</div>");
    // form-field END
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

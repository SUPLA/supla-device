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

#include "custom_parameter.h"

#include <stdio.h>
#include <string.h>
#include <supla/network/web_sender.h>
#include <supla/storage/config.h>
#include <supla/storage/storage.h>
#include <supla/tools.h>
#include <supla/log_wrapper.h>

#include "button_multiclick_parameters.h"

using Supla::Html::CustomParameter;

CustomParameter::CustomParameter(const char* paramTag,
                                 const char* paramLabel,
                                 int32_t defaultValue)
    : HtmlElement(HTML_SECTION_FORM), parameterValue(defaultValue) {
  int size = strlen(paramTag);
  if (size < 500) {
    tag = new char[size + 1];
    strncpy(tag, paramTag, size + 1);
  }

  size = strlen(paramLabel);
  if (size < 500) {
    label = new char[size + 1];
    strncpy(label, paramLabel, size + 1);
  }
}

CustomParameter::~CustomParameter() {
  if (tag != nullptr) {
    delete []tag;
    tag = nullptr;
  }
  if (label != nullptr) {
    delete []label;
    label = nullptr;
  }
}

void CustomParameter::send(Supla::WebSender* sender) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    cfg->getInt32(tag, &parameterValue);
    // form-field BEGIN
    sender->send("<div class=\"form-field\">");
    sender->sendLabelFor(tag, label);
    sender->send("<input type=\"number\" step=\"1\" ");
    sender->sendNameAndId(tag);
    sender->send(" value=\"");
    char buf[100] = {};
    snprintf(buf, sizeof(buf), "%d", static_cast<int>(parameterValue));
    sender->send(buf);
    sender->send("\">");
    sender->send("</div>");
    // form-field END
  }
}

bool CustomParameter::handleResponse(const char* key, const char* value) {
  if (tag != nullptr && strcmp(key, tag) == 0) {
    int32_t param = stringToInt(value);
    setParameterValue(param);
    return true;
  }
  return false;
}

int32_t CustomParameter::getParameterValue() {
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    cfg->getInt32(tag, &parameterValue);
  }
  return parameterValue;
}

void CustomParameter::setParameterValue(const int32_t newValue) {
  parameterValue = newValue;

  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    cfg->setInt32(tag, parameterValue);
    cfg->saveWithDelay(1000);
  }
}

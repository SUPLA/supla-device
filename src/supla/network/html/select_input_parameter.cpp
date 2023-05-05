/*
   Copyright (C) AC SOFTWARE SP. Z O.O

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
   */

#include "select_input_parameter.h"

#include <stdio.h>
#include <string.h>
#include <supla/network/web_sender.h>
#include <supla/storage/storage.h>
#include <supla/tools.h>
#include <supla/log_wrapper.h>
#include <supla/storage/config.h>

#include "button_multiclick_parameters.h"

namespace Supla {

namespace Html {

SelectValueMapElement::~SelectValueMapElement() {
  if (name) {
    delete[] name;
  }
  name = nullptr;
}

SelectInputParameter::SelectInputParameter(const char *paramTag,
                                             const char *paramLabel)
    : HtmlElement(HTML_SECTION_FORM) {
  int size = strlen(paramTag);
  if (size >= SUPLA_CONFIG_MAX_KEY_SIZE) {
    size = SUPLA_CONFIG_MAX_KEY_SIZE - 1;
    SUPLA_LOG_WARNING("Tag too long: \"%s\"; truncating", paramTag);
  }

  tag = new char[size + 1];
  strncpy(tag, paramTag, size + 1);

  size = strlen(paramLabel);
  if (size < 500) {
    label = new char[size + 1];
    strncpy(label, paramLabel, size + 1);
  }
}

SelectInputParameter::~SelectInputParameter() {
  if (tag != nullptr) {
    delete []tag;
    tag = nullptr;
  }
  if (label != nullptr) {
    delete []label;
    label = nullptr;
  }
  auto ptr = firstValue;
  while (ptr) {
    auto next = ptr->next;
    delete ptr;
    ptr = next;
  }
}

void SelectInputParameter::send(Supla::WebSender* sender) {
  auto cfg = Supla::Storage::ConfigInstance();
  int32_t value = 0;
  if (cfg) {
    cfg->getInt32(tag, &value);
  }

  // form-field BEGIN
  sender->send("<div class=\"form-field\">");
  sender->sendLabelFor(tag, label);
  sender->send("<select ");
  sender->sendNameAndId(tag);
  sender->send(">");
  auto ptr = firstValue;
  while (ptr) {
    sender->send("<option value=\"");
    sender->send(ptr->name);
    sender->send("\"");
    if (value == ptr->value) {
      sender->send(" selected");
    }
    sender->send(">");
    sender->send(ptr->name);
    sender->send("</option>");
    ptr = ptr->next;
  }
  sender->send("</select>");
  sender->send("</div>");
  // form-field END
}

bool SelectInputParameter::handleResponse(const char* key, const char* value) {
  if (tag != nullptr && strcmp(key, tag) == 0) {
    auto cfg = Supla::Storage::ConfigInstance();
    auto ptr = firstValue;
    while (ptr) {
      if (strncmp(ptr->name, value, strlen(ptr->name) + 1) == 0) {
        // we set value registered for a given name
        if (cfg) {
          cfg->setInt32(tag, ptr->value);
        }
        return true;
      }
      ptr = ptr->next;
    }
  }
  return false;
}

void SelectInputParameter::registerValue(const char* name, int value) {
  auto ptr = firstValue;

  if (ptr == nullptr) {
    ptr = new SelectValueMapElement;
    firstValue = ptr;
  } else {
    while (ptr->next) {
      ptr = ptr->next;
    }
    ptr->next = new SelectValueMapElement;
    ptr = ptr->next;
  }

  ptr->value = value;
  int size = strlen(name) + 1;
  ptr->name = new char[size];
  snprintf(ptr->name, size, "%s", name);
}

};  // namespace Html
};  // namespace Supla


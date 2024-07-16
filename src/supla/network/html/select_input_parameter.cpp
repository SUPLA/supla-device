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

namespace Supla {

namespace Html {

SelectValueMapElement::~SelectValueMapElement() {
  if (name) {
    delete[] name;
  }
  name = nullptr;
}

SelectInputParameter::SelectInputParameter() : HtmlElement(HTML_SECTION_FORM) {
}

SelectInputParameter::SelectInputParameter(const char* paramTag,
                                           const char* paramLabel)
    : HtmlElement(HTML_SECTION_FORM) {
  setTag(paramTag);
  setLabel(paramLabel);
}

void SelectInputParameter::setTag(const char* tagValue) {
  auto size = strlen(tagValue);
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

void SelectInputParameter::setLabel(const char *labelValue) {
  int size = 0;
  if (labelValue != nullptr) {
    size = strlen(labelValue);
  }

  if (label != nullptr) {
    delete []label;
    label = nullptr;
  }

  if (labelValue == nullptr || size == 0) {
    return;
  }

  if (size < 500) {
    label = new char[size + 1];
    strncpy(label, labelValue, size + 1);
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
  if (tag == nullptr || label == nullptr) {
    return;
  }

  auto cfg = Supla::Storage::ConfigInstance();
  int32_t value = 0;
  if (cfg) {
    int8_t value8 = 0;
    switch (baseTypeBitCount) {
      case 8:
        cfg->getInt8(tag, &value8);
        value = value8;
        break;
      default:
      case 32:
        cfg->getInt32(tag, &value);
        break;
    }
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
          int32_t valueInCfg = 0;
          int8_t value8 = 0;
          switch (baseTypeBitCount) {
            case 8:
              cfg->getInt8(tag, &value8);
              valueInCfg = value8;
              break;
            default:
            case 32:
              cfg->getInt32(tag, &valueInCfg);
              break;
          }
          if (valueInCfg != ptr->value) {
            switch (baseTypeBitCount) {
              case 8:
                cfg->setInt8(tag, ptr->value);
                break;
              default:
              case 32:
                cfg->setInt32(tag, ptr->value);
                break;
            }
            configChanged = true;
          }
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

void SelectInputParameter::onProcessingEnd() {
  configChanged = false;
}

void SelectInputParameter::setBaseTypeBitCount(uint8_t value) {
  baseTypeBitCount = value;
}

};  // namespace Html
};  // namespace Supla


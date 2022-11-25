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

#include "text_cmd_input_parameter.h"

#include <stdio.h>
#include <string.h>
#include <supla/network/web_sender.h>
#include <supla/storage/storage.h>
#include <supla/tools.h>
#include <supla/log_wrapper.h>

#include "button_multiclick_parameters.h"

namespace Supla {

namespace Html {

RegisteredCmdActionMap::~RegisteredCmdActionMap() {
  if (cmd) {
    delete[] cmd;
  }
  cmd = nullptr;
}

TextCmdInputParameter::TextCmdInputParameter(const char *paramTag,
                                             const char *paramLabel)
    : HtmlElement(HTML_SECTION_FORM) {
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

TextCmdInputParameter::~TextCmdInputParameter() {
  if (tag != nullptr) {
    delete []tag;
    tag = nullptr;
  }
  if (label != nullptr) {
    delete []label;
    label = nullptr;
  }
  auto ptr = firstCmd;
  while (ptr) {
    auto next = ptr->next;
    delete ptr;
    ptr = next;
  }
}

void TextCmdInputParameter::send(Supla::WebSender* sender) {
  // form-field BEGIN
  sender->send("<div class=\"form-field\">");
  sender->sendLabelFor(tag, label);
  sender->send("<input type=\"text\" ");
  sender->sendNameAndId(tag);
  sender->send(">");
  sender->send("</div>");
  // form-field END
}

bool TextCmdInputParameter::handleResponse(const char* key, const char* value) {
  if (tag != nullptr && strcmp(key, tag) == 0) {
    auto ptr = firstCmd;
    while (ptr) {
      if (strncmp(ptr->cmd, value, strlen(ptr->cmd) + 1) == 0) {
        runAction(ptr->eventId);
        return true;
      }
      ptr = ptr->next;
    }
  }
  return false;
}

void TextCmdInputParameter::registerCmd(const char* cmd, int eventId) {
  auto ptr = firstCmd;

  if (ptr == nullptr) {
    ptr = new RegisteredCmdActionMap;
    firstCmd = ptr;
  } else {
    while (ptr->next) {
      ptr = ptr->next;
    }
    ptr->next = new RegisteredCmdActionMap;
    ptr = ptr->next;
  }

  ptr->eventId = eventId;
  int size = strlen(cmd) + 1;
  ptr->cmd = new char[size];
  snprintf(ptr->cmd, size, "%s", cmd);
}

};  // namespace Html
};  // namespace Supla



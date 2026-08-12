// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef ARDUINO_ARCH_AVR
#include "div.h"

#include <string.h>
#include <supla/network/web_sender.h>
#include <stdio.h>
#include <stdint.h>


namespace Supla {

namespace Html {

DivBegin::DivBegin(const char *className, const char *idName) :
  HtmlElement(HTML_SECTION_FORM) {
    if (className) {
      int size = strlen(className);
      this->className = new char[size + 1];
      if (this->className) {
        snprintf(this->className, size + 1, "%s", className);
      }
    }
    if (idName) {
      int size = strlen(idName);
      this->idName = new char[size + 1];
      if (this->idName) {
        snprintf(this->idName, size + 1, "%s", idName);
      }
    }
}

DivBegin::~DivBegin() {
  if (className) {
    delete[] className;
    className = nullptr;
  }
  if (idName) {
    delete[] idName;
    idName = nullptr;
  }
}

void DivBegin::send(Supla::WebSender* sender) {
  auto div = sender->tag("div");
  if (className != nullptr) {
    div.attr("class", className);
  }
  if (idName != nullptr) {
    div.attr("id", idName);
  }
  div.close().finish();
}

void DivEnd::send(Supla::WebSender* sender) {
  sender->send("</div>");
}

};  // namespace Html
};  // namespace Supla

#endif  // ARDUINO_ARCH_AVR

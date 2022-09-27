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
  sender->send("<div");
  if (className != nullptr) {
    sender->send(" class=\"");
    sender->send(className);
    sender->send("\"");
  }
  if (idName != nullptr) {
    sender->send(" id=\"");
    sender->send(idName);
    sender->send("\"");
  }
  sender->send(">");
}

void DivEnd::send(Supla::WebSender* sender) {
    sender->send("</div>");
}

};  // namespace Html
};  // namespace Supla


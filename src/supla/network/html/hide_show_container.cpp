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

#ifndef ARDUINO_ARCH_AVR
#include "hide_show_container.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <supla/network/web_sender.h>
#include <inttypes.h>

namespace Supla {

namespace Html {

HideShowContainerBegin::HideShowContainerBegin(const char* displayName)
    : HtmlElement(HTML_SECTION_FORM) {
  int size = strlen(displayName);
  name = new char[size + 1];
  if (name) {
    snprintf(name, size + 1, "%s", displayName);
  }
}

HideShowContainerBegin::~HideShowContainerBegin() {
  if (name) {
    delete[] name;
    name = nullptr;
  }
}

void HideShowContainerBegin::send(Supla::WebSender* sender) {
  char idStr[50] = {};
  snprintf(idStr,
           sizeof(idStr),
           "%" PRIuPTR,
           reinterpret_cast<uintptr_t>(this));

  char linkId[60] = {};
  snprintf(linkId, sizeof(linkId), "%s_link", idStr);

  char onclick[240] = {};
  snprintf(onclick,
           sizeof(onclick),
           "document.getElementById(\"%s\").style.display=\"block\";"
           "document.getElementById(\"%s\").style.display=\"none\";"
           "return false;",
           idStr,
           linkId);

  auto link = sender->tag("div");
  link.attr("id", linkId);
  link.body([&]() {
    auto button = sender->tag("button");
    button.attr("onclick", onclick);
    button.body([&]() {
      sender->send("Show ");
      sender->sendSafe(name ? name : "");
    });
  });

  sender->tag("div").attr("id", idStr).attr("style", "display:none").body("");
}

void HideShowContainerEnd::send(Supla::WebSender* sender) {
  sender->send("</div>");
}

};  // namespace Html
};  // namespace Supla

#endif  // ARDUINO_ARCH_AVR

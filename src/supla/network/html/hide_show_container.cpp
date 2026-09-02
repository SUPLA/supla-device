// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

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

  auto box = sender->tag("div", false);
  box.attr("id", idStr).attr("style", "display:none");
  box.close();
}

void HideShowContainerEnd::send(Supla::WebSender* sender) {
  sender->send("</div>");
}

};  // namespace Html
};  // namespace Supla

#endif  // ARDUINO_ARCH_AVR

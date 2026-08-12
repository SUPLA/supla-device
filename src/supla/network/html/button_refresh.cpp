// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef ARDUINO_ARCH_AVR
#include "button_refresh.h"

#include <supla/network/web_sender.h>

using Supla::Html::ButtonRefresh;

ButtonRefresh::ButtonRefresh() : HtmlElement(HTML_SECTION_BUTTON_BEFORE) {
}

void ButtonRefresh::send(Supla::WebSender* sender) {
  sender->send(
      "<button type=\"button\" onclick=\"location.reload();\">"
      "REFRESH"
      "</button>");
}

#endif  // ARDUINO_ARCH_AVR

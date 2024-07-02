/*
 Copyright (C) AC SOFTWARE SP. Z O.O.

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

#include <stdio.h>
#include <string.h>

#include "web_sender.h"

namespace Supla {

WebSender::~WebSender() {}

void WebSender::send(int number) {
  char buf[100];
  snprintf(buf, sizeof(buf), "%d", number);
  send(buf);
}

void WebSender::send(int number, int precision) {
  if (precision < 0) {
    precision = 0;
  }
  char buf[100];
  int divider = 1;
  int printPrecission = precision;
  for (int i = 0; i < precision; i++) {
    divider *= 10;
    if (number % divider == 0) {
      printPrecission--;
    }
  }

  snprintf(buf, sizeof(buf),
      "%.*f", printPrecission, static_cast<float>(number) / divider);
  send(buf);
}

void WebSender::sendNameAndId(const char *id) {
  char buf[100];
  snprintf(buf, sizeof(buf), " name=\"%s\" id=\"%s\" ",
      id ? id : "", id ? id : "");
  send(buf);
}

void WebSender::sendLabelFor(const char *id, const char *label) {
  char buf[300];
  snprintf(buf, sizeof(buf), "<label for=\"%s\">%s</label>", id ? id : "",
      label ? label : "");
  send(buf);
}

void WebSender::sendSafe(const char *buf, int size) {
  if (size == -1) {
    size = strnlen(buf, 8000);
  }
  int partSize = 0;
  for (int i = 0; i < size; i++) {
    if (buf[i] == '\'' || buf[i] == '"' || buf[i] == '<' || buf[i] == '>'
        || buf[i] == '&') {
      if (partSize > 0) {
        send(buf + i - partSize, partSize);
        partSize = 0;
      }
      switch (buf[i]) {
        case '\'':
          send("&apos;");
          break;
        case '"':
          send("&quot;");
          break;
        case '<':
          send("&lt;");
          break;
        case '>':
          send("&gt;");
          break;
        case '&':
          send("&amp;");
          break;
      }
    } else {
      partSize++;
    }
  }
  if (partSize > 0) {
    send(buf + size - partSize, partSize);
  }
}

void WebSender::sendSelectItem(int value, const char *label, bool selected) {
  char buf[100];
  snprintf(buf, sizeof(buf), "<option value=\"%d\" %s>%s</option>", value,
      selected ? "selected" : "", label);
  send(buf);
}

void WebSender::sendHidden(bool hidden) {
  if (hidden) {
    send(" hidden");
  }
}

void WebSender::sendReadonly(bool readonly) {
  if (readonly) {
    send(" readonly");
  }
}

void WebSender::sendDisabled(bool disabled) {
  if (disabled) {
    send(" disabled");
  }
}

};  // namespace Supla

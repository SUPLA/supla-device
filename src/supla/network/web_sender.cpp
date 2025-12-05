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

#ifndef ARDUINO_ARCH_AVR

#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <inttypes.h>
#include <supla/log_wrapper.h>

#include "web_sender.h"

namespace Supla {

WebSender::~WebSender() {}

void WebSender::send(int number) {
  char buf[100];
  int size = snprintf(buf, sizeof(buf), "%d", number);
  if (size < 0) {
    SUPLA_LOG_WARNING("WebSender error - snprintf failed");
    return;
  }
  if (static_cast<size_t>(size) > sizeof(buf)) {
    SUPLA_LOG_WARNING("WebSender error - buffer too small");
    return;
  }
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

  int size = snprintf(buf, sizeof(buf),
      "%.*f", printPrecission, static_cast<float>(number) / divider);
  if (size < 0) {
    SUPLA_LOG_WARNING("WebSender error - snprintf failed");
    return;
  }
  if (static_cast<size_t>(size) > sizeof(buf)) {
    SUPLA_LOG_WARNING("WebSender error - buffer too small");
    return;
  }
  send(buf, size);
}

void WebSender::sendNameAndId(const char *id) {
  char buf[100];
  int size = snprintf(buf, sizeof(buf), " name=\"%s\" id=\"%s\" ",
      id ? id : "", id ? id : "");
  if (size < 0) {
    SUPLA_LOG_WARNING("WebSender error - snprintf failed");
    return;
  }
  if (static_cast<size_t>(size) > sizeof(buf)) {
    SUPLA_LOG_WARNING("WebSender error - buffer too small");
    return;
  }
  send(buf);
}

void WebSender::sendLabelFor(const char *id, const char *label) {
  char buf[300];
  int size = snprintf(buf,
                      sizeof(buf),
                      "<label for=\"%s\">%s</label>",
                      id ? id : "",
                      label ? label : "");
  if (size < 0) {
    SUPLA_LOG_WARNING("WebSender error - snprintf failed");
    return;
  }
  if (static_cast<size_t>(size) > sizeof(buf)) {
    SUPLA_LOG_WARNING("WebSender error - buffer too small");
    return;
  }
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

void WebSender::sendSelectItem(int value,
                               const char *label,
                               bool selected,
                               bool emptyValue) {
  char buf[100];
  int size = 0;
  if (emptyValue) {
    size = snprintf(buf,
                    sizeof(buf),
                    "<option value=\"\" %s>%s</option>",
                    selected ? "selected" : "",
                    label);
  } else {
    size = snprintf(buf,
                    sizeof(buf),
                    "<option value=\"%d\" %s>%s</option>",
                    value,
                    selected ? "selected" : "",
                    label);
  }
  if (size < 0) {
    SUPLA_LOG_WARNING("WebSender error - snprintf failed");
    return;
  }
  if (static_cast<size_t>(size) > sizeof(buf)) {
    SUPLA_LOG_WARNING("WebSender error - buffer too small");
    return;
  }
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

void WebSender::sendTimestamp(uint32_t timestamp) {
  // timestamp may contain unix timestamp, or just seconds since board boot
  char buf[100] = {};
  int size = 0;
  if (timestamp < 1600000000) {
    // somewhere in 2020, so assume it is seconds since board boot
    size = snprintf(buf, sizeof(buf), "%" PRIu32 " s (since boot)", timestamp);
  } else {
    struct tm timeinfo;
    time_t time = timestamp;
    localtime_r(&time, &timeinfo);
    size = strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
  }
  if (size < 0) {
    SUPLA_LOG_WARNING("WebSender error - snprintf failed");
    return;
  }
  if (static_cast<size_t>(size) > sizeof(buf)) {
    SUPLA_LOG_WARNING("WebSender error - buffer too small");
    return;
  }
  send(buf);
}

};  // namespace Supla

#endif

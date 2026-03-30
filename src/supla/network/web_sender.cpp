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

HtmlTag::HtmlTag(WebSender* sender, const char* tagName, bool paired)
    : sender_(sender), tagName_(tagName), paired_(paired) {
  if (sender_ && tagName_) {
    sender_->send("<");
    sender_->send(tagName_);
  }
}

HtmlTag::~HtmlTag() {
  release();
}

HtmlTag::HtmlTag(HtmlTag&& other) noexcept {
  *this = static_cast<HtmlTag&&>(other);
}

HtmlTag& HtmlTag::operator=(HtmlTag&& other) noexcept {
  if (this != &other) {
    release();
    sender_ = other.sender_;
    tagName_ = other.tagName_;
    paired_ = other.paired_;
    closed_ = other.closed_;
    finished_ = other.finished_;
    other.sender_ = nullptr;
    other.tagName_ = nullptr;
    other.closed_ = true;
    other.finished_ = true;
  }
  return *this;
}

HtmlTag& HtmlTag::attr(const char* name, const char* value) {
  if (!finished_ && sender_ && name) {
    sender_->send(" ");
    sender_->send(name);
    sender_->send("=\"");
    sender_->sendSafe(value ? value : "");
    sender_->send("\"");
  }
  return *this;
}

HtmlTag& HtmlTag::attr(const char* name, int value) {
  if (!finished_ && sender_ && name) {
    sender_->send(" ");
    sender_->send(name);
    sender_->send("=\"");
    sender_->send(value);
    sender_->send("\"");
  }
  return *this;
}

HtmlTag& HtmlTag::attrIf(const char* name, bool enabled) {
  if (enabled && sender_ && name && !finished_) {
    sender_->send(" ");
    sender_->send(name);
  }
  return *this;
}

HtmlTag& HtmlTag::close() {
  if (!closed_ && sender_) {
    sender_->send(">");
    closed_ = true;
  }
  return *this;
}

HtmlTag& HtmlTag::finish() {
  close();
  finished_ = true;
  return *this;
}

void HtmlTag::body(const char* text) {
  close();
  if (sender_) {
    sender_->sendSafe(text ? text : "");
  }
  end();
}

void HtmlTag::end() {
  if (finished_ || !sender_) {
    return;
  }
  close();
  if (paired_ && tagName_) {
    sender_->send("</");
    sender_->send(tagName_);
    sender_->send(">");
  }
  finished_ = true;
}

void HtmlTag::release() {
  if (!finished_ && sender_) {
    end();
  }
}

WebSender::~WebSender() {}

void WebSender::labelFor(const char* id, const char* text) {
  auto label = tag("label");
  label.attr("for", id ? id : "");
  label.body(text ? text : "");
}

void WebSender::textInput(const char* name,
                          const char* id,
                          const char* value,
                          int maxLength) {
  auto input = voidTag("input");
  input.attr("type", "text");
  if (maxLength >= 0) {
    input.attr("maxlength", maxLength);
  }
  if (name) {
    input.attr("name", name);
  }
  if (id) {
    input.attr("id", id);
  }
  if (value) {
    input.attr("value", value);
  }
  input.finish();
}

void WebSender::passwordInput(const char* name, const char* id) {
  auto input = voidTag("input");
  input.attr("type", "password");
  if (name) {
    input.attr("name", name);
  }
  if (id) {
    input.attr("id", id);
  }
  input.finish();
}

void WebSender::checkboxInput(const char* name,
                              const char* id,
                              bool checked,
                              const char* value) {
  auto input = voidTag("input");
  input.attr("type", "checkbox");
  input.attr("value", value ? value : "");
  input.attrIf("checked", checked);
  if (name) {
    input.attr("name", name);
  }
  if (id) {
    input.attr("id", id);
  }
  input.finish();
}

void WebSender::selectOption(const char* value,
                             int text,
                             bool selected) {
  char textBuf[32];
  int size = snprintf(textBuf, sizeof(textBuf), "%d", text);
  if (size < 0 || static_cast<size_t>(size) >= sizeof(textBuf)) {
    SUPLA_LOG_WARNING("WebSender error - snprintf failed");
    return;
  }
  selectOption(value, textBuf, selected);
}

void WebSender::selectOption(int value,
                             const char* text,
                             bool selected) {
  char valueBuf[32];
  int size = snprintf(valueBuf, sizeof(valueBuf), "%d", value);
  if (size < 0 || static_cast<size_t>(size) >= sizeof(valueBuf)) {
    SUPLA_LOG_WARNING("WebSender error - snprintf failed");
    return;
  }
  selectOption(valueBuf, text, selected);
}

void WebSender::selectOption(int value,
                             int text,
                             bool selected) {
  char valueBuf[32];
  char textBuf[32];
  int size = snprintf(valueBuf, sizeof(valueBuf), "%d", value);
  if (size < 0 || static_cast<size_t>(size) >= sizeof(valueBuf)) {
    SUPLA_LOG_WARNING("WebSender error - snprintf failed");
    return;
  }
  size = snprintf(textBuf, sizeof(textBuf), "%d", text);
  if (size < 0 || static_cast<size_t>(size) >= sizeof(textBuf)) {
    SUPLA_LOG_WARNING("WebSender error - snprintf failed");
    return;
  }
  selectOption(valueBuf, textBuf, selected);
}

void WebSender::selectOption(const char* value,
                             const char* text,
                             bool selected) {
  auto option = tag("option");
  option.attr("value", value ? value : "");
  option.attrIf("selected", selected);
  option.body(text ? text : "");
}

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

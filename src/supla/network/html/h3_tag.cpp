// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef ARDUINO_ARCH_AVR
#include "h3_tag.h"

#include <supla/network/web_sender.h>

#include <string.h>
#include <stdio.h>

using Supla::Html::H3Tag;

H3Tag::H3Tag(const char *text) {
  int size = strlen(text);
  this->text = new char[size + 1];
  if (this->text) {
    snprintf(this->text, size + 1, "%s", text);
  }
}

H3Tag::~H3Tag() {
  if (text) {
    delete[] text;
    text = nullptr;
  }
}

void H3Tag::send(Supla::WebSender* sender) {
  // form-field BEGIN
  sender->tag("h3").body(text);
  // form-field END
}

#endif  // ARDUINO_ARCH_AVR

/*
   Copyright (C) AC SOFTWARE SP. Z O.O

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

#include "h2_tag.h"

#include <supla/network/web_sender.h>

#include <string.h>
#include <stdio.h>

using Supla::Html::H2Tag;

H2Tag::H2Tag(const char *text) {
  int size = strlen(text);
  this->text = new char[size + 1];
  if (this->text) {
    snprintf(this->text, size + 1, "%s", text);
  }
}

H2Tag::~H2Tag() {
  if (text) {
    delete[] text;
    text = nullptr;
  }
}

void H2Tag::send(Supla::WebSender* sender) {
  // form-field BEGIN
  sender->send("<h2>");
  sender->send(text);
  sender->send("</h2>");
  // form-field END
}

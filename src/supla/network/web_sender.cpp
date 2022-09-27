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
  for (int i = 0; i < precision; i++) {
    divider *= 10;
  }

  snprintf(buf, sizeof(buf),
      "%.*f", precision, static_cast<float>(number) / divider);
  send(buf);
}

};  // namespace Supla

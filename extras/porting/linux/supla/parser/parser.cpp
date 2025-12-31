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

#include "parser.h"
#include <supla/time.h>
#include <supla-common/log.h>

#include <string>

Supla::Parser::Parser::Parser(Supla::Source::Source *src) : source(src) {}

void Supla::Parser::Parser::addKey(const std::string& key, int index) {
  keys[key] = index;
}

bool Supla::Parser::Parser::isValid() {
  return valid;
}

bool Supla::Parser::Parser::refreshParserSource() {
  if (!lastRefreshTime || millis() - lastRefreshTime > refreshTimeMs) {
    lastRefreshTime = millis();
    return refreshSource();
  }
  return true;
}

void Supla::Parser::Parser::setRefreshTime(unsigned int timeMs) {
  if (timeMs < 10) {
    timeMs = 10;
  }
  refreshTimeMs = timeMs;
}

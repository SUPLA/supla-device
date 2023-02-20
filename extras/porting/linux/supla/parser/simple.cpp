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

#include "simple.h"
#include <sstream>

Supla::Parser::Simple::Simple(Supla::Source::Source *src)
  : Supla::Parser::Parser(src) {
}

Supla::Parser::Simple::~Simple() {}

double Supla::Parser::Simple::getValue(const std::string &key) {
  int index = keys[key];
  if (index < 0 || index >= values.size()) {
    valid = false;
    return 0;
  }

  return values[index];
}


bool Supla::Parser::Simple::refreshSource() {
  if (source) {
    std::string sourceContent = source->getContent();

    if (sourceContent.length() == 0) {
      valid = false;
      return valid;
    }

    std::stringstream ss(sourceContent);
    std::string line;
    values.clear();

    for (int i = 0; std::getline(ss, line, '\n'); i++) {
      std::stringstream sline(line);
      sline >> values[i];
    }

    valid = true;
    return valid;
  }
  valid = false;
  return valid;
}

bool Supla::Parser::Simple::isBasedOnIndex() {
  return true;
}

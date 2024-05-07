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

Supla::Parser::Simple::~Simple() {
}

double Supla::Parser::Simple::getValue(const std::string &key) {
  int index = keys[key];
  if (index < 0 || index >= values.size()) {
    valid = false;
    return 0;
  }
  std::variant<int, bool, std::string, double> val = values[index];
  if (std::holds_alternative<double>(val)) {
    return std::get<double>(val);
  } else if (std::holds_alternative<int>(val)) {
    return static_cast<double>(std::get<int>(val));
  } else {
    valid = false;
    return 0;
  }
}

std::variant<int, bool, std::string> Supla::Parser::Simple::getStateValue(
    const std::string &key) {
  int index = keys[key];
  if (index < 0 || index >= values.size()) {
    valid = false;
    return 0;
  }
  std::variant<int, bool, std::string, double> val = values[index];
  if (std::holds_alternative<int>(val)) {
    return std::get<int>(val);
  } else if (std::holds_alternative<bool>(val)) {
    return std::get<bool>(val);
  } else if (std::holds_alternative<std::string>(val)) {
    return std::get<std::string>(val);
  } else if (std::holds_alternative<double>(val)) {
    return static_cast<int>(std::get<double>(val));
  } else {
    valid = false;
    return 0;
  }
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

    std::string strVal;
    for (int i = 0; std::getline(ss, strVal, '\n'); i++) {
      try {
        if (strVal == "true" || strVal == "false") {
          values[i] = strVal == "true";
        } else {
          double dblVal = std::stod(strVal);
          if (dblVal == static_cast<int>(dblVal)) {
            values[i] = static_cast<int>(dblVal);
          } else {
            values[i] = dblVal;
          }
        }
      } catch (...) {
        values[i] = strVal;
      }
    }
  }
  valid = true;
  return valid;
}

bool Supla::Parser::Simple::isBasedOnIndex() {
  return true;
}

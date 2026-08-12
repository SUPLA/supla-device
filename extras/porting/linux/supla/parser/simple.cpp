// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "simple.h"

#include <sstream>
#include <string>

Supla::Parser::Simple::Simple(Supla::Source::Source *src)
    : Supla::Parser::Parser(src) {
}

Supla::Parser::Simple::~Simple() {
}

double Supla::Parser::Simple::getValue(const std::string &key) {
  int index = keys[key];
  if (index < 0 || static_cast<size_t>(index) >= values.size()) {
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
  if (index < 0 || static_cast<size_t>(index) >= values.size()) {
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

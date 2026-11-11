// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "simple.h"

#include <sstream>
#include <string>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <limits>

Supla::Parser::Simple::Simple(Supla::Source::Source *src)
    : Supla::Parser::Parser(src) {
}

Supla::Parser::Simple::~Simple() {
}

double Supla::Parser::Simple::getValue(const std::string &key) {
  const auto mapping = keys.find(key);
  if (!valid || mapping == keys.end() || mapping->second < 0 ||
      static_cast<size_t>(mapping->second) >= values.size()) {
    valid = false;
    return 0;
  }
  const auto &val = values[mapping->second];
  if (std::holds_alternative<double>(val) &&
      std::isfinite(std::get<double>(val))) {
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
  const auto mapping = keys.find(key);
  if (!valid || mapping == keys.end() || mapping->second < 0 ||
      static_cast<size_t>(mapping->second) >= values.size()) {
    valid = false;
    return 0;
  }
  const auto &val = values[mapping->second];
  if (std::holds_alternative<int>(val)) {
    return std::get<int>(val);
  } else if (std::holds_alternative<bool>(val)) {
    return std::get<bool>(val);
  } else if (std::holds_alternative<std::string>(val)) {
    return std::get<std::string>(val);
  } else if (std::holds_alternative<double>(val)) {
    const double number = std::get<double>(val);
    if (std::isfinite(number) && number >= std::numeric_limits<int>::min() &&
        number <= std::numeric_limits<int>::max()) {
      return static_cast<int>(number);
    }
  }
  valid = false;
  return 0;
}

bool Supla::Parser::Simple::refreshSource() {
  valid = false;
  values.clear();
  if (source == nullptr) {
    return false;
  }
  const std::string sourceContent = source->getContent();
  if (sourceContent.empty()) {
    return false;
  }

  // Count getline-compatible fields before allocating or converting any.
  size_t count = 0;
  for (size_t start = 0; start < sourceContent.size();) {
    if (++count > MAX_FIELDS) {
      return false;
    }
    const size_t end = sourceContent.find('\n', start);
    if (end == std::string::npos) {
      break;
    }
    start = end + 1;
  }
  values.reserve(count);
  std::stringstream ss(sourceContent);
  std::string strVal;
  while (std::getline(ss, strVal, '\n')) {
    if (strVal == "true" || strVal == "false") {
      values.emplace_back(strVal == "true");
      continue;
    }
    char *end = nullptr;
    errno = 0;
    const double number = std::strtod(strVal.c_str(), &end);
    if (end == strVal.c_str() || errno == ERANGE) {
      values.emplace_back(strVal);
    } else if (std::isfinite(number) &&
               number >= std::numeric_limits<int>::min() &&
               number <= std::numeric_limits<int>::max() &&
               number == std::trunc(number)) {
      values.emplace_back(static_cast<int>(number));
    } else {
      values.emplace_back(number);
    }
  }
  valid = true;
  return valid;
}

bool Supla::Parser::Simple::isBasedOnIndex() {
  return true;
}

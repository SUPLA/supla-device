// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "parser.h"
#include <supla/time.h>
#include <supla-common/log.h>

#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <string>

Supla::Parser::Parser::Parser(Supla::Source::Source *src) : source(src) {}

void Supla::Parser::Parser::addKey(const std::string& key, int index) {
  keys[key] = index;
}

bool Supla::Parser::Parser::isValid() {
  return valid;
}

bool Supla::Parser::Parser::isSourceValid() {
  return isValid();
}

std::optional<double> Supla::Parser::Parser::readMeasurement(
    const std::string &key) {
  const auto raw = getNumberOrText(key);
  if (!raw) {
    return std::nullopt;
  }
  if (const auto *number = std::get_if<double>(&*raw)) {
    return std::isfinite(*number) ? std::optional<double>(*number)
                                 : std::nullopt;
  }
  // JSON numeric strings are supported by the existing measurement API.
  if (const auto *str = std::get_if<std::string>(&*raw);
      str && !isBasedOnIndex()) {
    char *end = nullptr;
    errno = 0;
    const double number = std::strtod(str->c_str(), &end);
    while (end != str->c_str() &&
           std::isspace(static_cast<unsigned char>(*end))) {
      ++end;
    }
    if (errno != ERANGE && end != str->c_str() &&
        end == str->c_str() + str->size() && std::isfinite(number)) {
      return number;
    }
  }
  return std::nullopt;
}

bool Supla::Parser::Parser::refreshParserSource() {
  if (!lastRefreshTime || millis() - lastRefreshTime > refreshTimeMs) {
    lastRefreshTime = millis();
    ++refreshRevision;
    const bool refreshed = refreshSource();
    usedFallback = source && source->lastReadUsedFallback();
    return refreshed;
  }
  return true;
}

bool Supla::Parser::Parser::isSourceConnected() const {
  return source ? source->isConnected() : false;
}

void Supla::Parser::Parser::setRefreshTime(unsigned int timeMs) {
  if (timeMs < 10) {
    timeMs = 10;
  }
  refreshTimeMs = timeMs;
}

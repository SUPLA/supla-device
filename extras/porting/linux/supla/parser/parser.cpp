// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

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

bool Supla::Parser::Parser::isSourceValid() {
  return isValid();
}

bool Supla::Parser::Parser::refreshParserSource() {
  if (!lastRefreshTime || millis() - lastRefreshTime > refreshTimeMs) {
    lastRefreshTime = millis();
    return refreshSource();
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

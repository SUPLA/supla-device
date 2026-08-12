// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <supla/log_wrapper.h>

#include "wind_parsed.h"

Supla::Sensor::WindParsed::WindParsed(
    Supla::Parser::Parser *parser)
    : SensorParsed(parser) {
}

void Supla::Sensor::WindParsed::onInit() {
  channel.setNewValue(getValue());
}

double Supla::Sensor::WindParsed::getValue() {
  double value = WIND_NOT_AVAILABLE;

  if (isParameterConfigured(Supla::Parser::Wind)) {
    if (refreshParserSource()) {
      value = getParameterValue(Supla::Parser::Wind);
    }
    if (!parser->isValid()) {
      if (!isDataErrorLogged) {
        isDataErrorLogged = true;
        SUPLA_LOG_WARNING("WindParsed: source data error");
      }
      return WIND_NOT_AVAILABLE;
    }
    isDataErrorLogged = false;
  }
  return value;
}


// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <supla/log_wrapper.h>

#include "rain_parsed.h"

Supla::Sensor::RainParsed::RainParsed(
    Supla::Parser::Parser *parser)
    : SensorParsed(parser) {
}

void Supla::Sensor::RainParsed::onInit() {
  channel.setNewValue(getValue());
}

double Supla::Sensor::RainParsed::getValue() {
  double value = RAIN_NOT_AVAILABLE;

  if (isParameterConfigured(Supla::Parser::Rain)) {
    if (refreshParserSource()) {
      value = getParameterValue(Supla::Parser::Rain);
    }
    if (!parser->isValid()) {
      if (!isDataErrorLogged) {
        isDataErrorLogged = true;
        SUPLA_LOG_WARNING("RainParsed: source data error");
      }
      return RAIN_NOT_AVAILABLE;
    }
    isDataErrorLogged = false;
  }
  return value;
}



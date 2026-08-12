// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <supla/log_wrapper.h>

#include "thermometer_parsed.h"

Supla::Sensor::ThermometerParsed::ThermometerParsed(
    Supla::Parser::Parser *parser)
    : SensorParsed(parser) {
}

void Supla::Sensor::ThermometerParsed::onInit() {
  channel.setNewValue(getValue());
}

double Supla::Sensor::ThermometerParsed::getValue() {
  double value = TEMPERATURE_NOT_AVAILABLE;

  if (isParameterConfigured(Supla::Parser::Temperature)) {
    if (refreshParserSource()) {
      value = getParameterValue(Supla::Parser::Temperature);
    }
    if (!parser->isValid()) {
      if (!isDataErrorLogged) {
        isDataErrorLogged = true;
        SUPLA_LOG_WARNING("ThermometerParsed: source data error");
      }
      return TEMPERATURE_NOT_AVAILABLE;
    }
    isDataErrorLogged = false;
  }
  return value;
}

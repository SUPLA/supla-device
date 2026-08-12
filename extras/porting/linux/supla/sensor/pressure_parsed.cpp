// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <supla/log_wrapper.h>

#include "pressure_parsed.h"

Supla::Sensor::PressureParsed::PressureParsed(
    Supla::Parser::Parser *parser)
    : SensorParsed(parser) {
}

void Supla::Sensor::PressureParsed::onInit() {
  channel.setNewValue(getValue());
}

double Supla::Sensor::PressureParsed::getValue() {
  double value = PRESSURE_NOT_AVAILABLE;

  if (isParameterConfigured(Supla::Parser::Pressure)) {
    if (refreshParserSource()) {
      value = getParameterValue(Supla::Parser::Pressure);
    }
    if (!parser->isValid()) {
      if (!isDataErrorLogged) {
        isDataErrorLogged = true;
        SUPLA_LOG_WARNING("PressureParsed: source data error");
      }
      return PRESSURE_NOT_AVAILABLE;
    }
    isDataErrorLogged = false;
  }
  return value;
}


// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <supla/log_wrapper.h>

#include "humidity_parsed.h"
#include "therm_hygro_meter_parsed.h"

Supla::Sensor::ThermHygroMeterParsed::ThermHygroMeterParsed(
    Supla::Parser::Parser *parser)
    : SensorParsed(parser) {
}

double Supla::Sensor::ThermHygroMeterParsed::getTemp() {
  double value = TEMPERATURE_NOT_AVAILABLE;

  if (isParameterConfigured(Supla::Parser::Temperature)) {
    if (refreshParserSource()) {
      value = getParameterValue(Supla::Parser::Temperature);
    }
    if (!parser->isValid()) {
      if (!isDataErrorLogged) {
        isDataErrorLogged = true;
        SUPLA_LOG_WARNING("ThermHygroMeterParsed: source data error");
      }
      return TEMPERATURE_NOT_AVAILABLE;
    }
    isDataErrorLogged = false;
  }
  return value;
}

double Supla::Sensor::ThermHygroMeterParsed::getHumi() {
  double value = HUMIDITY_NOT_AVAILABLE;

  if (isParameterConfigured(Supla::Parser::Humidity)) {
    if (refreshParserSource()) {
      value = getParameterValue(Supla::Parser::Humidity);
    }
    if (!parser->isValid()) {
      if (!isDataErrorLogged) {
        isDataErrorLogged = true;
        SUPLA_LOG_WARNING("ThermHygroMeterParsed: source data error");
      }
      return HUMIDITY_NOT_AVAILABLE;
    }
    isDataErrorLogged = false;
  }
  return value;
}


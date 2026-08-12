// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <supla/log_wrapper.h>

#include "humidity_parsed.h"

Supla::Sensor::HumidityParsed::HumidityParsed(
    Supla::Parser::Parser *parser)
    : SensorParsed(parser) {
}

void Supla::Sensor::HumidityParsed::onInit() {
  channel.setNewValue(-273.0, getHumi());
}

double Supla::Sensor::HumidityParsed::getHumi() {
  double value = HUMIDITY_NOT_AVAILABLE;

  if (isParameterConfigured(Supla::Parser::Humidity)) {
    if (refreshParserSource()) {
      value = getParameterValue(Supla::Parser::Humidity);
    }
    if (!parser->isValid()) {
      if (!isDataErrorLogged) {
        isDataErrorLogged = true;
        SUPLA_LOG_WARNING("HumidityParsed: source data error");
      }
      return HUMIDITY_NOT_AVAILABLE;
    }
    isDataErrorLogged = false;
  }
  return value;
}


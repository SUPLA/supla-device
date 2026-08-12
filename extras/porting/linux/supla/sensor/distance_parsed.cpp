// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <supla/log_wrapper.h>

#include "distance_parsed.h"

Supla::Sensor::DistanceParsed::DistanceParsed(
    Supla::Parser::Parser *parser)
    : SensorParsed(parser) {
}

void Supla::Sensor::DistanceParsed::onInit() {
  channel.setNewValue(getValue());
}

double Supla::Sensor::DistanceParsed::getValue() {
  double value = DISTANCE_NOT_AVAILABLE;

  if (isParameterConfigured(Supla::Parser::Distance)) {
    if (refreshParserSource()) {
      value = getParameterValue(Supla::Parser::Distance);
    }
    if (!parser->isValid()) {
      if (!isDataErrorLogged) {
        isDataErrorLogged = true;
        SUPLA_LOG_WARNING("DistanceParsed: source data error");
      }
      return DISTANCE_NOT_AVAILABLE;
    }
    isDataErrorLogged = false;
  }
  return value;
}


// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <supla/log_wrapper.h>

#include "weight_parsed.h"

Supla::Sensor::WeightParsed::WeightParsed(
    Supla::Parser::Parser *parser)
    : SensorParsed(parser) {
}

void Supla::Sensor::WeightParsed::onInit() {
  channel.setNewValue(getValue());
}

double Supla::Sensor::WeightParsed::getValue() {
  double value = WEIGHT_NOT_AVAILABLE;

  if (isParameterConfigured(Supla::Parser::Weight)) {
    if (refreshParserSource()) {
      value = getParameterValue(Supla::Parser::Weight);
    }
    if (!parser->isValid()) {
      if (!isDataErrorLogged) {
        isDataErrorLogged = true;
        SUPLA_LOG_WARNING("WeightParsed: source data error");
      }
      return WEIGHT_NOT_AVAILABLE;
    }
    isDataErrorLogged = false;
  }
  return value;
}

void Supla::Sensor::WeightParsed::tareScales() {
}

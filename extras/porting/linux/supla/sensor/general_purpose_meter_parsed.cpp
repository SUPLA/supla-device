// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "general_purpose_meter_parsed.h"

#include <supla/log_wrapper.h>
#include <cmath>

using Supla::Sensor::GeneralPurposeMeterParsed;

GeneralPurposeMeterParsed::GeneralPurposeMeterParsed(
    Supla::Parser::Parser *parser)
    : SensorParsed(parser) {
}

double GeneralPurposeMeterParsed::getValue() {
  double value = NAN;

  if (isParameterConfigured(Supla::Parser::Value)) {
    if (refreshParserSource()) {
      value = getParameterValue(Supla::Parser::Value);
    }
    if (!parser->isValid()) {
      if (!isDataErrorLogged) {
        isDataErrorLogged = true;
        SUPLA_LOG_WARNING("GPM: source data error");
      }
      return NAN;
    }
    isDataErrorLogged = false;
  }
  return value;
}





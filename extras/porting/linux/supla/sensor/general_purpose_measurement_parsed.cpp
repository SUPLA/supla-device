// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "general_purpose_measurement_parsed.h"

#include <supla/log_wrapper.h>
#include <cmath>

#include "binary_parsed.h"

using Supla::Sensor::GeneralPurposeMeasurementParsed;

GeneralPurposeMeasurementParsed::GeneralPurposeMeasurementParsed(
    Supla::Parser::Parser *parser)
    : SensorParsed(parser) {
}

double GeneralPurposeMeasurementParsed::getValue() {
  double value = NAN;

  if (isParameterConfigured(Supla::Parser::State)) {
    int state = getStateValue(false);
    if (state != 1) {
      if (state == 0) {
        setChannelStateOnline(true);
      }
      return NAN;
    }
  }

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

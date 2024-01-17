/*
   Copyright (C) AC SOFTWARE SP. Z O.O

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
   */

#include "general_purpose_measurement_parsed.h"

#include <supla/log_wrapper.h>
#include <cmath>

using Supla::Sensor::GeneralPurposeMeasurementParsed;

GeneralPurposeMeasurementParsed::GeneralPurposeMeasurementParsed(
    Supla::Parser::Parser *parser)
    : SensorParsed(parser) {
}

double GeneralPurposeMeasurementParsed::getValue() {
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



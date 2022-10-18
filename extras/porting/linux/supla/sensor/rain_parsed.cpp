/*
 * Copyright (C) AC SOFTWARE SP. Z O.O
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

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



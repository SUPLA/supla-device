/*
 Copyright (C) AC SOFTWARE SP. Z O.O.

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

#include <supla/log_wrapper.h>
#include <supla/time.h>

#include "impulse_counter_parsed.h"

Supla::Sensor::ImpulseCounterParsed::ImpulseCounterParsed(
    Supla::Parser::Parser *parser)
    : SensorParsed(parser) {
  channel.setType(SUPLA_CHANNELTYPE_IMPULSE_COUNTER);
}

void Supla::Sensor::ImpulseCounterParsed::iterateAlways() {
  if (millis() - lastReadTime > 10000) {
    lastReadTime = millis();
    channel.setNewValue(getValue());
  }
}

void Supla::Sensor::ImpulseCounterParsed::onInit() {
  channel.setNewValue(getValue());
}

uint64_t Supla::Sensor::ImpulseCounterParsed::getValue() {
  double value = 0;

  if (isParameterConfigured(Supla::Parser::Counter)) {
    if (refreshParserSource()) {
      value = getParameterValue(Supla::Parser::Counter);
    }
    if (!parser->isValid()) {
      if (!isDataErrorLogged) {
        isDataErrorLogged = true;
        SUPLA_LOG_WARNING(
                  "ImpulseCounterParsed: data source is not valid");
      }
      return 0;
    } else {
      isDataErrorLogged = false;
    }
  } else {
    SUPLA_LOG_WARNING(
              "ImpulseCounterParsed: \"counter\" parameter is not configured");
  }
  return static_cast<uint64_t>(value);
}

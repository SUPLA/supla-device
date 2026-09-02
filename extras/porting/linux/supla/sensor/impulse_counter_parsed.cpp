// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <supla/log_wrapper.h>
#include <supla/time.h>

#include "impulse_counter_parsed.h"

Supla::Sensor::ImpulseCounterParsed::ImpulseCounterParsed(
    Supla::Parser::Parser *parser)
    : SensorParsed(parser) {
}

void Supla::Sensor::ImpulseCounterParsed::iterateAlways() {
  if (millis() - lastReadTime > 10000) {
    lastReadTime = millis();
    setCounter(getValue());
  }
}

void Supla::Sensor::ImpulseCounterParsed::onInit() {
  VirtualImpulseCounter::onInit();
  setCounter(getValue());
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

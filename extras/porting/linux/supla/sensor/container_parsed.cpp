// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "container_parsed.h"

#include <supla/log_wrapper.h>
#include "supla/sensor/container.h"

using Supla::Sensor::ContainerParsed;

ContainerParsed::ContainerParsed(Supla::Parser::Parser *parser)
    : SensorParsed(parser) {
}


void ContainerParsed::onInit() {
  channel.setNewValue(readNewValue());
}

int ContainerParsed::readNewValue() {
  int value = 0;

  if (isParameterConfigured(Supla::Parser::Level)) {
    if (refreshParserSource()) {
      value = getParameterValue(Supla::Parser::Level);
    }
    if (!parser->isValid()) {
      if (!isDataErrorLogged) {
        isDataErrorLogged = true;
        SUPLA_LOG_WARNING("ContainerParsed[%d]: source data error",
                          getChannelNumber());
      }
      return 0;
    }
    isDataErrorLogged = false;
  }
  return value;
}


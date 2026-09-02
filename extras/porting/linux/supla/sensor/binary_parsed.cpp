// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <supla/log_wrapper.h>
#include <supla/time.h>

#include "binary_parsed.h"

Supla::Sensor::BinaryParsed::BinaryParsed(Supla::Parser::Parser *parser)
    : SensorParsed(parser) {
}

void Supla::Sensor::BinaryParsed::onInit() {
  VirtualBinary::onInit();
  registerActions();
  handleGetChannelState(nullptr);
}

bool Supla::Sensor::BinaryParsed::getValue() {
  int result = getStateValue(false);
  if (result < 0) {
    return false;
  }

  if (lastState != result) {
    clearedByTimeout = false;
    if (result == 1) {
      set();
    } else if (result == 0) {
      clear();
    }
  } else if (clearedByTimeout) {
    return false;
  }

  setLastState(result);
  return result == 1;
}

void Supla::Sensor::BinaryParsed::iterateAlways() {
  if (parser && (millis() - lastOfflineReadTime > 100)) {
    if (setOfflineIfSourceDisconnected()) {
      lastOfflineReadTime = millis();
      return;
    }
    refreshParserSource(false);
    lastOfflineReadTime = millis();
    setChannelStateOnline(!isOffline());
  }

  VirtualBinary::iterateAlways();
}

// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "seconds_counter.h"

#include <supla/time.h>
#include <supla/actions.h>

using Supla::Sensor::SecondsCounter;

SecondsCounter::SecondsCounter() {
}

void SecondsCounter::handleAction(int event, int action) {
  Supla::Sensor::VirtualImpulseCounter::handleAction(event, action);
  switch (action) {
    case ENABLE: {
      enable();
      break;
    }
    case DISABLE: {
      disable();
      break;
    }
  }
}

void SecondsCounter::iterateAlways() {
  if (isEnabled) {
    uint32_t now = millis();
    if (now - lastMillis + remainingMillis > 1000) {
      int seconds = (now - lastMillis + remainingMillis) / 1000;
      remainingMillis = (now - lastMillis + remainingMillis) % 1000;
      lastMillis = now;
      while (seconds--) {
        incCounter();
      }
    }
  }
}

void SecondsCounter::enable() {
  isEnabled = true;
  lastMillis = millis();
}

void SecondsCounter::disable() {
  isEnabled = false;
  remainingMillis += millis() - lastMillis;
}


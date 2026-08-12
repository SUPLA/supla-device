// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "uptime.h"
#include <supla/log_wrapper.h>

using Supla::Uptime;

Uptime::Uptime() {
}

void Uptime::iterate(uint32_t millis) {
  uint32_t timediff = millis - lastMillis;

  // 1 hour - failsafe in case of millis going back in time
  if (timediff > 3600000) {
     SUPLA_LOG_WARNING("Uptime calculation problem! %d %d", millis, lastMillis);
     lastMillis = millis;
     return;
  }
  while (timediff >= 1000) {
    deviceUptime++;
    connectionUptime++;
    timediff -= 1000;
    lastMillis += 1000;
  }
}

void Uptime::resetConnectionUptime() {
  connectionUptime = 0;
  acceptConnectionLostCause = true;
}

void Uptime::setConnectionLostCause(uint8_t cause) {
  if (acceptConnectionLostCause) {
    lastConnectionResetCause = cause;
    acceptConnectionLostCause = false;
  }
}

uint32_t Uptime::getUptime() const {
  return deviceUptime;
}

uint32_t Uptime::getConnectionUptime() const {
  return connectionUptime;
}

uint8_t Uptime::getLastResetCause() const {
  return lastConnectionResetCause;
}

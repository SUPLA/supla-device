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

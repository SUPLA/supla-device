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


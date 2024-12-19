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

#include "container.h"

#include <supla-common/proto.h>
#include <supla/time.h>

using Supla::Sensor::Container;

Container::Container() {
  channel.setType(SUPLA_CHANNELTYPE_CONTAINER);
  setDefaultFunction(SUPLA_CHANNELFNC_CONTAINER);
  channel.setContainerValue(0);
}

void Container::iterateAlways() {
  if (millis() - lastReadTime >= readIntervalMs) {
    lastReadTime = millis();
    channel.setContainerValue(getValue());
  }
}

void Container::setValue(int value) {
  if (value < 0 || value > 101) {
    return;
  }

  fillLevel = value;
}

int Container::getValue() {
  return fillLevel;
}

void Container::setReadIntervalMs(uint32_t timeMs) {
  if (timeMs < 100) {
    timeMs = 100;
  }
  readIntervalMs = timeMs;
}


void Container::setAlarmActive(bool alarmActive) {
  channel.setContainerAlarm(alarmActive);
}


bool Container::isAlarmActive() const {
  return channel.isContainerAlarmActive();
}

void Container::setWarningActive(bool warningActive) {
    channel.setContainerWarning(warningActive);
}

bool Container::isWarningActive() const {
    return channel.isContainerWarningActive();
}

void Container::setInvalidSensorStateActive(bool invalidSensorStateActive) {
    channel.setContainerInvalidSensorState(invalidSensorStateActive);
}

bool Container::isInvalidSensorStateActive() const {
    return channel.isContainerInvalidSensorStateActive();
}

void Container::setSoundAlarmOn(bool soundAlarmOn) {
  channel.setContainerSoundAlarmOn(soundAlarmOn);
}

bool Container::isSoundAlarmOn() const {
  return channel.isContainerSoundAlarmOn();
}

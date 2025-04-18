/*
 Copyright (C) malarz
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

#include "therm_press_meter.h"

#include <supla/time.h>

Supla::Sensor::ThermPressMeter::ThermPressMeter() {
  channel.setType(SUPLA_CHANNELTYPE_THERMOMETER);
  channel.setDefaultFunction(SUPLA_CHANNELFNC_THERMOMETER);
}

void Supla::Sensor::ThermPressMeter::iterateAlways() {
  if (millis() - lastReadTime > refreshIntervalMs) {
    lastReadTime = millis();
    channel.setNewValue(getTemp());
    pressureChannel.setNewValue(getPressure());
  }
}

void Supla::Sensor::ThermPressMeter::setHumidityCorrection(int32_t correction) {
  (void)(correction);
}

/*
 * Copyright (C) AC SOFTWARE SP. Z O.O
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <supla/log_wrapper.h>
#include <supla/time.h>

#include "ntc10k.h"

Supla::Sensor::NTC10k::NTC10k() {
}

void Supla::Sensor::NTC10k::onInit() {
}

void Supla::Sensor::NTC10k::readSensor() {
//  double temperature = 0;
//  SUPLA_LOG_DEBUG("NTC10k: temp: %.2f", temperature);
//  lastValidTemp = temperature;
}

double Supla::Sensor::NTC10k::getValue() {
  readSensor();
  return lastValidTemp;
}

void Supla::Sensor::NTC10k::set(double val) {
  lastValidTemp = val;
}

void Supla::Sensor::NTC10k::iterateAlways() {
  if (millis() - lastReadTime > 2000) {
    lastReadTime = millis();
    channel.setNewValue(getValue());
  }
}

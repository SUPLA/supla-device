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

#include "temperature_drop_sensor.h"

#include <supla/time.h>

using Supla::Sensor::TemperatureDropSensor;

TemperatureDropSensor::TemperatureDropSensor(
    Supla::Sensor::ThermHygroMeter *thermometer)
    : thermometer(thermometer) {
  for (int i = 0; i < MAX_TEMPERATURE_MEASUREMENTS; i++) {
    measurements[i] = INT16_MIN;
  }
  measurementIndex = 0;
  virtualBinary.setDefaultFunction(SUPLA_CHANNELFNC_OPENINGSENSOR_WINDOW);
  virtualBinary.set();  // by default "window" is considered to be closed
}

void TemperatureDropSensor::onInit() {
}

void TemperatureDropSensor::iterateAlways() {
  if (millis() - lastTemperatureUpdate >= probeIntervalMs) {
    lastTemperatureUpdate = millis();
    auto temperature = thermometer->getTempInt16();
    // add temperature to table
    if (temperature > INT16_MIN) {
      measurements[measurementIndex] = temperature;
      measurementIndex++;
      if (measurementIndex >= MAX_TEMPERATURE_MEASUREMENTS) {
        measurementIndex = 0;
      }
    }

    if (dropDetectionTimestamp) {
      if (temperature - averageAtDropDetection > temperatureDropThreshold / 2) {
        if (!detectTemperatureDrop(temperature, &averageAtDropDetection)) {
          virtualBinary.set();
          dropDetectionTimestamp = 0;
        }
      }

      // maximum time for drop detection is 30 minutes
      if (millis() - dropDetectionTimestamp > 30 * 60 * 1000) {
        virtualBinary.set();
        dropDetectionTimestamp = 0;
      }
    } else {
      if (detectTemperatureDrop(temperature, &averageAtDropDetection)) {
        virtualBinary.clear();
        dropDetectionTimestamp = millis();
      }
    }
  }
}

bool TemperatureDropSensor::detectTemperatureDrop(int16_t temperature,
                                           int16_t *average) const {
  // calculate 10 min average starting 3 min ago (measurements are stored
  // every probeIntervalMs)
  int oneMinutIndexCount = (60000 / probeIntervalMs);
  int fromIndex = measurementIndex - (10 + 3) * oneMinutIndexCount;
  while (fromIndex < 0) {
    fromIndex += MAX_TEMPERATURE_MEASUREMENTS;
  }
  int averageInPast = getAverage(fromIndex, 10 * oneMinutIndexCount);
  *average = averageInPast;
  if (averageInPast > INT16_MIN) {
    if (temperature - averageInPast < temperatureDropThreshold) {
      return true;
    }
  }
  return false;
}

int16_t TemperatureDropSensor::getAverage(int fromIndex,
                                          int itemsToCount) const {
  if (fromIndex < 0 || fromIndex >= MAX_TEMPERATURE_MEASUREMENTS ||
      itemsToCount <= 0 || itemsToCount > MAX_TEMPERATURE_MEASUREMENTS) {
    return INT16_MIN;
  }

  int sum = 0;
  int count = 0;
  for (int i = 0; i <= itemsToCount; i++) {
    auto temperature =
      measurements[(fromIndex + i) % MAX_TEMPERATURE_MEASUREMENTS];
    if (temperature > INT16_MIN) {
      sum += measurements[(fromIndex + i) % MAX_TEMPERATURE_MEASUREMENTS];
      count++;
    }
  }

  if (count == 0) {
    return INT16_MIN;
  }

  return sum / count;
}

bool TemperatureDropSensor::isDropDetected() const {
  return dropDetectionTimestamp != 0;
}

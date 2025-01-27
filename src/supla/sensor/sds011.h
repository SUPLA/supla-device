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

// Dependencies:
// https://github.com/ricki-z/SDS011

#ifndef SRC_SUPLA_SENSOR_SDS011_H_
#define SRC_SUPLA_SENSOR_SDS011_H_

#include <supla/sensor/general_purpose_measurement.h>

#include <SDS011.h>

namespace Supla {
namespace Sensor {
class sds011 : public Supla::Element {
 public:
  // rx_pin, tx_pin: pins to which the sensor is connected
  // refresh: time between readings (in minutes: 1-1440)
  explicit sds011(int rx_pin, int tx_pin, int refresh = 10) {
    if (refresh < 1) {
      refresh = 10;
    } else if (refresh > 1440) {
      refresh = 10;
    }
    refreshIntervalMs = refresh * 60 * 1000;
    sensor.begin(rx_pin, tx_pin);

    pm25channel = new GeneralPurposeMeasurement();
    pm25channel->setDefaultUnitAfterValue("μg/m³");
    pm25channel->setInitialCaption("PM 2.5");
    pm25channel->getChannel()->setDefaultIcon(8);
    pm25channel->setDefaultValuePrecision(1);

    pm10channel = new GeneralPurposeMeasurement();
    pm10channel->setDefaultUnitAfterValue("μg/m³");
    pm10channel->setInitialCaption("PM 10");
    pm10channel->getChannel()->setDefaultIcon(8);
    pm10channel->setDefaultValuePrecision(1);
  }

  void iterateAlways() override {
    if (millis() - lastReadTime > refreshIntervalMs) {
      float pm25 = NAN;
      float pm10 = NAN;
      sensor.read(&pm25, &pm10);
      lastReadTime = millis();

      pm25channel->setValue(pm25);
      pm10channel->setValue(pm10);
    }
  }

 protected:
  ::SDS011 sensor;
  uint32_t refreshIntervalMs = 600000;
  uint32_t lastReadTime = 0;

  GeneralPurposeMeasurement *pm25channel = nullptr;
  GeneralPurposeMeasurement *pm10channel = nullptr;
};

}  // namespace Sensor
}  // namespace Supla

#endif  // SRC_SUPLA_SENSOR_SDS011_H_

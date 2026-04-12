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

#ifndef SRC_SUPLA_SENSOR_SHT10_H_
#define SRC_SUPLA_SENSOR_SHT10_H_

#include <Arduino.h>
#include <SHT1x-ESP.h>  // SHT1x sensor library for ESPx by beegee_tokyo
// data pin pulled up with 10k resistor

namespace Supla {
namespace Sensor {
class SHT10 : public ThermHygroMeter {
 public:
  explicit SHT10(int data_pin_, int clock_pin_)
    : sht1x(data_pin_, clock_pin_, SHT1x::Voltage::DC_3_3v) {
  }

  double getTemp() {
    temperature = sht1x.readTemperatureC();
    if (isnan(temperature) || temperature < -30) {
      temperature = TEMPERATURE_NOT_AVAILABLE;
    }
    return temperature;
  }

  double getHumi() {
    humidity = sht1x.readHumidity();
    if (isnan(humidity) || humidity < 0) {
      humidity = HUMIDITY_NOT_AVAILABLE;
    }
    return humidity;
  }

 private:
  void iterateAlways() {
    if (millis() - lastReadTime > 10000) {
      lastReadTime = millis();
      channel.setNewValue(getTemp(), getHumi());
    }
  }

  void onInit() {
    channel.setNewValue(getTemp(), getHumi());
    SHT1x::TemperatureMeasurementResolution::Temperature_12bit;
    SHT1x::HumidityMeasurementResolution::Humidity_8bit;
  }

 protected:
  ::SHT1x sht1x;
  double temperature = TEMPERATURE_NOT_AVAILABLE;
  double humidity = HUMIDITY_NOT_AVAILABLE;
};
};  // namespace Sensor
};  // namespace Supla

#endif  // SRC_SUPLA_SENSOR_SHT10_H_

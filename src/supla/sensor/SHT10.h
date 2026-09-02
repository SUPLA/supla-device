// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_SENSOR_SHT10_H_
#define SRC_SUPLA_SENSOR_SHT10_H_

#include <Arduino.h>
#include <supla/sensor/therm_hygro_meter.h>
#include <SHT1x-ESP.h>  // https://github.com/beegee-tokyo/SHT1x-ESP
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
  }

 protected:
  ::SHT1x sht1x;
  double temperature = TEMPERATURE_NOT_AVAILABLE;
  double humidity = HUMIDITY_NOT_AVAILABLE;
};
};  // namespace Sensor
};  // namespace Supla

#endif  // SRC_SUPLA_SENSOR_SHT10_H_

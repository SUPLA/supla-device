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

   You should have received a copy of the GNU General Public License898
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
   */

// Dependencies:
// https://github.com/kevinlutzer/Arduino-PM1006K

#ifndef SRC_SUPLA_SENSOR_AIR_QUALITY_PM1006K_H_
#define SRC_SUPLA_SENSOR_AIR_QUALITY_PM1006K_H_

#include <supla/sensor/general_purpose_measurement.h>

#include <PM1006K.h>

namespace Supla {
namespace Sensor {
class AirQualityPm1006k : public GeneralPurposeMeasurement {
 public:
  // rx_pin, tx_pin: pins to which the sensor is connected
  // fan_pin: pin to powering fan (HIGH is enabled)
  // refresh: time between readings (in sec: 60-86400)
  // fan: fan working time (in sec: 15-120)
  explicit AirQualityPm1006k(int rx_pin, int tx_pin, int fan_pin = -1,
      int refresh = 600, int fan = 15)
      : GeneralPurposeMeasurement(nullptr, false) {
    if (refresh < 60) {
      refresh = 600;
    } else if (refresh > 86400) {
      refresh = 600;
    }
    if (fan < 15) {
      fan = 15;
    } else if (fan > 120) {
      fan = 120;
    } else if (fan > refresh) {
      fan = refresh;
    }

    // FAN setup
    fanPin = fan_pin;
    if (fanPin >= 0) {
      pinMode(fanPin, OUTPUT);
      fanOff = false;
      digitalWrite(fanPin, HIGH);
      SUPLA_LOG_DEBUG("PM1006K FAN: started & on");
    }

    // Setup and create instance of the PM1006K driver
    // The baud rate for the serial connection must be PM1006K::BAUD_RATE.
    Serial1.begin(PM1006K::BAUD_RATE, SERIAL_8N1, rx_pin, tx_pin);
    sensor = new PM1006K(&Serial1);

    refreshIntervalMs = refresh * 60 * 1000;
    fanTime = fan * 1000;
    setDefaultUnitAfterValue("μg/m³");
    setInitialCaption("PM 2.5");
    getChannel()->setDefaultIcon(8);
  }

  void iterateAlways() override {
    // enable fan fanTime before reading sensor
    if (millis() - lastReadTime > refreshIntervalMs-fanTime) {
      if ((fanPin >= 0)  && fanOff) {
        fanOff = false;
        digitalWrite(fanPin, HIGH);
        SUPLA_LOG_DEBUG("PM1006K FAN: on");
      }
    }

    if (millis() - lastReadTime > refreshIntervalMs) {
      double value = NAN;
      if (!sensor->takeMeasurement()) {
        SUPLA_LOG_DEBUG("PM1006K: failed to take measurement");
      } else {
        value = sensor->getPM2_5();
        SUPLA_LOG_DEBUG("PM1006K: PM2.5 read: %.0f", value);
      }

      if (isnan(value) || value <= 0) {
        if (invalidCounter < 3) {
          invalidCounter++;
        } else {
          lastValue = NAN;
        }
      } else {
        invalidCounter = 0;
        lastValue = value;
        if (fanPin >= 0) {
          fanOff = true;
          digitalWrite(fanPin, LOW);
          SUPLA_LOG_DEBUG("PM1006K FAN: off");
        }
      }
      channel.setNewValue(getValue());
      lastReadTime = millis();
    }
  }

  double getValue() override {
    return lastValue;
  }

 protected:
  ::PM1006K* sensor = nullptr;
  uint32_t refreshIntervalMs = 600000;
  uint32_t lastReadTime = 0;
  double lastValue = NAN;
  int fanPin = 0;
  bool fanOff = true;
  uint32_t fanTime = 15;
  int invalidCounter = 0;
};

}  // namespace Sensor
}  // namespace Supla

#endif  // SRC_SUPLA_SENSOR_AIR_QUALITY_PM1006K_H_

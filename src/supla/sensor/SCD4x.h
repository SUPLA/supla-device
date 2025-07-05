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

#ifndef SRC_SUPLA_SENSOR_SCD4X_H_
#define SRC_SUPLA_SENSOR_SCD4X_H_

// Dependency: SparkFun_SCD4x_Arduino_Library
//             use library manager to install it
// https://github.com/sparkfun/SparkFun_SCD4x_Arduino_Library

#include "SparkFun_SCD4x_Arduino_Library.h"

#include "therm_hygro_meter.h"
#include "general_purpose_measurement.h"

namespace Supla {
namespace Sensor {
class SCD4x : public ThermHygroMeter {
 public:
  SCD4x() {
    co2channel = new GeneralPurposeMeasurement();
    co2channel->setDefaultUnitAfterValue("ppm");
    co2channel->setInitialCaption("CO₂");
    co2channel->getChannel()->setDefaultIcon(8);
    co2channel->setDefaultValuePrecision(1);
  }

  double getTemp() override {
    readValuesFromDevice();
    return temperature;
  }

  double getHumi() override {
    return humidity;
  }

  double getCO2() {
    return co2;
  }

  void onInit() override {
    if (scd.begin() == false) {
      SUPLA_LOG_DEBUG("SCD4x Sensor not detected. Please check wiring.");
    } else {
      SUPLA_LOG_DEBUG("SCD4x Sensor detected.");
    }
  }

  GeneralPurposeMeasurement* getCO2channel() {
    return co2channel;
  }

 private:
  void readValuesFromDevice() {
    if (scd.readMeasurement()) {
      retryCount = 0;
      temperature = scd.getTemperature();
      humidity = scd.getHumidity();
      co2 = scd.getCO2();
      co2channel->setValue(co2);
    } else {
      SUPLA_LOG_DEBUG("SCD4x read error");
      retryCount++;
      if (retryCount > 3) {
        retryCount = 0;
        temperature = TEMPERATURE_NOT_AVAILABLE;
        humidity = HUMIDITY_NOT_AVAILABLE;
        co2 = NAN;
      }
    }
  }

 protected:
  double temperature = TEMPERATURE_NOT_AVAILABLE;
  double humidity = HUMIDITY_NOT_AVAILABLE;
  double co2 = NAN;
  int8_t retryCount = 0;
  ::SCD4x scd;
  GeneralPurposeMeasurement *co2channel = nullptr;
};

};  // namespace Sensor
};  // namespace Supla

#endif  // SRC_SUPLA_SENSOR_SCD4X_H_

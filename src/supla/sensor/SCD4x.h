// SPDX-FileCopyrightText: malarz
// SPDX-License-Identifier: GPL-2.0-or-later

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

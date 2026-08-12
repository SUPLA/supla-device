// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_SENSOR_BME280_H_
#define SRC_SUPLA_SENSOR_BME280_H_

// Dependency: Adafruid BME280 library - use library manager to install it
#include <Adafruit_BME280.h>

#include "therm_hygro_press_meter.h"

namespace Supla {
namespace Sensor {
class BME280 : public ThermHygroPressMeter {
 public:
  explicit BME280(int8_t address = 0x77, float altitude = NAN)
      : address(address), sensorStatus(false), altitude(altitude) {
  }

  double getTemp() {
    float value = TEMPERATURE_NOT_AVAILABLE;
    bool retryDone = false;
    do {
      if (!sensorStatus || isnan(value)) {
        sensorStatus = bme.begin(address);
        retryDone = true;
      }
      value = TEMPERATURE_NOT_AVAILABLE;
      if (sensorStatus) {
        value = bme.readTemperature();
      }
    } while (isnan(value) && !retryDone);
    return value;
  }

  double getHumi() {
    float value = HUMIDITY_NOT_AVAILABLE;
    bool retryDone = false;
    do {
      if (!sensorStatus || isnan(value)) {
        sensorStatus = bme.begin(address);
        retryDone = true;
      }
      value = HUMIDITY_NOT_AVAILABLE;
      if (sensorStatus) {
        value = bme.readHumidity();
      }
    } while (isnan(value) && !retryDone);
    return value;
  }

  double getPressure() {
    float value = PRESSURE_NOT_AVAILABLE;
    bool retryDone = false;
    do {
      if (!sensorStatus || isnan(value)) {
        sensorStatus = bme.begin(address);
        retryDone = true;
      }
      value = PRESSURE_NOT_AVAILABLE;
      if (sensorStatus) {
        value = bme.readPressure() / 100.0;
      }
    } while (isnan(value) && !retryDone);
    if (!isnan(altitude)) {
      value = bme.seaLevelForAltitude(altitude, value);
    }
    return value;
  }

  void onInit() {
    sensorStatus = bme.begin(address);

    pressureChannel.setNewValue(getPressure());
    channel.setNewValue(getTemp(), getHumi());
  }

  void setAltitude(float newAltitude) {
    altitude = newAltitude;
  }

 protected:
  int8_t address;
  bool sensorStatus;
  float altitude;
  Adafruit_BME280 bme;  // I2C
};

};  // namespace Sensor
};  // namespace Supla

#endif  // SRC_SUPLA_SENSOR_BME280_H_

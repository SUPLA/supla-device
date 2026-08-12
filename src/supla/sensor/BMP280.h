// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O., malarz
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_SENSOR_BMP280_H_
#define SRC_SUPLA_SENSOR_BMP280_H_

// Dependency: Adafruid BMP280 library - use library manager to install it
#include <Adafruit_BMP280.h>

#include "therm_press_meter.h"

namespace Supla {
namespace Sensor {
class BMP280 : public ThermPressMeter {
 public:
  explicit BMP280(int8_t address = 0x76, float altitude = NAN)
      : address(address), sensorStatus(false), altitude(altitude) {
  }

  double getTemp() {
    float value = TEMPERATURE_NOT_AVAILABLE;
    bool retryDone = false;
    do {
      if (!sensorStatus || isnan(value)) {
        sensorStatus = bmp.begin(address);
        retryDone = true;
      }
      value = TEMPERATURE_NOT_AVAILABLE;
      if (sensorStatus) {
        value = bmp.readTemperature();
      }
    } while (isnan(value) && !retryDone);
    return value;
  }

double getPressure() {
    float value = PRESSURE_NOT_AVAILABLE;
    bool retryDone = false;
    do {
      if (!sensorStatus || isnan(value)) {
        sensorStatus = bmp.begin(address);
        retryDone = true;
      }
      value = PRESSURE_NOT_AVAILABLE;
      if (sensorStatus) {
        value = bmp.readPressure() / 100.0;
      }
    } while (isnan(value) && !retryDone);
    if (!isnan(altitude)) {
      value = bmp.seaLevelForAltitude(altitude, value);
    }
    return value;
  }

  void onInit() {
    sensorStatus = bmp.begin(address);

    channel.setNewValue(getTemp());
    pressureChannel.setNewValue(getPressure());
  }

  void setAltitude(float newAltitude) {
    altitude = newAltitude;
  }

 protected:
  int8_t address;
  bool sensorStatus;
  float altitude;
  Adafruit_BMP280 bmp;  // I2C
};

};  // namespace Sensor
};  // namespace Supla

#endif  // SRC_SUPLA_SENSOR_BMP280_H_

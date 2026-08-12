// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O., malarz
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_SENSOR_BMP180_H_
#define SRC_SUPLA_SENSOR_BMP180_H_

// Dependency: Adafruid BMP085 library - use library manager to install it
// BMP180 is upgraded version of BMP085
#include <Adafruit_BMP085.h>

#include "therm_press_meter.h"

namespace Supla {
namespace Sensor {
class BMP180 : public ThermPressMeter {
 public:
  explicit BMP180(int8_t address = 0x77)
      : address(address), sensorStatus(false) {
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
    return value;
  }

  void onInit() {
    sensorStatus = bmp.begin(address);

    channel.setNewValue(getTemp());
    pressureChannel.setNewValue(getPressure());
  }

 protected:
  int8_t address;
  bool sensorStatus;
  Adafruit_BMP085 bmp;  // I2C
};

};  // namespace Sensor
};  // namespace Supla

#endif  // SRC_SUPLA_SENSOR_BMP180_H_

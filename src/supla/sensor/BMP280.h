/*
 Copyright (C) AC SOFTWARE SP. Z O.O., malarz

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

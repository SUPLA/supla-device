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

#ifndef SRC_SUPLA_SENSOR_SHT3X_H_
#define SRC_SUPLA_SENSOR_SHT3X_H_

// Dependency: ClosedCube SHT3x library - use library manager to install it
// https://github.com/closedcube/ClosedCube_SHT31D_Arduino

#include <ClosedCube_SHT31D.h>

#include "therm_hygro_meter.h"

namespace Supla {
namespace Sensor {
class SHT3x : public ThermHygroMeter {
 public:
  explicit SHT3x(int8_t address = 0x44) : address(address) {
  }

  double getTemp() override {
    readValuesFromDevice();
    return temperature;
  }

  double getHumi() override {
    return humidity;
  }

  void onInit() override {
    sht.begin(address);
    channel.setNewValue(getTemp(), getHumi());
  }

 private:
  void readValuesFromDevice() {
    SHT31D result = sht.readTempAndHumidity(
        SHT3XD_REPEATABILITY_LOW, SHT3XD_MODE_CLOCK_STRETCH, 50);

    if (result.error != SHT3XD_NO_ERROR) {
      Serial.print(F("SHT [ERROR] Code #"));
      Serial.println(result.error);
      retryCount++;
      if (retryCount > 3) {
        retryCount = 0;
        temperature = TEMPERATURE_NOT_AVAILABLE;
        humidity = HUMIDITY_NOT_AVAILABLE;
      }
    } else {
      retryCount = 0;
      temperature = result.t;
      humidity = result.rh;
    }
  }

 protected:
  int8_t address;
  double temperature = TEMPERATURE_NOT_AVAILABLE;
  double humidity = HUMIDITY_NOT_AVAILABLE;
  int8_t retryCount = 0;
  ::ClosedCube_SHT31D sht;  // I2C
};

};  // namespace Sensor
};  // namespace Supla

#endif  // SRC_SUPLA_SENSOR_SHT3X_H_

// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_SENSOR_SHT3X_H_
#define SRC_SUPLA_SENSOR_SHT3X_H_

// Dependency: ClosedCube SHT3x library - use library manager to install it
// https://github.com/closedcube/ClosedCube_SHT31D_Arduino (currently
// unavailable)
// https://github.com/malarz-supla/ClosedCube_SHT31D_Arduino (fork with fixes)

#include <ClosedCube_SHT31D.h>
#include <supla/log_wrapper.h>

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
      SUPLA_LOG_ERROR("SHT [ERROR] Code #%d", result.error);
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

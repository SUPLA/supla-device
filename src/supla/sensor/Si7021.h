// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_SENSOR_SI7021_H_
#define SRC_SUPLA_SENSOR_SI7021_H_

// Dependency: Adafruid Si7021 library - use library manager to install it
// https://github.com/adafruit/Adafruit_Si7021

#include <Adafruit_Si7021.h>
#include <supla/log_wrapper.h>

#include "therm_hygro_meter.h"

namespace Supla {
namespace Sensor {
class Si7021 : public ThermHygroMeter {
 public:
  Si7021() {
  }

  double getTemp() {
    float value = TEMPERATURE_NOT_AVAILABLE;
    value = sensor.readTemperature();

    if (isnan(value)) {
      value = TEMPERATURE_NOT_AVAILABLE;
    }

    return value;
  }

  double getHumi() {
    float value = HUMIDITY_NOT_AVAILABLE;
    value = sensor.readHumidity();

    if (isnan(value)) {
      value = HUMIDITY_NOT_AVAILABLE;
    }

    return value;
  }

  void onInit() {
    sensor.begin();

    switch (sensor.getModel()) {
      case SI_Engineering_Samples:
        SUPLA_LOG_INFO("Found model: SI engineering samples");
        break;
      case SI_7013:
        SUPLA_LOG_INFO("Found model: Si7013");
        break;
      case SI_7020:
        SUPLA_LOG_INFO("Found model: Si7020");
        break;
      case SI_7021:
        SUPLA_LOG_INFO("Found model: Si7021");
        break;
      case SI_UNKNOWN:
      default:
        SUPLA_LOG_INFO("Unknown model");
    }

    channel.setNewValue(getTemp(), getHumi());
  }

 protected:
  ::Adafruit_Si7021 sensor;  // I2C
};

};  // namespace Sensor
};  // namespace Supla

#endif  // SRC_SUPLA_SENSOR_SI7021_H_

// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_SENSOR_AHT_H_
#define SRC_SUPLA_SENSOR_AHT_H_

#include <Adafruit_AHTX0.h>

#include "therm_hygro_meter.h"

namespace Supla {
namespace Sensor {
class AHT : public ThermHygroMeter {
 public:
  void onInit() {
    aht.begin();
  }

  double getTemp() {
    double value = TEMPERATURE_NOT_AVAILABLE;
    aht.getEvent(&humidity, &temp);
    value = temp.temperature;
    if (isnan(value)) {
      value = TEMPERATURE_NOT_AVAILABLE;
    }
    if (value == TEMPERATURE_NOT_AVAILABLE) {
      retryCountTemp++;
      if (retryCountTemp > 3) {
        retryCountTemp = 0;
      } else {
        value = lastValidTemp;
      }
    } else {
      retryCountTemp = 0;
    }
    lastValidTemp = value;
    return value;
  }

  double getHumi() {
    double value = HUMIDITY_NOT_AVAILABLE;
    value = humidity.relative_humidity;
    if (isnan(value)) {
      value = HUMIDITY_NOT_AVAILABLE;
    }
    if (value == HUMIDITY_NOT_AVAILABLE) {
      retryCountHumi++;
      if (retryCountHumi > 3) {
        retryCountHumi = 0;
      } else {
        value = lastValidHumi;
      }
    } else {
      retryCountHumi = 0;
    }
    lastValidHumi = value;
    return value;
  }

 protected:
  Adafruit_AHTX0 aht;
  double lastValidTemp = TEMPERATURE_NOT_AVAILABLE;
  double lastValidHumi = HUMIDITY_NOT_AVAILABLE;
  int8_t retryCountTemp = 0;
  int8_t retryCountHumi = 0;
  sensors_event_t temp;
  sensors_event_t humidity;
};
};      // namespace Sensor
};      // namespace Supla
#endif  // SRC_SUPLA_SENSOR_AHT_H_

// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_SENSOR_DHT_H_
#define SRC_SUPLA_SENSOR_DHT_H_

#include <DHT.h>

#include "therm_hygro_meter.h"

namespace Supla {
namespace Sensor {
class DHT : public ThermHygroMeter {
 public:
  DHT(int pin, int dhtType) : dht(pin, dhtType) {
    dht.begin();
    delay(100);
    retryCountTemp = 0;
    retryCountHumi = 0;
    lastValidTemp = TEMPERATURE_NOT_AVAILABLE;
    lastValidHumi = HUMIDITY_NOT_AVAILABLE;
  }

  double getTemp() {
    double value = TEMPERATURE_NOT_AVAILABLE;
    value = dht.readTemperature();
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
    value = dht.readHumidity();
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
  ::DHT dht;
  double lastValidTemp;
  double lastValidHumi;
  int8_t retryCountTemp;
  int8_t retryCountHumi;
};

};  // namespace Sensor
};  // namespace Supla

#endif  // SRC_SUPLA_SENSOR_DHT_H_

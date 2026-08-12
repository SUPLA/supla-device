// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_ESP_IDF_SUPLA_SHT40_ESP_SHT40_H_
#define EXTRAS_ESP_IDF_SUPLA_SHT40_ESP_SHT40_H_

#include <driver/i2c.h>

#include <supla/sensor/therm_hygro_meter.h>

namespace Supla {
namespace Sensor {
class SHT40 : public ThermHygroMeter {
 public:
  SHT40(int sda, int scl, uint8_t addr);

  void onInit() override;
  void iterateAlways() override;

  double getTemp() override;
  double getHumi() override;

  void readSensor();

 protected:
  double lastValidTemp = TEMPERATURE_NOT_AVAILABLE;
  double lastValidHumi = HUMIDITY_NOT_AVAILABLE;
  int8_t retryCountTemp = 0;
  int8_t retryCountHumi = 0;
  int sda = 0;
  int scl = 0;
  uint8_t addr = 0x44;
};

};  // namespace Sensor
};  // namespace Supla

#endif  // EXTRAS_ESP_IDF_SUPLA_SHT40_ESP_SHT40_H_

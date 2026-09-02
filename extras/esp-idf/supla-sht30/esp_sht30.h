// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_ESP_IDF_SUPLA_SHT30_ESP_SHT30_H_
#define EXTRAS_ESP_IDF_SUPLA_SHT30_ESP_SHT30_H_

#include <esp_i2c_driver.h>
#include <supla/sensor/therm_hygro_meter.h>
#include <driver/i2c_master.h>

namespace Supla {
namespace Sensor {
class SHT30 : public ThermHygroMeter {
 public:
  SHT30(Supla::I2CDriver *driver, uint8_t addr);

  void onInit() override;

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
  Supla::I2CDriver *driver = nullptr;
  i2c_master_dev_handle_t devHandle = nullptr;
};

};  // namespace Sensor
};  // namespace Supla

#endif  // EXTRAS_ESP_IDF_SUPLA_SHT30_ESP_SHT30_H_

// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_ESP_IDF_SUPLA_I2C_DRIVER_ESP_I2C_DRIVER_H_
#define EXTRAS_ESP_IDF_SUPLA_I2C_DRIVER_ESP_I2C_DRIVER_H_

#include <driver/i2c_types.h>

namespace Supla {
class Mutex;

class I2CDriver {
 public:
  I2CDriver(int sda, int scl, bool internalPullUp = false);
  ~I2CDriver();

  void initialize();
  void deinitialize();
  bool isInitialized() const;

  void aquire();
  void release();

  i2c_master_dev_handle_t addDevice(uint8_t address, uint32_t frequency);
  void releaseDevice(i2c_master_dev_handle_t *handle);

 protected:
  int sda = 0;
  int scl = 0;
  bool initialized = false;
  i2c_master_bus_handle_t busHandle = nullptr;
  Supla::Mutex *mutex = nullptr;
  bool internalPullUp = false;
};

}  // namespace Supla

#endif  // EXTRAS_ESP_IDF_SUPLA_I2C_DRIVER_ESP_I2C_DRIVER_H_

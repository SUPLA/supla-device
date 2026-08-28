// SPDX-FileCopyrightText: AC SOFTWARE SP. Z.O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_TEST_MCP23008TESTS_ESP_I2C_DRIVER_H_
#define EXTRAS_TEST_MCP23008TESTS_ESP_I2C_DRIVER_H_

#include <stdint.h>

#include <driver/i2c_master.h>

namespace Supla {
class I2CDriver {
 public:
  I2CDriver(int sda, int scl, bool internalPullUp = false)
      : deviceHandle(this) {
    (void)sda;
    (void)scl;
    (void)internalPullUp;
  }

  ~I2CDriver() = default;

  void initialize() { initialized = true; }

  void deinitialize() { initialized = false; }

  bool isInitialized() const { return initialized; }

  void aquire() {}

  void release() {}

  i2c_master_dev_handle_t addDevice(uint8_t address, uint32_t frequency) {
    (void)address;
    (void)frequency;
    return deviceHandle;
  }

  void releaseDevice(i2c_master_dev_handle_t *handle) { (void)handle; }

  i2c_master_dev_handle_t deviceHandle = nullptr;

 private:
  bool initialized = false;
};
}  // namespace Supla

#endif  // EXTRAS_TEST_MCP23008TESTS_ESP_I2C_DRIVER_H_

// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_ESP_IDF_SUPLA_I2C_DRIVER_DEPRECATED_ESP_I2C_DRIVER_H_
#define EXTRAS_ESP_IDF_SUPLA_I2C_DRIVER_DEPRECATED_ESP_I2C_DRIVER_H_

#include <driver/i2c.h>

namespace Supla {
class I2CDriver {
 public:
  I2CDriver(int sda, int scl);

  void initialize();
  bool isInitialized() const;

  i2c_port_t getI2CNumber() const;

 protected:
  int sda = 0;
  int scl = 0;
  bool initialized = false;
  i2c_port_t i2cNumber = {};
};

}  // namespace Supla

#endif  // EXTRAS_ESP_IDF_SUPLA_I2C_DRIVER_DEPRECATED_ESP_I2C_DRIVER_H_

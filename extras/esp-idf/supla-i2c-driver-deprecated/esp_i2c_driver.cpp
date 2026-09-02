// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "esp_i2c_driver.h"
#include <supla/log_wrapper.h>

using Supla::I2CDriver;

I2CDriver::I2CDriver(int sda, int scl) : sda(sda), scl(scl) {
}

void I2CDriver::initialize() {
  if (!isInitialized()) {
    static int i2cCounter = 0;
    if (i2cCounter >= I2C_NUM_MAX) {
      SUPLA_LOG_ERROR("Too many I2C drivers");
      return;
    }

    i2cNumber = static_cast<i2c_port_t>(i2cCounter);
    i2cCounter++;

    i2c_config_t conf = {};

    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = sda;
    conf.scl_io_num = scl;
    conf.sda_pullup_en = false;
    conf.scl_pullup_en = false;
    conf.master.clk_speed = 40000;

    i2c_param_config(I2C_NUM_0, &conf);

    esp_err_t err = (i2c_driver_install(I2C_NUM_0, conf.mode, 0, 0, 0));

    if (err == ESP_OK) {
      SUPLA_LOG_INFO("I2C driver initialized, %d", i2cNumber);
      initialized = true;
    } else {
      SUPLA_LOG_WARNING("Failed to init i2c (%d)", err);
    }
  }
}

bool I2CDriver::isInitialized() const {
  return initialized;
}

i2c_port_t I2CDriver::getI2CNumber() const {
  return i2cNumber;
}

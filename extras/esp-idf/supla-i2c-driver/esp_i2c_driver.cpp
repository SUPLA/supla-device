/*
   Copyright (C) AC SOFTWARE SP. Z O.O

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
   */

#include "esp_i2c_driver.h"

#include <supla/log_wrapper.h>
#include <driver/i2c_master.h>
#include <supla/mutex.h>

using Supla::I2CDriver;

I2CDriver::I2CDriver(int sda, int scl) : sda(sda), scl(scl) {
  mutex = Supla::Mutex::Create();
  mutex->unlock();
}

I2CDriver::~I2CDriver() {
  if (mutex) {
    delete mutex;
    mutex = nullptr;
  }
}

void I2CDriver::initialize() {
  if (!isInitialized()) {
    i2c_master_bus_config_t conf = {};

    conf.clk_source = I2C_CLK_SRC_DEFAULT;
    conf.sda_io_num = static_cast<gpio_num_t>(sda);
    conf.scl_io_num = static_cast<gpio_num_t>(scl);
    conf.flags.enable_internal_pullup = 0;
    conf.glitch_ignore_cnt = 7;

    esp_err_t err = i2c_new_master_bus(&conf, &busHandle);

    if (err == ESP_OK) {
      SUPLA_LOG_INFO("I2C driver initialized");
      initialized = true;
    } else {
      SUPLA_LOG_WARNING("Failed to init i2c (%d)", err);
    }
  }
}

i2c_master_dev_handle_t *I2CDriver::addDevice(uint8_t address,
                                              uint32_t frequency) {
  if (i2c_master_probe(busHandle, address, 200) != ESP_OK) {
    SUPLA_LOG_WARNING("Failed to probe i2c device 0x%2X", address);
    return nullptr;
  }
  i2c_device_config_t devCfg = {
      .dev_addr_length = I2C_ADDR_BIT_LEN_7,
      .device_address = address,
      .scl_speed_hz = frequency,
  };

  auto *devHandle = new i2c_master_dev_handle_t;
  if (devHandle == nullptr) {
    SUPLA_LOG_WARNING("Failed to allocate i2c device handle");
    return nullptr;
  }
  auto err = i2c_master_bus_add_device(busHandle, &devCfg, devHandle);
  if (err != ESP_OK) {
    SUPLA_LOG_WARNING("Failed to add i2c device (%d)", err);
  }
  return devHandle;
}

void I2CDriver::releaseDevice(i2c_master_dev_handle_t *handle) {
  if (handle == nullptr) {
    return;
  }
  i2c_master_bus_rm_device(*handle);
}

bool I2CDriver::isInitialized() const {
  return initialized;
}

void I2CDriver::aquire() {
  mutex->lock();
}

void I2CDriver::release() {
  mutex->unlock();
}


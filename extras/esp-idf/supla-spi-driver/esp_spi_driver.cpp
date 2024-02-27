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

#include "esp_spi_driver.h"

#include <supla/log_wrapper.h>

using Supla::SPIDriver;

SPIDriver::SPIDriver(int miso, int mosi, int clk)
    : miso(miso), mosi(mosi), clk(clk) {
}

void SPIDriver::initialize() {
  if (!isInitialized()) {
    esp_err_t ret;
    spi_bus_config_t buscfg = {};
    buscfg.miso_io_num = miso;
    buscfg.mosi_io_num = mosi;
    buscfg.sclk_io_num = clk;
    buscfg.quadwp_io_num = -1;
    buscfg.quadhd_io_num = -1;
    buscfg.max_transfer_sz = 0;
    ret = spi_bus_initialize(HSPI_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
      SUPLA_LOG_ERROR("Failed to initialize SPI bus (%d)", ret);
    } else {
      SUPLA_LOG_DEBUG("SPI bus initialized");
    }
    initialized = true;
  }
}

bool SPIDriver::addDevice(spi_device_interface_config_t *devcfg,
                          spi_device_handle_t *deviceHandle) {
  if (devcfg == nullptr || deviceHandle == nullptr) {
    return false;
  }
  initialize();

  auto ret = spi_bus_add_device(HSPI_HOST, devcfg, deviceHandle);
  if (ret != ESP_OK) {
    SUPLA_LOG_ERROR("Failed to add SPI device (%d)", ret);
    return false;
  }
  return true;
}


bool SPIDriver::isInitialized() const {
  return initialized;
}


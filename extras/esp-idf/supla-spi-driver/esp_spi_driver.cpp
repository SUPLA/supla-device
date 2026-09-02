// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "esp_spi_driver.h"

#include <supla/log_wrapper.h>

using Supla::SPIDriver;

SPIDriver::SPIDriver(int16_t miso, int16_t mosi, int16_t clk)
    : miso(miso), mosi(mosi), clk(clk) {
}

bool SPIDriver::initialize() {
  if (isInitialized()) {
    return true;
  }

  esp_err_t ret;
  spi_bus_config_t buscfg = {};
  buscfg.miso_io_num = miso;
  buscfg.mosi_io_num = mosi;
  buscfg.sclk_io_num = clk;
  buscfg.quadwp_io_num = -1;
  buscfg.quadhd_io_num = -1;
  buscfg.max_transfer_sz = 0;
  ret = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
  if (ret != ESP_OK) {
    SUPLA_LOG_ERROR("Failed to initialize SPI bus (%d)", ret);
    initialized = false;
    return false;
  }

  SUPLA_LOG_DEBUG("SPI bus initialized");
  initialized = true;
  return true;
}

bool SPIDriver::addDevice(spi_device_interface_config_t *devcfg,
                          spi_device_handle_t *deviceHandle) {
  if (devcfg == nullptr || deviceHandle == nullptr) {
    return false;
  }
  if (!initialize()) {
    return false;
  }

  auto ret = spi_bus_add_device(SPI2_HOST, devcfg, deviceHandle);
  if (ret != ESP_OK) {
    SUPLA_LOG_ERROR("Failed to add SPI device (%d)", ret);
    return false;
  }
  return true;
}


bool SPIDriver::isInitialized() const {
  return initialized;
}


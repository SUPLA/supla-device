// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_TEST_DOUBLES_ESP_IDF_DRIVER_SPI_MASTER_H_
#define EXTRAS_TEST_DOUBLES_ESP_IDF_DRIVER_SPI_MASTER_H_

#include <esp_err.h>

#include <stddef.h>
#include <stdint.h>

typedef int spi_host_device_t;
typedef int spi_dma_chan_t;
typedef void *spi_device_handle_t;

constexpr spi_host_device_t SPI2_HOST = 2;
constexpr spi_dma_chan_t SPI_DMA_CH_AUTO = 0;

typedef struct {
  int miso_io_num;
  int mosi_io_num;
  int sclk_io_num;
  int quadwp_io_num;
  int quadhd_io_num;
  size_t max_transfer_sz;
} spi_bus_config_t;

typedef struct {
  int dummy;
} spi_device_interface_config_t;

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t spi_bus_initialize(spi_host_device_t host_id,
                             const spi_bus_config_t *bus_config,
                             spi_dma_chan_t dma_chan);
esp_err_t spi_bus_add_device(spi_host_device_t host_id,
                             const spi_device_interface_config_t *dev_config,
                             spi_device_handle_t *handle);

#ifdef __cplusplus
}
#endif

#endif  // EXTRAS_TEST_DOUBLES_ESP_IDF_DRIVER_SPI_MASTER_H_

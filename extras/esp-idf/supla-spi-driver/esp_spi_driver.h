// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_ESP_IDF_SUPLA_SPI_DRIVER_ESP_SPI_DRIVER_H_
#define EXTRAS_ESP_IDF_SUPLA_SPI_DRIVER_ESP_SPI_DRIVER_H_

#include <driver/spi_master.h>

namespace Supla {
class SPIDriver {
 public:
  SPIDriver(int16_t miso, int16_t mosi, int16_t clk);

  void initialize();
  bool isInitialized() const;

  bool addDevice(spi_device_interface_config_t *devcfg,
                 spi_device_handle_t *deviceHandle);

 protected:
  int16_t miso = -1;
  int16_t mosi = -1;
  int16_t clk = -1;
  bool initialized = false;
};

}  // namespace Supla

#endif  // EXTRAS_ESP_IDF_SUPLA_SPI_DRIVER_ESP_SPI_DRIVER_H_

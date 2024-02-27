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

#ifndef EXTRAS_ESP_IDF_SUPLA_SPI_DRIVER_ESP_SPI_DRIVER_H_
#define EXTRAS_ESP_IDF_SUPLA_SPI_DRIVER_ESP_SPI_DRIVER_H_

#include <driver/spi_master.h>

namespace Supla {
class SPIDriver {
 public:
  SPIDriver(int miso, int mosi, int clk);

  void initialize();
  bool isInitialized() const;

  bool addDevice(spi_device_interface_config_t *devcfg,
                 spi_device_handle_t *deviceHandle);

 protected:
  int miso = -1;
  int mosi = -1;
  int clk = -1;
  bool initialized = false;
};

}  // namespace Supla

#endif  // EXTRAS_ESP_IDF_SUPLA_SPI_DRIVER_ESP_SPI_DRIVER_H_

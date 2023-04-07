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

#ifndef EXTRAS_ESP_IDF_SUPLA_I2C_DRIVER_ESP_I2C_DRIVER_H_
#define EXTRAS_ESP_IDF_SUPLA_I2C_DRIVER_ESP_I2C_DRIVER_H_

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

#endif  // EXTRAS_ESP_IDF_SUPLA_I2C_DRIVER_ESP_I2C_DRIVER_H_

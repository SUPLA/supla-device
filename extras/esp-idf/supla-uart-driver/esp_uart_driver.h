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

#ifndef EXTRAS_ESP_IDF_SUPLA_UART_DRIVER_ESP_UART_DRIVER_H_
#define EXTRAS_ESP_IDF_SUPLA_UART_DRIVER_ESP_UART_DRIVER_H_

#include "driver/uart.h"
#include "driver/gpio.h"

namespace Supla {
class UartDriver {
 public:
  UartDriver(int txGpio, int rxGpio, int uartNum);
  virtual ~UartDriver();

  void initialize();
  bool isInitialized() const;

  int read(void *buf, size_t maxSize);
  int write(const void *buf, size_t size);

 protected:
  bool initialized = false;
  int txGpio = 0;
  int rxGpio = 0;
  uart_port_t uartNum = static_cast<uart_port_t>(1);
};

}  // namespace Supla

#endif  // EXTRAS_ESP_IDF_SUPLA_UART_DRIVER_ESP_UART_DRIVER_H_

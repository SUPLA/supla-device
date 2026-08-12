// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

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

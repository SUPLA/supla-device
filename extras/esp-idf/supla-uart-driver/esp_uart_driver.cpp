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

#include "esp_uart_driver.h"
#include <supla/log_wrapper.h>
#include <supla/network/network.h>

#define RX_BUF_SIZE 1024

using Supla::UartDriver;

UartDriver::UartDriver(int txGpio, int rxGpio, int uartNum)
    : txGpio(txGpio),
      rxGpio(rxGpio),
      uartNum(static_cast<uart_port_t>(uartNum)) {
}

UartDriver::~UartDriver() {
}


void UartDriver::initialize() {
  if (!isInitialized()) {
    const uart_config_t uartConfig = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,
        .source_clk = UART_SCLK_DEFAULT,
    };
    // We won't use a buffer for sending data.
    uart_driver_install(uartNum, RX_BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_param_config(uartNum, &uartConfig);
    uart_set_pin(uartNum, txGpio, rxGpio, -1, -1);
    initialized = true;
  }
}

bool UartDriver::isInitialized() const {
  return initialized;
}

int UartDriver::read(void *buf, size_t maxSize) {
  size_t size = 0;
  uart_get_buffered_data_len(uartNum, &size);
  if (size > 0) {
    if (size > maxSize) {
      size = maxSize;
    }
    int ret = uart_read_bytes(uartNum, buf, size, 1);
    if (ret > 0) {
      Supla::Network::printData(" *** UART Recv", buf, ret);
    }
    return ret;
  }
  return 0;
}

int UartDriver::write(const void *buf, size_t size) {
  Supla::Network::printData(" *** UART Send", buf, size);
  return uart_write_bytes(uartNum, buf, size);
}


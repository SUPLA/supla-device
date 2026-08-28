// SPDX-FileCopyrightText: AC SOFTWARE SP. Z.O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_TEST_MCP23008TESTS_DRIVER_I2C_MASTER_H_
#define EXTRAS_TEST_MCP23008TESTS_DRIVER_I2C_MASTER_H_

#include <stddef.h>
#include <stdint.h>

using esp_err_t = int;
using i2c_master_dev_handle_t = void *;

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t i2c_master_transmit(i2c_master_dev_handle_t i2c_dev,
                              const uint8_t *write_buffer,
                              size_t write_size,
                              int xfer_timeout_ms);
esp_err_t i2c_master_transmit_receive(i2c_master_dev_handle_t i2c_dev,
                                      const uint8_t *write_buffer,
                                      size_t write_size,
                                      uint8_t *read_buffer,
                                      size_t read_size,
                                      int xfer_timeout_ms);

#ifdef __cplusplus
}
#endif

#endif  // EXTRAS_TEST_MCP23008TESTS_DRIVER_I2C_MASTER_H_

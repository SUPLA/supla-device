// SPDX-FileCopyrightText: AC SOFTWARE SP. Z.O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <mcp23008_test_support.h>

#include <driver/i2c_master.h>

namespace {
int i2cAccessCount = 0;
uint8_t readValue = 0;
}  // namespace

esp_err_t i2c_master_transmit(i2c_master_dev_handle_t i2c_dev,
                              const uint8_t *write_buffer,
                              size_t write_size,
                              int xfer_timeout_ms) {
  (void)i2c_dev;
  (void)write_buffer;
  (void)write_size;
  (void)xfer_timeout_ms;
  i2cAccessCount++;
  return 0;
}

esp_err_t i2c_master_transmit_receive(i2c_master_dev_handle_t i2c_dev,
                                      const uint8_t *write_buffer,
                                      size_t write_size,
                                      uint8_t *read_buffer,
                                      size_t read_size,
                                      int xfer_timeout_ms) {
  (void)i2c_dev;
  (void)write_buffer;
  (void)write_size;
  (void)xfer_timeout_ms;
  i2cAccessCount++;
  if (read_buffer != nullptr && read_size > 0) {
    read_buffer[0] = readValue;
  }
  return 0;
}

void MCP23008TestSupport::reset() {
  i2cAccessCount = 0;
  readValue = 0;
}

void MCP23008TestSupport::setReadValue(uint8_t value) {
  readValue = value;
}

int MCP23008TestSupport::getI2cAccessCount() {
  return i2cAccessCount;
}

// SPDX-FileCopyrightText: AC SOFTWARE SP. Z.O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "esp_idf_spi_mock.h"

#include <driver/spi_master.h>

#include <vector>

namespace {
std::vector<esp_err_t> busInitializeResults;
esp_err_t busAddDeviceResult = ESP_OK;
size_t busInitializeCallCount = 0;
size_t busAddDeviceCallCount = 0;
}  // namespace

void EspIdfSpiMock::reset() {
  busInitializeResults.clear();
  busAddDeviceResult = ESP_OK;
  busInitializeCallCount = 0;
  busAddDeviceCallCount = 0;
}

void EspIdfSpiMock::setBusInitializeResults(
    const std::vector<esp_err_t> &results) {
  busInitializeResults = results;
}

void EspIdfSpiMock::setBusAddDeviceResult(esp_err_t result) {
  busAddDeviceResult = result;
}

size_t EspIdfSpiMock::getBusInitializeCallCount() {
  return busInitializeCallCount;
}

size_t EspIdfSpiMock::getBusAddDeviceCallCount() {
  return busAddDeviceCallCount;
}

extern "C" {

esp_err_t spi_bus_initialize(spi_host_device_t,
                             const spi_bus_config_t *,
                             spi_dma_chan_t) {
  const size_t callIndex = busInitializeCallCount++;
  if (callIndex < busInitializeResults.size()) {
    return busInitializeResults[callIndex];
  }
  return ESP_OK;
}

esp_err_t spi_bus_add_device(spi_host_device_t,
                             const spi_device_interface_config_t *,
                             spi_device_handle_t *handle) {
  busAddDeviceCallCount++;
  if (busAddDeviceResult == ESP_OK && handle != nullptr) {
    *handle = reinterpret_cast<spi_device_handle_t>(1);
  }
  return busAddDeviceResult;
}

}  // extern "C"

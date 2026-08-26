// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_TEST_DOUBLES_ESP_IDF_SPI_MOCK_H_
#define EXTRAS_TEST_DOUBLES_ESP_IDF_SPI_MOCK_H_

#include <esp_err.h>

#include <stddef.h>

#include <vector>

namespace EspIdfSpiMock {

void reset();
void setBusInitializeResults(const std::vector<esp_err_t> &results);
void setBusAddDeviceResult(esp_err_t result);
size_t getBusInitializeCallCount();
size_t getBusAddDeviceCallCount();

}  // namespace EspIdfSpiMock

#endif  // EXTRAS_TEST_DOUBLES_ESP_IDF_SPI_MOCK_H_

// SPDX-FileCopyrightText: AC SOFTWARE SP. Z.O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <esp_spi_driver.h>
#include <gtest/gtest.h>

#include <vector>

#include "esp_idf_spi_mock.h"

namespace {

class SPIDriverTests : public ::testing::Test {
 protected:
  void SetUp() override {
    EspIdfSpiMock::reset();
  }
};

TEST_F(SPIDriverTests, InitializesSuccessfully) {
  Supla::SPIDriver driver(1, 2, 3);

  EXPECT_TRUE(driver.initialize());
  EXPECT_TRUE(driver.isInitialized());
  EXPECT_EQ(EspIdfSpiMock::getBusInitializeCallCount(), 1);
}

TEST_F(SPIDriverTests, KeepsUninitializedAfterFailure) {
  EspIdfSpiMock::setBusInitializeResults({ESP_FAIL});
  Supla::SPIDriver driver(1, 2, 3);

  EXPECT_FALSE(driver.initialize());
  EXPECT_FALSE(driver.isInitialized());
}

TEST_F(SPIDriverTests, RetriesInitializationAfterFailure) {
  EspIdfSpiMock::setBusInitializeResults({ESP_FAIL, ESP_OK});
  Supla::SPIDriver driver(1, 2, 3);

  EXPECT_FALSE(driver.initialize());
  EXPECT_TRUE(driver.initialize());
  EXPECT_TRUE(driver.isInitialized());
  EXPECT_EQ(EspIdfSpiMock::getBusInitializeCallCount(), 2);
}

TEST_F(SPIDriverTests, DoesNotAddDeviceAfterInitializationFailure) {
  EspIdfSpiMock::setBusInitializeResults({ESP_FAIL});
  Supla::SPIDriver driver(1, 2, 3);
  spi_device_interface_config_t devcfg = {};
  spi_device_handle_t deviceHandle = nullptr;

  EXPECT_FALSE(driver.addDevice(&devcfg, &deviceHandle));
  EXPECT_EQ(EspIdfSpiMock::getBusAddDeviceCallCount(), 0);
}

TEST_F(SPIDriverTests, DoesNotInitializeBusAgainAfterSuccess) {
  Supla::SPIDriver driver(1, 2, 3);

  EXPECT_TRUE(driver.initialize());
  EXPECT_TRUE(driver.initialize());
  EXPECT_EQ(EspIdfSpiMock::getBusInitializeCallCount(), 1);
}

}  // namespace

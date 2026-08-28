// SPDX-FileCopyrightText: AC SOFTWARE SP. Z.O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include <supla_mcp23008.h>

#include <mcp23008_test_support.h>

namespace {

TEST(MCP23008Tests, ReadsValidPinFromInitializedDevice) {
  Supla::I2CDriver driver(1, 2);
  Supla::MCP23008 io(&driver);

  ASSERT_TRUE(io.init());
  MCP23008TestSupport::reset();
  MCP23008TestSupport::setReadValue(1 << 3);

  EXPECT_EQ(1, io.customDigitalRead(0, 3));
  EXPECT_EQ(1, MCP23008TestSupport::getI2cAccessCount());
}

TEST(MCP23008Tests, RejectsPinAboveRangeWithoutI2cAccess) {
  Supla::I2CDriver driver(1, 2);
  Supla::MCP23008 io(&driver);

  ASSERT_TRUE(io.init());
  MCP23008TestSupport::reset();

  EXPECT_EQ(0, io.customDigitalRead(0, 8));
  EXPECT_EQ(0, MCP23008TestSupport::getI2cAccessCount());
}

TEST(MCP23008Tests, ReturnsZeroForFailedInitializationWithoutI2cAccess) {
  Supla::I2CDriver driver(1, 2);
  driver.deviceHandle = nullptr;
  Supla::MCP23008 io(&driver);

  ASSERT_FALSE(io.init());
  MCP23008TestSupport::reset();

  EXPECT_EQ(0, io.customDigitalRead(0, 0));
  EXPECT_EQ(0, MCP23008TestSupport::getI2cAccessCount());
}

}  // namespace

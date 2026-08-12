// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>
#include <supla/device/register_device.h>
#include <supla/modbus/modbus_device_handler.h>

#include <cstring>
#include <string>

class ModbusDeviceHandlerTests : public ::testing::Test {
 protected:
  void SetUp() override {
    Supla::RegisterDevice::resetToDefaults();
    Supla::RegisterDevice::setName("0123456789ABCDEFGHIJKLMNOPQRSTU");
    Supla::RegisterDevice::setSoftVer("0123456789ABCDEFGHIJK");
    const char guid[] = {
        0x01,
        0x02,
        0x03,
        0x04,
        0x05,
        0x06,
        0x07,
        0x08,
        0x09,
        0x0A,
        0x0B,
        0x0C,
        0x0D,
        0x0E,
        0x0F,
        0x10,
    };
    Supla::RegisterDevice::setGUID(guid);
  }
};

static void expectReadDoesNotWritePastResponseBuffer(
    Supla::ModbusDeviceHandler& handler,
    uint16_t address,
    uint16_t nRegs,
    const char* expected,
    size_t expectedSize) {
  uint8_t response[10];
  std::memset(response, 0xCC, sizeof(response));

  auto result = handler.holdingProcessRequest(
      address, nRegs, response, Supla::Modbus::Access::READ);

  EXPECT_EQ(result, Supla::Modbus::Result::OK);
  for (size_t i = 0; i < expectedSize; ++i) {
    EXPECT_EQ(response[i], static_cast<uint8_t>(expected[i]));
  }
  for (size_t i = expectedSize; i < sizeof(response); ++i) {
    EXPECT_EQ(response[i], 0xCC);
  }
}

static std::string getGuidText() {
  char guidText[37] = {};
  Supla::RegisterDevice::fillGUIDText(guidText);
  return guidText;
}

TEST_F(ModbusDeviceHandlerTests, NameReadDoesNotWritePastResponseBuffer) {
  Supla::ModbusDeviceHandler handler;
  expectReadDoesNotWritePastResponseBuffer(handler, 0, 2, "0123", 4);
}

TEST_F(ModbusDeviceHandlerTests,
       NameReadFromMiddleDoesNotWritePastResponseBuffer) {
  Supla::ModbusDeviceHandler handler;
  expectReadDoesNotWritePastResponseBuffer(handler, 2, 3, "456789", 6);
}

TEST_F(ModbusDeviceHandlerTests, NameReadAtEndDoesNotWritePastResponseBuffer) {
  Supla::ModbusDeviceHandler handler;
  expectReadDoesNotWritePastResponseBuffer(handler, 14, 2, "STU\0", 4);
}

TEST_F(ModbusDeviceHandlerTests, SoftVerReadDoesNotWritePastResponseBuffer) {
  Supla::ModbusDeviceHandler handler;
  expectReadDoesNotWritePastResponseBuffer(handler, 16, 2, "0123", 4);
}

TEST_F(ModbusDeviceHandlerTests,
       SoftVerReadFromMiddleDoesNotWritePastResponseBuffer) {
  Supla::ModbusDeviceHandler handler;
  expectReadDoesNotWritePastResponseBuffer(handler, 18, 3, "456789", 6);
}

TEST_F(ModbusDeviceHandlerTests,
       SoftVerReadAtEndDoesNotWritePastResponseBuffer) {
  Supla::ModbusDeviceHandler handler;
  uint8_t response[10];
  std::memset(response, 0xCC, sizeof(response));

  auto result = handler.holdingProcessRequest(
      26, 2, response, Supla::Modbus::Access::READ);

  EXPECT_EQ(result, Supla::Modbus::Result::OK);
  EXPECT_EQ(response[0], 0x00);
  EXPECT_EQ(response[1], 0x00);
  EXPECT_EQ(response[2], '0');
  EXPECT_EQ(response[3], '1');
  for (size_t i = 4; i < sizeof(response); ++i) {
    EXPECT_EQ(response[i], 0xCC);
  }
}

TEST_F(ModbusDeviceHandlerTests, GuidReadDoesNotWritePastResponseBuffer) {
  Supla::ModbusDeviceHandler handler;
  auto guidText = getGuidText();
  expectReadDoesNotWritePastResponseBuffer(handler, 27, 2, guidText.c_str(), 4);
}

TEST_F(ModbusDeviceHandlerTests,
       GuidReadFromMiddleDoesNotWritePastResponseBuffer) {
  Supla::ModbusDeviceHandler handler;
  auto guidText = getGuidText();
  expectReadDoesNotWritePastResponseBuffer(
      handler, 31, 3, guidText.c_str() + 8, 6);
}

TEST_F(ModbusDeviceHandlerTests, GuidReadAtEndDoesNotWritePastResponseBuffer) {
  Supla::ModbusDeviceHandler handler;
  char guidText[38] = {};
  Supla::RegisterDevice::fillGUIDText(guidText);
  expectReadDoesNotWritePastResponseBuffer(handler, 44, 2, guidText + 34, 4);
}

TEST_F(ModbusDeviceHandlerTests,
       FullDumpOfSupportedRegistersStaysWithinBuffer) {
  Supla::ModbusDeviceHandler handler;
  uint8_t response[94];
  std::memset(response, 0xCC, sizeof(response));

  auto result = handler.holdingProcessRequest(
      0, 46, response, Supla::Modbus::Access::READ);

  EXPECT_EQ(result, Supla::Modbus::Result::OK);

  char expectedName[32] = {};
  const char nameLiteral[] = "0123456789ABCDEFGHIJKLMNOPQRSTU";
  std::memcpy(expectedName, nameLiteral, sizeof(nameLiteral) - 1);

  char expectedSoftVer[22] = {};
  const char softVerLiteral[] = "0123456789ABCDEFGHIJ";
  std::memcpy(expectedSoftVer, softVerLiteral, sizeof(softVerLiteral) - 1);

  const std::string guidText = getGuidText();

  EXPECT_EQ(std::memcmp(response, expectedName, sizeof(expectedName)), 0);
  EXPECT_EQ(
      std::memcmp(response + 32, expectedSoftVer, sizeof(expectedSoftVer)), 0);
  EXPECT_EQ(std::memcmp(response + 54, guidText.c_str(), guidText.size()), 0);

  EXPECT_EQ(response[92], 0xCC);
  EXPECT_EQ(response[93], 0xCC);
}

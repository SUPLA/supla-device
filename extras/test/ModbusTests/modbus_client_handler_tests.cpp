// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>
#include <supla/modbus/modbus_client_handler.h>

#include <cstdint>

namespace {

using Supla::Modbus::Access;
using Supla::Modbus::Result;
using Supla::ModbusClientHandler;

class InputOnlyHandler : public ModbusClientHandler {
 public:
  InputOnlyHandler() {
    modbusAddressOffset = 100;
    usedRegistersCount = 4;
  }

  bool isInputSupported() override {
    return true;
  }

  Result inputProcessRequest(uint16_t, uint16_t, uint8_t *buffer) override {
    buffer[0] = 0x12;
    buffer[1] = 0x34;
    return Result::OK;
  }
};

TEST(ModbusClientHandlerTests, InputOnlyHandlerCanBeReachedByInputDispatcher) {
  InputOnlyHandler handler;
  uint8_t bytes[] = {0xCC, 0xCC};

  ASSERT_TRUE(ModbusClientHandler::IsInputSupported());
  EXPECT_FALSE(ModbusClientHandler::IsHoldingSupported());
  EXPECT_EQ(ModbusClientHandler::InputProcessRequest(100, 1, bytes),
            Result::OK);
  EXPECT_EQ(bytes[0], 0x12);
  EXPECT_EQ(bytes[1], 0x34);
}

TEST(ModbusClientHandlerTests, InputOnlyHandlerDoesNotExposeHoldingRegisters) {
  InputOnlyHandler handler;
  uint8_t bytes[] = {0xCC, 0xCC};

  EXPECT_EQ(ModbusClientHandler::HoldingProcessRequest(
                100, 1, bytes, Access::READ),
            Result::INVALID_REGISTER_ADDRESS);
  EXPECT_EQ(bytes[0], 0xCC);
  EXPECT_EQ(bytes[1], 0xCC);
}

}  // namespace

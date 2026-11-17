// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>
#include <simple_time.h>
#include <supla/channel.h>
#include <supla/modbus/modbus_em_handler.h>
#include <supla/sensor/electricity_meter.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace {

using Supla::Modbus::Access;
using Supla::Modbus::Result;
using Supla::ModbusClientHandler;
using Supla::Sensor::ElectricityMeter;

class ModbusEMHandlerTests : public ::testing::TestWithParam<bool> {
 protected:
  void SetUp() override {
    Supla::Channel::resetToDefaults();
    meter = std::make_unique<ElectricityMeter>();
    handler = std::make_unique<Supla::ModbusEMHandler>(meter.get(), 100);
  }

  std::vector<uint16_t> read(uint16_t address, uint16_t count) {
    SCOPED_TRACE(::testing::Message() << "address=" << address
                                    << ", count=" << count);
    // Guard both sides of the exact response, including partial-field reads.
    std::vector<uint8_t> bytes(count * 2 + 4, 0xCC);
    auto result = GetParam()
                      ? ModbusClientHandler::InputProcessRequest(
                            address, count, bytes.data() + 2)
                      : ModbusClientHandler::HoldingProcessRequest(
                            address, count, bytes.data() + 2, Access::READ);
    EXPECT_EQ(result, Result::OK);
    EXPECT_EQ(bytes.front(), 0xCC);
    EXPECT_EQ(bytes[1], 0xCC);
    EXPECT_EQ(bytes[bytes.size() - 2], 0xCC);
    EXPECT_EQ(bytes.back(), 0xCC);
    std::vector<uint16_t> words;
    for (int i = 0; i < count; ++i) {
      words.push_back((bytes[2 + i * 2] << 8) | bytes[3 + i * 2]);
    }
    return words;
  }

  SimpleTime time;
  std::unique_ptr<ElectricityMeter> meter;
  std::unique_ptr<Supla::ModbusEMHandler> handler;
};

TEST_P(ModbusEMHandlerTests, ForwardBalancedEnergyFollowsMeterAndReset) {
  for (uint64_t value : {0x1122334455667788ULL, 0x1122334455667799ULL, 0ULL}) {
    SCOPED_TRACE(value);
    meter->setFwdBalancedEnergy(value);
    std::vector<uint16_t> expected = {
        static_cast<uint16_t>(value >> 48),
        static_cast<uint16_t>(value >> 32),
        static_cast<uint16_t>(value >> 16),
        static_cast<uint16_t>(value)};
    EXPECT_EQ(read(110, 4), expected);
    EXPECT_EQ(read(111, 2),
              (std::vector<uint16_t>{expected[1], expected[2]}));
  }
}

TEST_P(ModbusEMHandlerTests, CommonReservedRegistersPreserveEnergyAddresses) {
  meter->setVoltagePhaseAngle13(2400);
  // Match the existing diagnostic constant to isolate the buffer-position bug.
  meter->setFwdBalancedEnergy(0x0123456789ABCDEFULL);
  meter->setRvrBalancedEnergy(0x1020304050607080ULL);

  EXPECT_EQ(read(104, 14),
            (std::vector<uint16_t>{2400, 0, 0, 0, 0, 0,
                                   0x0123, 0x4567, 0x89AB, 0xCDEF,
                                   0x1020, 0x3040, 0x5060, 0x7080}));
}

TEST_P(ModbusEMHandlerTests, MissingVoltageSequencePreservesCurrentSequence) {
  meter->clearVoltagePhaseSequenceFlag();
  meter->setCurrentPhaseSequence(true);

  EXPECT_EQ(read(101, 2), (std::vector<uint16_t>{0, 2}));
}

TEST_P(ModbusEMHandlerTests, MissingCurrentSequencePreservesVoltageAngles) {
  meter->clearCurrentPhaseSequenceFlag();
  meter->setVoltagePhaseAngle12(1200);
  meter->setVoltagePhaseAngle13(2400);

  EXPECT_EQ(read(102, 3), (std::vector<uint16_t>{0, 1200, 2400}));
}

TEST_P(ModbusEMHandlerTests, PhaseReservedRegisterPreservesCurrentAddress) {
  for (int phase = 0; phase < 3; ++phase) {
    SCOPED_TRACE(phase);
    meter->setPowerFactor(phase, 987);
    meter->setCurrent(phase, 0x00012345);

    EXPECT_EQ(read(132 + phase * 30, 4),
              (std::vector<uint16_t>{987, 0, 0x0001, 0x2345}));
  }
}

TEST_P(ModbusEMHandlerTests, CommonTailPreservesFirstPhaseAddress) {
  meter->setVoltage(0, 23001);

  EXPECT_EQ(read(128, 3), (std::vector<uint16_t>{0, 0, 23001}));
}

TEST_P(ModbusEMHandlerTests, PhaseTailPreservesNextPhaseAddress) {
  for (int phase = 0; phase < 2; ++phase) {
    SCOPED_TRACE(phase);
    meter->setVoltage(phase + 1, 23123);

    EXPECT_EQ(read(158 + phase * 30, 3),
              (std::vector<uint16_t>{0, 0, 23123}));
  }
}

TEST_P(ModbusEMHandlerTests, PhaseBlockPreservesAllFieldAddressesAndUnits) {
  for (int phase = 0; phase < 3; ++phase) {
    SCOPED_TRACE(phase);
    meter->setVoltage(phase, 23001);
    meter->setPhaseAngle(phase, -123);
    meter->setPowerFactor(phase, -987);
    meter->setCurrent(phase, 0x00012345);
    meter->setPowerActive(phase, -123456700);
    meter->setPowerReactive(phase, 765432100);
    meter->setPowerApparent(phase, 123456700);
    meter->setFwdActEnergy(phase, 0x1122334455667788ULL);
    meter->setRvrActEnergy(phase, 0x99AABBCCDDEEFF00ULL);
    meter->setFwdReactEnergy(phase, 0x1020304050607080ULL);
    meter->setRvrReactEnergy(phase, 0x9080706050403020ULL);

    EXPECT_EQ(read(130 + phase * 30, 30),
              (std::vector<uint16_t>{
                  23001, 0xFF85, 0xFC25, 0,
                  0x0001, 0x2345, 0xFFED, 0x2979,
                  0x0074, 0xCBB1, 0x0012, 0xD687,
                  0x1122, 0x3344, 0x5566, 0x7788,
                  0x99AA, 0xBBCC, 0xDDEE, 0xFF00,
                  0x1020, 0x3040, 0x5060, 0x7080,
                  0x9080, 0x7060, 0x5040, 0x3020, 0, 0}));
  }
}

// Controls: distinguish the positioning bugs above from endian/unit errors.
TEST_P(ModbusEMHandlerTests, PresentSequencesAndAnglesUseDocumentedRegisters) {
  meter->setFreq(4999);
  meter->setVoltagePhaseSequence(false);
  meter->setCurrentPhaseSequence(true);
  meter->setVoltagePhaseAngle12(1200);
  meter->setVoltagePhaseAngle13(2400);

  EXPECT_EQ(read(100, 5), (std::vector<uint16_t>{4999, 1, 2, 1200, 2400}));
}

TEST_P(ModbusEMHandlerTests, Every64BitEnergyFragmentUsesBigEndianWordOrder) {
  meter->setRvrBalancedEnergy(0x1122334455667788ULL);
  std::vector<uint16_t> starts = {114};
  for (int phase = 0; phase < 3; ++phase) {
    meter->setFwdActEnergy(phase, 0x1122334455667788ULL);
    meter->setRvrActEnergy(phase, 0x1122334455667788ULL);
    meter->setFwdReactEnergy(phase, 0x1122334455667788ULL);
    meter->setRvrReactEnergy(phase, 0x1122334455667788ULL);
    for (int offset : {12, 16, 20, 24}) {
      starts.push_back(130 + phase * 30 + offset);
    }
  }
  const std::vector<uint16_t> words = {0x1122, 0x3344, 0x5566, 0x7788};
  for (auto start : starts) {
    for (int offset = 0; offset < 4; ++offset) {
      for (int count = 1; count <= 4 - offset; ++count) {
        SCOPED_TRACE(::testing::Message() << "field=" << start
                                        << ", offset=" << offset
                                        << ", count=" << count);
        EXPECT_EQ(read(start + offset, count),
                  (std::vector<uint16_t>(words.begin() + offset,
                                         words.begin() + offset + count)));
      }
    }
  }
}

TEST_P(ModbusEMHandlerTests, Every32BitFieldFragmentPreservesSignAndScale) {
  for (int phase = 0; phase < 3; ++phase) {
    meter->setCurrent(phase, 0x00012345);
    meter->setPowerActive(phase, -123456700);
    meter->setPowerReactive(phase, -123456700);
    meter->setPowerApparent(phase, 123456700);
    for (int field : {4, 6, 8, 10}) {
      uint16_t start = 130 + phase * 30 + field;
      const std::vector<uint16_t> words =
          field == 4 ? std::vector<uint16_t>{0x0001, 0x2345}
          : field == 10 ? std::vector<uint16_t>{0x0012, 0xD687}
                        : std::vector<uint16_t>{0xFFED, 0x2979};
      EXPECT_EQ(read(start, 2), words);
      EXPECT_EQ(read(start, 1), (std::vector<uint16_t>{words[0]}));
      EXPECT_EQ(read(start + 1, 1), (std::vector<uint16_t>{words[1]}));
    }
  }
}

TEST_P(ModbusEMHandlerTests, UnusedRegistersReadAsZeroInIsolation) {
  EXPECT_EQ(read(105, 5), std::vector<uint16_t>(5, 0));
  EXPECT_EQ(read(118, 12), std::vector<uint16_t>(12, 0));
  for (int phase = 0; phase < 3; ++phase) {
    EXPECT_EQ(read(133 + phase * 30, 1), std::vector<uint16_t>(1, 0));
    EXPECT_EQ(read(158 + phase * 30, 2), std::vector<uint16_t>(2, 0));
  }
}

TEST_P(ModbusEMHandlerTests, NonDefaultBaseStillUsesRelativeRegisterMap) {
  handler.reset();
  handler = std::make_unique<Supla::ModbusEMHandler>(meter.get(), 1000);
  meter->setFreq(5000);
  meter->setRvrBalancedEnergy(0x1122334455667788ULL);

  EXPECT_EQ(read(1000, 1), (std::vector<uint16_t>{5000}));
  EXPECT_EQ(read(1014, 4),
            (std::vector<uint16_t>{0x1122, 0x3344, 0x5566, 0x7788}));
}

TEST_P(ModbusEMHandlerTests, OutOfRangeRequestsAreRejectedWithoutBufferWrites) {
  for (auto request : {std::vector<uint16_t>{99, 1}, {220, 1}, {219, 2}}) {
    uint8_t bytes[] = {0xCC, 0xCC, 0xCC, 0xCC};
    auto result = GetParam()
                      ? ModbusClientHandler::InputProcessRequest(
                            request[0], request[1], bytes)
                      : ModbusClientHandler::HoldingProcessRequest(
                            request[0], request[1], bytes, Access::READ);
    EXPECT_EQ(result, Result::INVALID_REGISTER_ADDRESS);
    EXPECT_EQ((std::vector<uint8_t>(bytes, bytes + 4)),
              (std::vector<uint8_t>(4, 0xCC)));
  }
}

TEST_F(ModbusEMHandlerTests, WriteIsRejectedWithoutChangingMeterOrBuffer) {
  meter->setRvrBalancedEnergy(12345);
  uint8_t bytes[] = {0x11, 0x22};

  EXPECT_NE(ModbusClientHandler::HoldingProcessRequest(
                114, 1, bytes, Access::WRITE), Result::OK);
  EXPECT_EQ(meter->getRvrBalancedActEnergy(), 12345);
  EXPECT_EQ(bytes[0], 0x11);
  EXPECT_EQ(bytes[1], 0x22);
}

std::string registerFunctionName(const ::testing::TestParamInfo<bool> &info) {
  return info.param ? "Input" : "Holding";
}

INSTANTIATE_TEST_SUITE_P(RegisterFunctions,
                         ModbusEMHandlerTests,
                         ::testing::Bool(),
                         registerFunctionName);

}  // namespace

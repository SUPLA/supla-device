// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <gtest/gtest.h>
#include <SuplaDevice.h>
#include <supla-common/log.h>
#include <supla/protocol/supla_srpc.h>
#include <string>
#include <vector>

#if SUPLA_SRPC_PACKET_LOG_ENABLED && !defined(SUPLA_DISABLE_LOGS)

extern "C" const char *supla_test_get_last_log();
extern "C" void supla_test_clear_last_log();

namespace {

class SrpcPacketLogTests : public ::testing::Test {
 protected:
  void SetUp() override {
    oldLogLevel = supla_log_get_level();
    supla_log_set_level(LOG_DEBUG);
    srpc = new Supla::Protocol::SuplaSrpc(&sd);
    supla_test_clear_last_log();
  }

  void TearDown() override {
    delete srpc;
    srpc = nullptr;
    supla_log_set_level(oldLogLevel);
  }

  SuplaDeviceClass sd;
  Supla::Protocol::SuplaSrpc *srpc = nullptr;
  int oldLogLevel = LOG_VERBOSE;
};

}  // namespace

TEST_F(SrpcPacketLogTests, CalcfgPasswordRedactionLogsRealMetadata) {
  TSD_DeviceCalCfgRequest request = {};
  request.SenderID = 123;
  request.ChannelNumber = 4;
  request.Command = SUPLA_CALCFG_CMD_SET_CFG_MODE_PASSWORD;
  request.SuperUserAuthorized = 1;
  request.DataType = 222;
  request.DataSize = 5;
  memcpy(request.Data, "abcde", request.DataSize);

  srpc->logSrpcPacket(false,
                      SUPLA_SD_CALL_DEVICE_CALCFG_REQUEST,
                      reinterpret_cast<const uint8_t *>(&request),
                      offsetof(TSD_DeviceCalCfgRequest, Data) +
                          request.DataSize);
  std::string log = supla_test_get_last_log();

  EXPECT_NE(log.find("SuperUserAuthorized=1"), std::string::npos);
  EXPECT_NE(log.find("DataType=222"), std::string::npos);
  EXPECT_NE(log.find("DataSize=5"), std::string::npos);
  EXPECT_NE(log.find("Data=<redacted>"), std::string::npos);
  EXPECT_EQ(log.find("raw=["), std::string::npos);
}

TEST_F(SrpcPacketLogTests, CalcfgRejectsOversizedDataBeforeRawDump) {
  const size_t headerSize = offsetof(TSD_DeviceCalCfgRequest, Data);
  const size_t dataSize = SUPLA_CALCFG_DATA_MAXSIZE + 1;
  std::vector<uint8_t> packet(headerSize + dataSize, 0xAB);
  auto *request = reinterpret_cast<TSD_DeviceCalCfgRequest *>(packet.data());
  request->SenderID = 123;
  request->ChannelNumber = 4;
  request->Command = SUPLA_CALCFG_CMD_RESET_COUNTERS;
  request->SuperUserAuthorized = 1;
  request->DataType = 0;
  request->DataSize = dataSize;

  srpc->logSrpcPacket(false,
                      SUPLA_SD_CALL_DEVICE_CALCFG_REQUEST,
                      packet.data(),
                      packet.size());
  std::string log = supla_test_get_last_log();

  EXPECT_NE(log.find("Data=<invalid-size>"), std::string::npos);
  EXPECT_NE(log.find("DataSize=129"), std::string::npos);
  EXPECT_EQ(log.find("raw=["), std::string::npos);
}

TEST_F(SrpcPacketLogTests, LongRawDumpUsesContinuationLog) {
  constexpr size_t packetSize = 2100;
  std::vector<uint8_t> packet(packetSize);
  for (size_t i = 0; i < packet.size(); i++) {
    packet[i] = static_cast<uint8_t>(i % 251);
  }

  srpc->logSrpcPacket(false, 0x7FFF, packet.data(), packet.size());
  std::string log = supla_test_get_last_log();

  EXPECT_NE(log.find("SRPC raw-cont=["), std::string::npos);
  EXPECT_EQ(log.find("..."), std::string::npos);
}

TEST_F(SrpcPacketLogTests, RegisterDeviceHeaderUsesContinuationLog) {
  std::vector<uint8_t> storage(sizeof(TSuplaDataPacket));
  auto *packet = reinterpret_cast<TSuplaDataPacket *>(storage.data());
  auto *header = reinterpret_cast<TDS_SuplaRegisterDeviceHeader *>(
      packet->data);
  packet->version = 28;
  packet->call_id = SUPLA_DS_CALL_REGISTER_DEVICE_G;
  packet->data_size = sizeof(TDS_SuplaRegisterDeviceHeader) + 1;
  header->Flags = 0x1CCD0;
  header->ManufacturerID = 21;
  header->ProductID = 10;
  header->channel_count = 10;
  memcpy(header->Name, "Test Device", sizeof("Test Device") - 1);
  memcpy(header->SoftVer,
         "1.2.3-test",
         sizeof("1.2.3-test") - 1);
  memcpy(header->ServerName,
         "server.example",
         sizeof("server.example") - 1);

  const size_t packetSize = sizeof(TSuplaDataPacket) - SUPLA_MAX_DATA_SIZE +
                            sizeof(TDS_SuplaRegisterDeviceHeader);
  srpc->logSrpcPacket(true,
                      SUPLA_DS_CALL_REGISTER_DEVICE_G,
                      storage.data(),
                      packetSize);
  std::string log = supla_test_get_last_log();

  EXPECT_NE(log.find("SRPC cont=[ServerName=\"server.example\""),
            std::string::npos);
  EXPECT_NE(log.find("channel_count=10]"), std::string::npos);
}

#endif  // SUPLA_SRPC_PACKET_LOG_ENABLED && !SUPLA_DISABLE_LOGS

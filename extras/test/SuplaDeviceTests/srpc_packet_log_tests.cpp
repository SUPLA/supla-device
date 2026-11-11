// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <gtest/gtest.h>
#include <SuplaDevice.h>
#include <supla-common/log.h>
#include <supla/log_indent.h>
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

TEST_F(SrpcPacketLogTests, CompactConfigReplyKeepsDirectionCallIdAndSize) {
  TSDS_SetChannelConfigResult result = {};
  result.Result = SUPLA_CONFIG_RESULT_NOT_ALLOWED;
  result.ChannelNumber = 8;
  srpc->logSrpcPacket(false, SUPLA_SD_CALL_SET_CHANNEL_CONFIG_RESULT,
                      reinterpret_cast<const uint8_t *>(&result),
                      sizeof(result));
  EXPECT_STREQ(supla_test_get_last_log(),
               "SRPC RX SET_CH_CFG_RES(691) size=3 "
               "{Ch=8, CfgType=0(default), Res=6(NOT_ALLOWED)}");
  result.Result = SUPLA_CONFIG_RESULT_TRUE;
  srpc->logSrpcPacket(true, SUPLA_DS_CALL_SET_CHANNEL_CONFIG_RESULT,
                      reinterpret_cast<const uint8_t *>(&result),
                      sizeof(result));
  EXPECT_STREQ(supla_test_get_last_log(),
               "SRPC TX SET_CH_CFG_RES(692) size=3 "
               "{Ch=8, CfgType=0(default), Res=1(OK)}");
}

TEST_F(SrpcPacketLogTests, CompactDecodedPacketOmitsPayloadLabel) {
  supla_log_set_level(LOG_VERBOSE);
  TDS_SuplaDeviceChannel_E channel = {};
  channel.Number = 8;
  channel.Type = SUPLA_CHANNELTYPE_ACTIONTRIGGER;
  channel.ActionTriggerCaps = 0xC;
  channel.Default = SUPLA_CHANNELFNC_ACTIONTRIGGER;
  channel.ValueValidityTimeSec = 30;
  channel.DefaultIcon = 7;
  channel.SubDeviceId = 3;
  srpc->logSrpcPacket(true, SUPLA_DS_CALL_REGISTER_DEVICE_G,
                      reinterpret_cast<const uint8_t *>(&channel),
                      sizeof(channel));
  const std::string log = supla_test_get_last_log();
  const std::string prefix = "SRPC TX REG_DEV_G(76) size=" +
      std::to_string(sizeof(channel)) + " {Ch=8, ";
  EXPECT_EQ(log.find(prefix), 0U);
  EXPECT_EQ(log.find("call="), std::string::npos);
  EXPECT_EQ(log.find("payload="), std::string::npos);
  EXPECT_EQ(log.find(" s="), std::string::npos);
  EXPECT_NE(log.find("ATCaps=0xC, DefFunc=700"), std::string::npos);
  EXPECT_NE(log.find("ValidSec=30, Val[8]="), std::string::npos);
  EXPECT_NE(log.find("DefIcon=7, SubDevId=3}"), std::string::npos);
}

TEST_F(SrpcPacketLogTests, ShortChannelConfigFieldsKeepRedaction) {
  TSD_ChannelConfig config = {};
  config.ChannelNumber = 8;
  config.ConfigType = SUPLA_CONFIG_TYPE_DEFAULT;
  config.ConfigSize = 16;
  srpc->logSrpcPacket(false, SUPLA_SD_CALL_SET_CHANNEL_CONFIG,
                      reinterpret_cast<const uint8_t *>(&config),
                      offsetof(TSD_ChannelConfig, Config) + config.ConfigSize);
  const std::string log = supla_test_get_last_log();
  EXPECT_NE(log.find("{Ch=8, Func=0, CfgType=0(default), CfgSize=16, "
                     "Cfg=<redacted>}"), std::string::npos);
  EXPECT_EQ(log.find("raw=["), std::string::npos);
}

TEST_F(SrpcPacketLogTests, ChannelConfigRawPayloadRequiresVerboseLogging) {
  TSD_ChannelConfig config = {};
  config.ChannelNumber = 8;
  config.Func = SUPLA_CHANNELFNC_ACTIONTRIGGER;
  config.ConfigType = SUPLA_CONFIG_TYPE_DEFAULT;
  config.ConfigSize = sizeof(TChannelConfig_ActionTrigger);
  const auto size = offsetof(TSD_ChannelConfig, Config) + config.ConfigSize;
  srpc->logSrpcPacket(false, SUPLA_SD_CALL_SET_CHANNEL_CONFIG,
                      reinterpret_cast<const uint8_t *>(&config), size);
  std::string log = supla_test_get_last_log();
  EXPECT_NE(log.find("{Ch=8, Func=700, CfgType=0(default), CfgSize=16}"),
            std::string::npos);
  EXPECT_EQ(log.find("raw=["), std::string::npos);

  supla_log_set_level(LOG_VERBOSE);
  srpc->logSrpcPacket(false, SUPLA_SD_CALL_SET_CHANNEL_CONFIG,
                      reinterpret_cast<const uint8_t *>(&config), size);
  log = supla_test_get_last_log();
  EXPECT_EQ(log.find("  SRPC RX SET_CH_CFG(682) size=16 raw=["), 0U);
  EXPECT_EQ(Supla::LogIndent::spaces(), 0);
}

TEST_F(SrpcPacketLogTests, ShortFunctionFieldsDistinguishRelayAndRgbw) {
  supla_log_set_level(LOG_VERBOSE);
  TDS_SuplaDeviceChannel_E channel = {};
  channel.Type = SUPLA_CHANNELTYPE_RELAY;
  channel.FuncList = 0x12;
  srpc->logSrpcPacket(true, SUPLA_DS_CALL_REGISTER_DEVICE_G,
                      reinterpret_cast<const uint8_t *>(&channel),
                      sizeof(channel));
  EXPECT_NE(std::string(supla_test_get_last_log()).find("Funcs=0x12"),
            std::string::npos);
  channel.Type = SUPLA_CHANNELTYPE_DIMMER;
  channel.RGBW_FuncList = 0x34;
  srpc->logSrpcPacket(true, SUPLA_DS_CALL_REGISTER_DEVICE_G,
                      reinterpret_cast<const uint8_t *>(&channel),
                      sizeof(channel));
  EXPECT_NE(std::string(supla_test_get_last_log()).find("RGBWFuncs=0x34"),
            std::string::npos);
}

TEST_F(SrpcPacketLogTests, CompactPingKeepsMetadataWithoutPayload) {
  supla_log_set_level(LOG_VERBOSE);
  srpc->logSrpcPacket(true, SUPLA_DCS_CALL_PING_SERVER, nullptr, 0);
  const std::string expected = "SRPC TX PING(" +
      std::to_string(SUPLA_DCS_CALL_PING_SERVER) + ") size=0";
  EXPECT_STREQ(supla_test_get_last_log(), expected.c_str());
}

TEST_F(SrpcPacketLogTests, RegistrationChannelDetailsRequireVerboseLogging) {
  TDS_SuplaDeviceChannel_E channel = {};
  channel.Type = SUPLA_CHANNELTYPE_ACTIONTRIGGER;
  srpc->logSrpcPacket(true, SUPLA_DS_CALL_REGISTER_DEVICE_G,
                      reinterpret_cast<const uint8_t *>(&channel),
                      sizeof(channel));
  EXPECT_STREQ(supla_test_get_last_log(), "");
  supla_log_set_level(LOG_VERBOSE);
  srpc->logSrpcPacket(true, SUPLA_DS_CALL_REGISTER_DEVICE_G,
                      reinterpret_cast<const uint8_t *>(&channel),
                      sizeof(channel));
  EXPECT_NE(std::string(supla_test_get_last_log()).find("ATCaps="),
            std::string::npos);
  EXPECT_EQ(Supla::LogIndent::spaces(), 0);
}

TEST_F(SrpcPacketLogTests, CompactRegistrationStillRedactsSensitiveData) {
  const char secret[] = "private-data";
  srpc->logSrpcPacket(true, SUPLA_DS_CALL_REGISTER_DEVICE,
                      reinterpret_cast<const uint8_t *>(secret),
                      sizeof(secret) - 1);
  const std::string log = supla_test_get_last_log();
  EXPECT_EQ(log.find("SRPC TX REG_DEV("), 0U);
  EXPECT_NE(log.find(" size=12 {<redacted>}"), std::string::npos);
  EXPECT_EQ(log.find(secret), std::string::npos);
  EXPECT_EQ(log.find("raw=["), std::string::npos);
}

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

  EXPECT_EQ(log.find("SRPC RX DEV_CALCFG_REQ("), 0U);
  EXPECT_NE(log.find("SuperAuth=1"), std::string::npos);
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

  EXPECT_NE(log.find("SRPC cont=[Server=\"server.example\""),
            std::string::npos);
  EXPECT_NE(log.find("MfrId=21, ProdId=10"), std::string::npos);
  EXPECT_NE(log.find("ChCount=10]"), std::string::npos);
}

#endif  // SUPLA_SRPC_PACKET_LOG_ENABLED && !SUPLA_DISABLE_LOGS

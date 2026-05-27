/*
 Copyright (C) AC SOFTWARE SP. Z O.O.

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <gtest/gtest.h>
#include <SuplaDevice.h>
#include <supla-common/log.h>
#include <supla/protocol/supla_srpc.h>
#include <string>
#include <vector>

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

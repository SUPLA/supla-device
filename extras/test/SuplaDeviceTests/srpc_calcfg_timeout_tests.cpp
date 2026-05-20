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

#include <SuplaDevice.h>
#include <arduino_mock.h>
#include <channel_element_mock.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <network_client_mock.h>
#include <simple_time.h>
#include <srpc_mock.h>
#include <supla/clock/clock.h>
#include <supla/protocol/supla_srpc.h>

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::NiceMock;
using ::testing::Return;

class SrpcLayerTestStub : public Supla::Protocol::SuplaSrpc {
 public:
  explicit SrpcLayerTestStub(SuplaDeviceClass *sdc)
      : Supla::Protocol::SuplaSrpc(sdc) {
  }

  using Supla::Protocol::SuplaSrpc::deinitializeSrpc;
  using Supla::Protocol::SuplaSrpc::initializeSrpc;

  void setRegisteredAndReady() {
    registered = 1;
  }
};

class SrpcCalcfgTimeoutTests : public ::testing::Test {
 protected:
  SimpleTime time;
  SrpcMock srpc;
  SuplaDeviceClass sd;
  NiceMock<ChannelElementMock> element{0};
  NetworkClientMock *client = nullptr;
  SrpcLayerTestStub *srpcLayer = nullptr;

  void SetUp() override {
    if (SuplaDevice.getClock()) {
      delete SuplaDevice.getClock();
    }
    Supla::Channel::resetToDefaults();

    client = new NetworkClientMock;
    srpcLayer = new SrpcLayerTestStub(&sd);
    srpcLayer->setVersion(28);
    srpcLayer->setNetworkClient(client);
    srpcLayer->setRegisteredAndReady();

    EXPECT_CALL(srpc, srpc_params_init(_)).Times(1);
    EXPECT_CALL(srpc, srpc_free(_)).Times(AnyNumber());
    EXPECT_CALL(srpc, srpc_rd_free(_)).Times(AnyNumber());
    int dummy = 0;
    EXPECT_CALL(srpc, srpc_init(_)).WillOnce(Return(&dummy));
    EXPECT_CALL(srpc, srpc_set_proto_version(&dummy, 28)).Times(1);

    srpcLayer->initializeSrpc();
  }

  void TearDown() override {
    if (srpcLayer) {
      srpcLayer->deinitializeSrpc();
      delete srpcLayer;
      srpcLayer = nullptr;
    }
    Supla::Channel::resetToDefaults();
  }
};

TEST_F(SrpcCalcfgTimeoutTests, PendingCalcfgTimesOutAndSendsFalse) {
  TSD_DeviceCalCfgRequest request = {};
  request.ChannelNumber = 0;
  request.SenderID = 1234;
  request.SuperUserAuthorized = 1;
  request.Command = SUPLA_CALCFG_CMD_IDENTIFY_SUBDEVICE;

  EXPECT_CALL(element, handleCalcfgFromServer(_))
      .WillOnce(Return(SUPLA_SRPC_CALCFG_RESULT_PENDING));
  EXPECT_CALL(element, getCalcfgPendingTimeoutMs(_)).WillOnce(Return(5000));
  EXPECT_CALL(srpc, srpc_getdata(_, _, _))
      .WillOnce(
          [&request](void *, TsrpcReceivedData *rd, unsigned _supla_int_t) {
            memset(rd, 0, sizeof(TsrpcReceivedData));
            rd->call_id = SUPLA_SD_CALL_DEVICE_CALCFG_REQUEST;
            rd->data.sd_device_calcfg_request = &request;
            return SUPLA_RESULT_TRUE;
          });

  Supla::messageReceived(nullptr, 0, 0, srpcLayer, 28);
  ASSERT_NE(srpcLayer->calCfgResultPending.get(0, request.Command), nullptr);

  time.advance(5001);
  EXPECT_CALL(srpc, srpc_ds_async_device_calcfg_result(_, _))
      .WillOnce([](void *, TDS_DeviceCalCfgResult *result) {
        EXPECT_EQ(result->ChannelNumber, 0);
        EXPECT_EQ(result->ReceiverID, 1234);
        EXPECT_EQ(result->Command, SUPLA_CALCFG_CMD_IDENTIFY_SUBDEVICE);
        EXPECT_EQ(result->Result, SUPLA_CALCFG_RESULT_FALSE);
        EXPECT_EQ(result->DataSize, 0);
        return 0;
      });

  srpcLayer->iterate(time.value);
  EXPECT_EQ(srpcLayer->calCfgResultPending.get(0, request.Command), nullptr);
}

TEST_F(SrpcCalcfgTimeoutTests, DisconnectClearsPendingCalcfg) {
  srpcLayer->calCfgResultPending.set(
      0, 1234, SUPLA_CALCFG_CMD_IDENTIFY_SUBDEVICE, 5000);
  ASSERT_NE(srpcLayer->calCfgResultPending.get(
                0, SUPLA_CALCFG_CMD_IDENTIFY_SUBDEVICE),
            nullptr);

  EXPECT_CALL(*client, stop()).Times(1);
  srpcLayer->disconnect();

  EXPECT_EQ(srpcLayer->calCfgResultPending.get(
                0, SUPLA_CALCFG_CMD_IDENTIFY_SUBDEVICE),
            nullptr);
}

TEST_F(SrpcCalcfgTimeoutTests, ClearPendingCalcfgTimeoutDisablesExpiry) {
  srpcLayer->calCfgResultPending.set(
      0, 1234, SUPLA_CALCFG_CMD_IDENTIFY_SUBDEVICE, 5000);
  ASSERT_NE(srpcLayer->calCfgResultPending.get(
                0, SUPLA_CALCFG_CMD_IDENTIFY_SUBDEVICE),
            nullptr);

  srpcLayer->clearPendingCalCfgTimeout(0, SUPLA_CALCFG_CMD_IDENTIFY_SUBDEVICE);

  time.advance(6000);
  EXPECT_CALL(srpc, srpc_ds_async_device_calcfg_result(_, _)).Times(0);

  srpcLayer->iterate(time.value);
  ASSERT_NE(srpcLayer->calCfgResultPending.get(
                0, SUPLA_CALCFG_CMD_IDENTIFY_SUBDEVICE),
            nullptr);
}

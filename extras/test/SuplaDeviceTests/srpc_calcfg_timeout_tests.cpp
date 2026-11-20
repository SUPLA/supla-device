// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <SuplaDevice.h>
#include <arduino_mock.h>
#include <channel_element_mock.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <network_client_mock.h>
#include <simple_time.h>
#include <srpc_mock.h>
#include <supla/clock/clock.h>
#include <supla/device/subdevice_pairing_handler.h>
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
  int srpcHandle = 0;

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

    EXPECT_CALL(srpc, srpc_free(_)).Times(AnyNumber());
    EXPECT_CALL(srpc, srpc_rd_free(_)).Times(AnyNumber());
    initializeSrpc();
  }

  void initializeSrpc() {
    EXPECT_CALL(srpc, srpc_params_init(_)).Times(1);
    EXPECT_CALL(srpc, srpc_init(_)).WillOnce(Return(&srpcHandle));
    EXPECT_CALL(srpc, srpc_set_proto_version(&srpcHandle, 28)).Times(1);
    srpcLayer->initializeSrpc();
  }

  void TearDown() override {
    if (srpcLayer) {
      srpcLayer->deinitializeSrpc();
      client->expectStopOnDestruction();
      delete srpcLayer;
      srpcLayer = nullptr;
    }
    Supla::Channel::resetToDefaults();
  }
};

namespace {

class PairingHandlerMock : public Supla::Device::SubdevicePairingHandler {
 public:
  MOCK_METHOD(bool,
              startPairing,
              (Supla::Protocol::SuplaSrpc *, TCalCfg_SubdevicePairingResult *),
              (override));
};

class SrpcPairingTests : public SrpcCalcfgTimeoutTests {
 protected:
  static constexpr int kPairingCommand =
      SUPLA_CALCFG_CMD_START_SUBDEVICE_PAIRING;
  PairingHandlerMock pairing;

  void SetUp() override {
    SrpcCalcfgTimeoutTests::SetUp();
    sd.setSubdevicePairingHandler(&pairing);
    sd.addFlags(SUPLA_DEVICE_FLAG_CALCFG_SUBDEVICE_PAIRING);
  }

  void TearDown() override {
    sd.removeFlags(SUPLA_DEVICE_FLAG_CALCFG_SUBDEVICE_PAIRING);
    SrpcCalcfgTimeoutTests::TearDown();
  }

  void requestPairing(int senderId, unsigned char status) {
    TSD_DeviceCalCfgRequest request = {};
    request.ChannelNumber = -1;
    request.SenderID = senderId;
    request.SuperUserAuthorized = 1;
    request.Command = kPairingCommand;

    EXPECT_CALL(pairing, startPairing(_, _))
        .WillOnce([status](auto *, TCalCfg_SubdevicePairingResult *result) {
          result->PairingResult = status;
          return status == SUPLA_CALCFG_PAIRINGRESULT_PROCEDURE_STARTED;
        });
    EXPECT_CALL(srpc, srpc_getdata(_, _, _))
        .WillOnce([&request](void *, TsrpcReceivedData *rd, unsigned int) {
          *rd = {};
          rd->call_id = SUPLA_SD_CALL_DEVICE_CALCFG_REQUEST;
          rd->data.sd_device_calcfg_request = &request;
          return SUPLA_RESULT_TRUE;
        });
    EXPECT_CALL(srpc, srpc_ds_async_device_calcfg_result(_, _))
        .WillOnce([senderId, status](void *, TDS_DeviceCalCfgResult *result) {
          EXPECT_EQ(result->ReceiverID, senderId);
          EXPECT_EQ(result->Command, kPairingCommand);
          EXPECT_EQ(result->Result, SUPLA_CALCFG_RESULT_TRUE);
          EXPECT_EQ(result->DataSize, sizeof(TCalCfg_SubdevicePairingResult));
          TCalCfg_SubdevicePairingResult pairingResult = {};
          memcpy(&pairingResult, result->Data, sizeof(pairingResult));
          EXPECT_EQ(pairingResult.PairingResult, status);
          return 0;
        });
    Supla::messageReceived(&srpcHandle, 0, 0, srpcLayer, 28);
  }

  void expectCompletionReceiver(int receiverId) {
    EXPECT_CALL(srpc, srpc_ds_async_device_calcfg_result(_, _))
        .WillOnce([receiverId](void *, TDS_DeviceCalCfgResult *result) {
          EXPECT_EQ(result->ReceiverID, receiverId);
          EXPECT_EQ(result->Command, kPairingCommand);
          return 0;
        });
    srpcLayer->sendPendingCalCfgResult(
        -1, SUPLA_CALCFG_RESULT_TRUE, kPairingCommand);
  }
};

TEST_F(SrpcPairingTests, OngoingReplyPreservesOriginalCompletionReceiver) {
  requestPairing(1234, SUPLA_CALCFG_PAIRINGRESULT_PROCEDURE_STARTED);
  requestPairing(5678, SUPLA_CALCFG_PAIRINGRESULT_ONGOING);
  expectCompletionReceiver(1234);
}

TEST_F(SrpcPairingTests, OngoingReplyRestoresTrackingAfterReconnect) {
  requestPairing(1234, SUPLA_CALCFG_PAIRINGRESULT_PROCEDURE_STARTED);
  EXPECT_CALL(*client, stop()).Times(1);
  srpcLayer->disconnect();
  EXPECT_TRUE(srpcLayer->calCfgResultPending.empty());
  initializeSrpc();

  requestPairing(5678, SUPLA_CALCFG_PAIRINGRESULT_ONGOING);
  expectCompletionReceiver(5678);
}

TEST_F(SrpcPairingTests, RefusedPairingDoesNotRegisterCompletion) {
  requestPairing(1234, SUPLA_CALCFG_PAIRINGRESULT_NOT_STARTED_BUSY);
  EXPECT_TRUE(srpcLayer->calCfgResultPending.empty());

  requestPairing(5678, SUPLA_CALCFG_PAIRINGRESULT_NOT_STARTED_NOT_READY);
  EXPECT_TRUE(srpcLayer->calCfgResultPending.empty());
}

}  // namespace

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

// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <SuplaDevice.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <network_client_mock.h>
#include <simple_time.h>
#include <srpc_mock.h>
#include <supla-common/log.h>
#include <supla/device/register_device.h>
#include <supla/protocol/supla_srpc.h>

#include <array>
#include <cstdint>

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

extern "C" const char *supla_test_get_last_log();
extern "C" void supla_test_clear_last_log();

namespace {

class WriteFailureTestSrpc : public Supla::Protocol::SuplaSrpc {
 public:
  explicit WriteFailureTestSrpc(SuplaDeviceClass *sdc)
      : Supla::Protocol::SuplaSrpc(sdc) {
  }

  using Supla::Protocol::SuplaSrpc::deinitializeSrpc;
  using Supla::Protocol::SuplaSrpc::initializeSrpc;

  uint32_t waitForIterateForTest() const {
    return waitForIterate;
  }

  void scheduleReconnectForTest(uint32_t now) {
    scheduleReconnect(now);
  }
};

class SrpcWriteFailureTests : public ::testing::Test {
 protected:
  SimpleTime time;
  int oldLogLevel = LOG_VERBOSE;

  void SetUp() override {
    Supla::RegisterDevice::resetToDefaults();
    oldLogLevel = supla_log_get_level();
    supla_log_set_level(LOG_DEBUG);
    supla_test_clear_last_log();
  }

  void TearDown() override {
    supla_log_set_level(oldLogLevel);
    Supla::RegisterDevice::resetToDefaults();
  }
};

}  // namespace

TEST_F(SrpcWriteFailureTests, FullWriteDoesNotMarkFailure) {
  SuplaDeviceClass device;
  WriteFailureTestSrpc protocol(&device);
  auto *client = new NiceMock<NetworkClientMock>;
  protocol.setNetworkClient(client);
  std::array<uint8_t, 4> buffer = {};

  EXPECT_CALL(*client, connected()).WillOnce(Return(1));
  EXPECT_CALL(*client, writeImp(_, buffer.size()))
      .WillOnce(Return(buffer.size()));

  EXPECT_EQ(Supla::dataWrite(buffer.data(), buffer.size(), &protocol),
            buffer.size());
  EXPECT_FALSE(protocol.hasWriteFailure());
}

TEST_F(SrpcWriteFailureTests, ClosedConnectionMarksFailureWithoutWriting) {
  SuplaDeviceClass device;
  WriteFailureTestSrpc protocol(&device);
  auto *client = new NiceMock<NetworkClientMock>;
  protocol.setNetworkClient(client);
  std::array<uint8_t, 4> buffer = {};

  EXPECT_CALL(*client, connected()).WillOnce(Return(0));
  EXPECT_CALL(*client, writeImp(_, _)).Times(0);
  EXPECT_CALL(*client, stop()).Times(0);

  EXPECT_EQ(Supla::dataWrite(buffer.data(), buffer.size(), &protocol), 0);
  EXPECT_TRUE(protocol.hasWriteFailure());
}

TEST_F(SrpcWriteFailureTests, PartialWriteStopsClientAndLatchesFailure) {
  SuplaDeviceClass device;
  WriteFailureTestSrpc protocol(&device);
  auto *client = new NiceMock<NetworkClientMock>;
  protocol.setNetworkClient(client);
  std::array<uint8_t, 4> buffer = {};

  EXPECT_CALL(*client, connected()).Times(1).WillOnce(Return(1));
  EXPECT_CALL(*client, writeImp(_, buffer.size()))
      .Times(1)
      .WillOnce(Return(buffer.size() - 1));
  EXPECT_CALL(*client, stop()).Times(1);

  EXPECT_EQ(Supla::dataWrite(buffer.data(), buffer.size(), &protocol),
            buffer.size() - 1);
  EXPECT_TRUE(protocol.hasWriteFailure());

  EXPECT_EQ(Supla::dataWrite(buffer.data(), buffer.size(), &protocol), 0);
}

TEST_F(SrpcWriteFailureTests, PacketSentUsesCallbackUserParamsForFailureState) {
  SuplaDeviceClass srpcHandleDevice;
  WriteFailureTestSrpc srpcHandle(&srpcHandleDevice);
  SuplaDeviceClass device;
  WriteFailureTestSrpc protocol(&device);
  protocol.markWriteFailure();

  Supla::Protocol::SuplaSrpc::onPacketSent(
      &srpcHandle, SUPLA_DCS_CALL_GETVERSION, nullptr, 0, &protocol);

  EXPECT_STREQ(supla_test_get_last_log(), "");
}

TEST_F(SrpcWriteFailureTests,
       IterateDisconnectsAndSchedulesReconnectAfterWriteFailure) {
  SrpcMock srpcMock;
  SuplaDeviceClass device;
  WriteFailureTestSrpc protocol(&device);
  auto *client = new NiceMock<NetworkClientMock>;
  int dummy = 0;

  Supla::RegisterDevice::setServerName("supla.example");
  Supla::RegisterDevice::setEmail("user@example.com");
  protocol.setNetworkClient(client);

  EXPECT_CALL(srpcMock, srpc_params_init(_));
  EXPECT_CALL(srpcMock, srpc_init(_)).WillOnce(Return(&dummy));
  EXPECT_CALL(srpcMock, srpc_set_proto_version(&dummy, 23));
  EXPECT_CALL(srpcMock, srpc_iterate(&dummy))
      .WillOnce(Return(SUPLA_RESULT_TRUE));
  EXPECT_CALL(srpcMock, srpc_free(&dummy));
  EXPECT_CALL(*client, connected()).WillOnce(Return(1));
  EXPECT_CALL(*client, stop()).Times(1);

  protocol.initializeSrpc();
  protocol.markWriteFailure();

  EXPECT_FALSE(protocol.iterate(1000));
  EXPECT_EQ(protocol.waitForIterateForTest(), 1000U);
  protocol.deinitializeSrpc();
}

TEST_F(SrpcWriteFailureTests, ReconnectDelayIncreasesAndIsCappedAtOneMinute) {
  SuplaDeviceClass device;
  WriteFailureTestSrpc protocol(&device);
  const uint32_t expectedDelays[] = {
      1000, 5000, 5000, 15000, 15000, 15000, 60000, 60000};

  for (uint32_t delay : expectedDelays) {
    protocol.scheduleReconnectForTest(100);
    EXPECT_EQ(protocol.waitForIterateForTest(), delay);
  }
}

TEST_F(SrpcWriteFailureTests, SuccessfulRegistrationResetsReconnectDelay) {
  SuplaDeviceClass device;
  WriteFailureTestSrpc protocol(&device);
  TSD_SuplaRegisterDeviceResult result = {};
  result.result_code = SUPLA_RESULTCODE_TRUE;
  result.activity_timeout = 30;

  protocol.scheduleReconnectForTest(100);
  protocol.scheduleReconnectForTest(200);
  EXPECT_EQ(protocol.waitForIterateForTest(), 5000U);

  protocol.onRegisterResult(&result);
  protocol.scheduleReconnectForTest(300);

  EXPECT_EQ(protocol.waitForIterateForTest(), 1000U);
}

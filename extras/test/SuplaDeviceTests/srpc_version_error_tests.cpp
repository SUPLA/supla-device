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

#include <stdint.h>
#include <cstring>

#include <SuplaDevice.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <network_client_mock.h>
#include <simple_time.h>
#include <srpc_mock.h>
#include <supla/device/register_device.h>
#include <supla/protocol/supla_srpc.h>

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Invoke;
using ::testing::Return;

namespace {

class VersionErrorTestSrpc : public Supla::Protocol::SuplaSrpc {
 public:
  explicit VersionErrorTestSrpc(SuplaDeviceClass *sdc)
      : Supla::Protocol::SuplaSrpc(sdc) {
  }

  void initializeSrpcForTest() {
    initializeSrpc();
  }

  void deinitializeSrpcForTest() {
    deinitializeSrpc();
  }

  void setRegisteredForTest(int8_t value) {
    registered = value;
  }

  uint32_t lastIterateTimeForTest() const {
    return lastIterateTime;
  }

  uint32_t waitForIterateForTest() const {
    return waitForIterate;
  }
};

class SuplaSrpcVersionErrorTests : public ::testing::Test {
 protected:
  SuplaSrpcVersionErrorTests() : srpcLayer(&device) {
  }

  void SetUp() override {
    time.value = 0;
    client = new NetworkClientMock;
    Supla::RegisterDevice::resetToDefaults();
    Supla::RegisterDevice::setServerName("supla.example");
    Supla::RegisterDevice::setEmail("user@example.com");

    EXPECT_CALL(srpc, srpc_params_init(_));
    EXPECT_CALL(srpc, srpc_init(_)).WillOnce(Return(&dummy));
    EXPECT_CALL(srpc, srpc_set_proto_version(&dummy, 23));
    EXPECT_CALL(srpc, srpc_free(_)).Times(AnyNumber());
    EXPECT_CALL(srpc, srpc_rd_free(_)).Times(AnyNumber());
    EXPECT_CALL(*client, connected()).WillRepeatedly(Return(1));

    srpcLayer.setNetworkClient(client);
    srpcLayer.initializeSrpcForTest();
  }

  void TearDown() override {
    srpcLayer.deinitializeSrpcForTest();
    Supla::RegisterDevice::resetToDefaults();
  }

  SimpleTime time;
  SrpcMock srpc;
  SuplaDeviceClass device;
  VersionErrorTestSrpc srpcLayer;
  NetworkClientMock *client = nullptr;
  int dummy = 0;
};

}  // namespace

TEST_F(SuplaSrpcVersionErrorTests,
       VersionErrorDefersFreeUntilReceiveCallbackReturns) {
  TSDC_SuplaVersionError versionError = {};
  bool callbackActive = false;
  bool iterateReturned = false;
  int freeCalls = 0;
  int freeWhileCallbackActive = 0;
  int freeBeforeIterateReturned = 0;

  EXPECT_CALL(srpc, srpc_free(_))
      .Times(1)
      .WillOnce([&](void *) {
        freeCalls++;
        if (callbackActive) {
          freeWhileCallbackActive++;
        }
        if (!iterateReturned) {
          freeBeforeIterateReturned++;
        }
      });
  EXPECT_CALL(srpc, srpc_getdata(_, _, _))
      .WillOnce([&](void *,
                    TsrpcReceivedData *rd,
                    unsigned _supla_int_t) {
        memset(rd, 0, sizeof(TsrpcReceivedData));
        rd->call_id = SUPLA_SDC_CALL_VERSIONERROR;
        rd->data.sdc_version_error = &versionError;
        return SUPLA_RESULT_TRUE;
      });
  EXPECT_CALL(srpc, srpc_rd_free(_)).Times(1);
  EXPECT_CALL(srpc, srpc_iterate(_))
      .WillOnce(Invoke([&](void *srpcHandle) {
        callbackActive = true;
        Supla::messageReceived(srpcHandle, 0, 0, &srpcLayer, 23);
        EXPECT_EQ(freeCalls, 0);
        callbackActive = false;
        iterateReturned = true;
        return static_cast<char>(SUPLA_RESULT_TRUE);
      }));
  EXPECT_CALL(*client, stop()).Times(1);

  time.advance(1000);
  EXPECT_FALSE(srpcLayer.iterate(time.value));

  EXPECT_FALSE(callbackActive);
  EXPECT_EQ(freeCalls, 1);
  EXPECT_EQ(freeWhileCallbackActive, 0);
  EXPECT_EQ(freeBeforeIterateReturned, 0);
  EXPECT_EQ(device.getCurrentStatus(), STATUS_PROTOCOL_VERSION_ERROR);
  EXPECT_EQ(srpcLayer.lastIterateTimeForTest(), time.value);
  EXPECT_EQ(srpcLayer.waitForIterateForTest(), 15000U);

  EXPECT_FALSE(srpcLayer.iterate(time.value + 5000));
  EXPECT_EQ(freeCalls, 1);
}

TEST_F(SuplaSrpcVersionErrorTests, RegistrationRejectionKeepsItsRetryDelay) {
  srpcLayer.setRegisteredForTest(2);

  EXPECT_CALL(srpc, srpc_iterate(_)).WillOnce(Return(SUPLA_RESULT_TRUE));
  EXPECT_CALL(*client, stop()).Times(1);
  EXPECT_CALL(srpc, srpc_free(_)).Times(1);

  time.advance(1000);
  EXPECT_FALSE(srpcLayer.iterate(time.value));
  EXPECT_EQ(srpcLayer.waitForIterateForTest(), 10000U);
}

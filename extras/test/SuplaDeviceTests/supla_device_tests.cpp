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
#include <clock_mock.h>
#include <element_mock.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <network_client_mock.h>
#include <network_with_mac_mock.h>
#include <srpc_mock.h>
#include <supla/clock/clock.h>
#include <supla/protocol/supla_srpc.h>
#include <supla/storage/storage.h>
#include <timer_mock.h>
#include <storage_mock.h>
#include <string.h>

using ::testing::_;
using ::testing::Return;

class SuplaDeviceTests : public ::testing::Test {
 protected:
  virtual void SetUp() {
    Supla::Channel::lastCommunicationTimeMs = 0;
    memset(&(Supla::Channel::reg_dev), 0, sizeof(Supla::Channel::reg_dev));
  }
  virtual void TearDown() {
    Supla::Channel::lastCommunicationTimeMs = 0;
    memset(&(Supla::Channel::reg_dev), 0, sizeof(Supla::Channel::reg_dev));
  }
};

class TimeInterfaceStub : public TimeInterface {
 public:
  virtual uint32_t millis() override {
    static uint32_t value = 0;
    value += 1000;
    return value;
  }
};

TEST_F(SuplaDeviceTests, DefaultValuesTest) {
  SuplaDeviceClass sd;
  SrpcMock srpc;
  TimerMock timer;

  EXPECT_EQ(sd.getCurrentStatus(), STATUS_UNKNOWN);
  EXPECT_EQ(sd.getClock(), nullptr);
}

TEST_F(SuplaDeviceTests, ClockMethods) {
  NetworkClientMock *client = new NetworkClientMock;
  SuplaDeviceClass sd;
  Supla::Protocol::SuplaSrpc srpcLayer(&sd);

  ASSERT_EQ(sd.getClock(), nullptr);
  srpcLayer.onGetUserLocaltimeResult(nullptr);

  ClockMock clock;
  ASSERT_EQ(sd.getClock(), &clock);

  EXPECT_CALL(clock, parseLocaltimeFromServer(nullptr)).Times(1);

  srpcLayer.onGetUserLocaltimeResult(nullptr);
}

TEST_F(SuplaDeviceTests, StartWithoutNetworkInterfaceNoElements) {
  NetworkClientMock *client = new NetworkClientMock;
  SuplaDeviceClass sd;
  TimerMock timer;

  ASSERT_EQ(Supla::Element::begin(), nullptr);
  EXPECT_CALL(timer, initTimers());

  EXPECT_FALSE(sd.begin());
  EXPECT_EQ(sd.getCurrentStatus(), STATUS_MISSING_NETWORK_INTERFACE);
}

TEST_F(SuplaDeviceTests, StartWithoutNetworkInterfaceNoElementsWithStorage) {
  NetworkClientMock *client = new NetworkClientMock;
  SuplaDeviceClass sd;
  TimerMock timer;
  StorageMockSimulator storage;

  ASSERT_EQ(Supla::Element::begin(), nullptr);
  EXPECT_CALL(storage, commit()).Times(1);

  ::testing::InSequence seq;
  EXPECT_CALL(timer, initTimers());

  EXPECT_TRUE(storage.isEmpty());

  EXPECT_FALSE(sd.begin());
  EXPECT_EQ(sd.getCurrentStatus(), STATUS_MISSING_NETWORK_INTERFACE);

  EXPECT_TRUE(storage.isPreampleInitialized());
}

TEST_F(SuplaDeviceTests,
       StartWithoutNetworkInterfaceNoElementsWithStorageAndDataLoadAttempt) {
  NetworkClientMock *client = new NetworkClientMock;
  SuplaDeviceClass sd;
  TimerMock timer;
  StorageMockSimulator storage;
  EXPECT_CALL(storage, commit()).Times(1);

  ::testing::InSequence seq;
  ASSERT_EQ(Supla::Element::begin(), nullptr);
  EXPECT_CALL(timer, initTimers());

  EXPECT_TRUE(storage.isEmpty());
  EXPECT_FALSE(sd.begin());
  EXPECT_EQ(sd.getCurrentStatus(), STATUS_MISSING_NETWORK_INTERFACE);
  EXPECT_TRUE(storage.isPreampleInitialized());
}

TEST_F(SuplaDeviceTests, StartWithoutNetworkInterfaceWithElements) {
  ::testing::InSequence seq;
  NetworkClientMock *client = new NetworkClientMock;
  SuplaDeviceClass sd;
  TimerMock timer;
  ElementMock el1;
  ElementMock el2;

  ASSERT_NE(Supla::Element::begin(), nullptr);

  EXPECT_CALL(el1, onInit());
  EXPECT_CALL(el2, onInit());

  EXPECT_CALL(timer, initTimers());
  EXPECT_CALL(el1, onTimer());
  EXPECT_CALL(el2, onTimer());
  EXPECT_CALL(el1, onFastTimer());
  EXPECT_CALL(el2, onFastTimer());

  EXPECT_FALSE(sd.begin());
  EXPECT_EQ(sd.getCurrentStatus(), STATUS_MISSING_NETWORK_INTERFACE);
  sd.onTimer();
  sd.onFastTimer();
}

TEST_F(SuplaDeviceTests, StartWithoutNetworkInterfaceWithElementsWithStorage) {
  NetworkClientMock *client = new NetworkClientMock;
  StorageMockSimulator storage;
  EXPECT_CALL(storage, commit()).Times(1);
  SuplaDeviceClass sd;
  TimerMock timer;
  ElementMock el1;
  ElementMock el2;

  ::testing::InSequence seq;
  ASSERT_NE(Supla::Element::begin(), nullptr);

  EXPECT_CALL(el1, onSaveState());
  EXPECT_CALL(el2, onSaveState());

  EXPECT_CALL(el1, onLoadState());
  EXPECT_CALL(el2, onLoadState());

  EXPECT_CALL(el1, onInit());
  EXPECT_CALL(el2, onInit());

  EXPECT_CALL(timer, initTimers());

  EXPECT_TRUE(storage.isEmpty());
  EXPECT_FALSE(sd.begin());
  EXPECT_EQ(sd.getCurrentStatus(), STATUS_MISSING_NETWORK_INTERFACE);
  EXPECT_TRUE(storage.isPreampleInitialized());
}

TEST_F(SuplaDeviceTests, BeginStopsAtEmptyGUID) {
  ::testing::InSequence seq;
  NetworkMockWithMac net;
  TimerMock timer;

  NetworkClientMock *client = new NetworkClientMock;
  SuplaDeviceClass sd;

  EXPECT_CALL(timer, initTimers());

  EXPECT_FALSE(sd.begin());
  EXPECT_EQ(sd.getCurrentStatus(), STATUS_INVALID_GUID);
}

TEST_F(SuplaDeviceTests, BeginStopsAtEmptyAuthkey) {
  ::testing::InSequence seq;
  NetworkMockWithMac net;
  TimerMock timer;

  NetworkClientMock *client = new NetworkClientMock;
  SuplaDeviceClass sd;

  EXPECT_CALL(timer, initTimers());

  char GUID[SUPLA_GUID_SIZE] = {1};
  sd.setGUID(GUID);
  EXPECT_FALSE(sd.begin());
  EXPECT_EQ(sd.getCurrentStatus(), STATUS_INVALID_AUTHKEY);
}

TEST_F(SuplaDeviceTests, BeginStopsAtEmptyServer) {
  ::testing::InSequence seq;
  NetworkMockWithMac net;
  TimerMock timer;

  NetworkClientMock *client = new NetworkClientMock;
  SuplaDeviceClass sd;

  EXPECT_CALL(timer, initTimers());

  char GUID[SUPLA_GUID_SIZE] = {1};
  char AUTHKEY[SUPLA_AUTHKEY_SIZE] = {2};
  sd.setGUID(GUID);
  sd.setAuthKey(AUTHKEY);
  sd.setEmail("john@supla");
  EXPECT_FALSE(sd.begin());
  EXPECT_EQ(sd.getCurrentStatus(), STATUS_UNKNOWN_SERVER_ADDRESS);
}

TEST_F(SuplaDeviceTests, BeginStopsAtEmptyEmail) {
  ::testing::InSequence seq;
  NetworkMockWithMac net;
  TimerMock timer;

  NetworkClientMock *client = new NetworkClientMock;
  SuplaDeviceClass sd;

  EXPECT_CALL(timer, initTimers());

  char GUID[SUPLA_GUID_SIZE] = {1};
  char AUTHKEY[SUPLA_AUTHKEY_SIZE] = {2};
  sd.setGUID(GUID);
  sd.setAuthKey(AUTHKEY);
  sd.setServer("supla.rulez");
  EXPECT_FALSE(sd.begin());
  EXPECT_EQ(sd.getCurrentStatus(), STATUS_MISSING_CREDENTIALS);
}

TEST_F(SuplaDeviceTests, SuccessfulBegin) {
  SrpcMock srpc;
  NetworkMockWithMac net;
  TimerMock timer;

  EXPECT_CALL(net, getMacAddr(_)).WillRepeatedly(Return(false));

  ::testing::InSequence seq;
  NetworkClientMock *client = new NetworkClientMock;
  SuplaDeviceClass sd;
  int dummy;

  EXPECT_CALL(timer, initTimers());

  char GUID[SUPLA_GUID_SIZE] = {1};
  char AUTHKEY[SUPLA_AUTHKEY_SIZE] = {2};
  sd.setGUID(GUID);
  sd.setServer("supla.rulez");
  sd.setEmail("john@supla");
  sd.setAuthKey(AUTHKEY);
  EXPECT_TRUE(sd.begin());
  EXPECT_EQ(sd.getCurrentStatus(), STATUS_INITIALIZED);
}

TEST_F(SuplaDeviceTests, SuccessfulBeginAlternative) {
  SrpcMock srpc;
  NetworkMockWithMac net;
  TimerMock timer;
  EXPECT_CALL(net, getMacAddr(_)).WillRepeatedly(Return(false));

  ::testing::InSequence seq;
  NetworkClientMock *client = new NetworkClientMock;
  SuplaDeviceClass sd;
  int dummy;

  EXPECT_CALL(timer, initTimers());

  char GUID[SUPLA_GUID_SIZE] = {1};
  char AUTHKEY[SUPLA_AUTHKEY_SIZE] = {2};
  EXPECT_TRUE(sd.begin(GUID, "supla.rulez", "superman@supla.org", AUTHKEY));
  EXPECT_EQ(sd.getCurrentStatus(), STATUS_INITIALIZED);
}

TEST_F(SuplaDeviceTests, FailedBeginAlternativeOnEmptyAUTHKEY) {
  ::testing::InSequence seq;
  SrpcMock srpc;
  NetworkMockWithMac net;
  TimerMock timer;

  NetworkClientMock *client = new NetworkClientMock;
  SuplaDeviceClass sd;
  int dummy;

  EXPECT_CALL(timer, initTimers());

  char GUID[SUPLA_GUID_SIZE] = {1};
  char AUTHKEY[SUPLA_AUTHKEY_SIZE] = {0};
  EXPECT_FALSE(sd.begin(GUID, "supla.rulez", "superman@supla.org", AUTHKEY));
  EXPECT_EQ(sd.getCurrentStatus(), STATUS_INVALID_AUTHKEY);
}

TEST_F(SuplaDeviceTests, TwoChannelElementsNoNetworkWithStorage) {
  SrpcMock srpc;
  NetworkMockWithMac net;
  EXPECT_CALL(net, getMacAddr(_)).WillRepeatedly(Return(false));
  StorageMockSimulator storage;
  EXPECT_CALL(storage, commit()).Times(2);
  TimerMock timer;
  TimeInterfaceStub time;
  NetworkClientMock *client = new NetworkClientMock;
  SuplaDeviceClass sd;
  ElementMock el1;
  ElementMock el2;
  int dummy;
  EXPECT_CALL(el1, onSaveState());
  EXPECT_CALL(el2, onSaveState());

  EXPECT_CALL(el1, onLoadState());
  EXPECT_CALL(el2, onLoadState());

  EXPECT_CALL(el1, onInit());
  EXPECT_CALL(el2, onInit());

  EXPECT_CALL(timer, initTimers());
  EXPECT_CALL(net, setup());

  char GUID[SUPLA_GUID_SIZE] = {1};
  char AUTHKEY[SUPLA_AUTHKEY_SIZE] = {2};
  EXPECT_TRUE(storage.isEmpty());
  EXPECT_TRUE(sd.begin(GUID, "supla.rulez", "superman@supla.org", AUTHKEY));
  EXPECT_EQ(sd.getCurrentStatus(), STATUS_INITIALIZED);
  EXPECT_CALL(el1, iterateAlways()).Times(2);
  EXPECT_CALL(el2, iterateAlways()).Times(2);
  EXPECT_CALL(net, isReady()).WillRepeatedly(Return(false));

  EXPECT_CALL(el1, onSaveState());
  EXPECT_CALL(el2, onSaveState());

  for (int i = 0; i < 2; i++) sd.iterate();
  EXPECT_TRUE(storage.isPreampleInitialized());
}

TEST_F(SuplaDeviceTests, OnVersionErrorShouldCallDisconnect) {
  NetworkMockWithMac net;
  NetworkClientMock *client = new NetworkClientMock;
  TimeInterfaceStub time;

  EXPECT_CALL(*client, stop()).Times(1);

  SuplaDeviceClass sd;
  Supla::Protocol::SuplaSrpc srpcLayer(&sd);

  TSDC_SuplaVersionError versionError{};

  srpcLayer.onVersionError(&versionError);
  EXPECT_EQ(sd.getCurrentStatus(), STATUS_PROTOCOL_VERSION_ERROR);
}

TEST_F(SuplaDeviceTests, OnRegisterResultOK) {
  NetworkClientMock *client = new NetworkClientMock;
  NetworkMockWithMac net;
  SrpcMock srpc;
  TimeInterfaceStub time;
  SuplaDeviceClass sd;
  Supla::Protocol::SuplaSrpc srpcLayer(&sd);

  EXPECT_CALL(srpc, srpc_dcs_async_set_activity_timeout(_, _)).Times(1);

  TSD_SuplaRegisterDeviceResult register_device_result{};
  register_device_result.result_code = SUPLA_RESULTCODE_TRUE;
  register_device_result.activity_timeout = 45;
  register_device_result.version = 20;
  register_device_result.version_min = 1;

  srpcLayer.onRegisterResult(&register_device_result);

  EXPECT_EQ(sd.getCurrentStatus(), STATUS_REGISTERED_AND_READY);
}

TEST_F(SuplaDeviceTests, OnRegisterResultBadCredentials) {
  NetworkMockWithMac net;
  SrpcMock srpc;
  TimeInterfaceStub time;
  NetworkClientMock *client = new NetworkClientMock;
  SuplaDeviceClass sd;
  Supla::Protocol::SuplaSrpc srpcLayer(&sd);

  EXPECT_CALL(srpc, srpc_dcs_async_set_activity_timeout(_, _)).Times(0);
  EXPECT_CALL(*client, stop()).Times(1);

  TSD_SuplaRegisterDeviceResult register_device_result{};
  register_device_result.result_code = SUPLA_RESULTCODE_BAD_CREDENTIALS;
  register_device_result.activity_timeout = 45;
  register_device_result.version = 20;
  register_device_result.version_min = 1;

  srpcLayer.onRegisterResult(&register_device_result);

  EXPECT_EQ(sd.getCurrentStatus(), STATUS_BAD_CREDENTIALS);
}

TEST_F(SuplaDeviceTests, OnRegisterResultTemporairlyUnavailable) {
  NetworkMockWithMac net;
  SrpcMock srpc;
  NetworkClientMock *client = new NetworkClientMock;
  TimeInterfaceStub time;
  SuplaDeviceClass sd;
  Supla::Protocol::SuplaSrpc srpcLayer(&sd);

  EXPECT_CALL(srpc, srpc_dcs_async_set_activity_timeout(_, _)).Times(0);
  EXPECT_CALL(*client, stop()).Times(1);

  TSD_SuplaRegisterDeviceResult register_device_result{};
  register_device_result.result_code = SUPLA_RESULTCODE_TEMPORARILY_UNAVAILABLE;
  register_device_result.activity_timeout = 45;
  register_device_result.version = 20;
  register_device_result.version_min = 1;

  srpcLayer.onRegisterResult(&register_device_result);

  EXPECT_EQ(sd.getCurrentStatus(), STATUS_TEMPORARILY_UNAVAILABLE);
}

TEST_F(SuplaDeviceTests, OnRegisterResultLocationConflict) {
  NetworkMockWithMac net;
  SrpcMock srpc;
  NetworkClientMock *client = new NetworkClientMock;
  TimeInterfaceStub time;
  SuplaDeviceClass sd;
  Supla::Protocol::SuplaSrpc srpcLayer(&sd);

  EXPECT_CALL(srpc, srpc_dcs_async_set_activity_timeout(_, _)).Times(0);
  EXPECT_CALL(*client, stop()).Times(1);

  TSD_SuplaRegisterDeviceResult register_device_result{};
  register_device_result.result_code = SUPLA_RESULTCODE_LOCATION_CONFLICT;
  register_device_result.activity_timeout = 45;
  register_device_result.version = 20;
  register_device_result.version_min = 1;

  srpcLayer.onRegisterResult(&register_device_result);

  EXPECT_EQ(sd.getCurrentStatus(), STATUS_LOCATION_CONFLICT);
}

TEST_F(SuplaDeviceTests, OnRegisterResultChannelConflict) {
  NetworkMockWithMac net;
  SrpcMock srpc;
  NetworkClientMock *client = new NetworkClientMock;
  TimeInterfaceStub time;
  SuplaDeviceClass sd;
  Supla::Protocol::SuplaSrpc srpcLayer(&sd);

  EXPECT_CALL(srpc, srpc_dcs_async_set_activity_timeout(_, _)).Times(0);
  EXPECT_CALL(*client, stop()).Times(1);

  TSD_SuplaRegisterDeviceResult register_device_result{};
  register_device_result.result_code = SUPLA_RESULTCODE_CHANNEL_CONFLICT;
  register_device_result.activity_timeout = 45;
  register_device_result.version = 20;
  register_device_result.version_min = 1;

  srpcLayer.onRegisterResult(&register_device_result);

  EXPECT_EQ(sd.getCurrentStatus(), STATUS_CHANNEL_CONFLICT);
}

TEST_F(SuplaDeviceTests, OnRegisterResultDeviceDisabled) {
  NetworkMockWithMac net;
  SrpcMock srpc;
  NetworkClientMock *client = new NetworkClientMock;
  TimeInterfaceStub time;
  SuplaDeviceClass sd;
  Supla::Protocol::SuplaSrpc srpcLayer(&sd);

  EXPECT_CALL(srpc, srpc_dcs_async_set_activity_timeout(_, _)).Times(0);
  EXPECT_CALL(*client, stop()).Times(1);

  TSD_SuplaRegisterDeviceResult register_device_result{};
  register_device_result.result_code = SUPLA_RESULTCODE_DEVICE_DISABLED;
  register_device_result.activity_timeout = 45;
  register_device_result.version = 20;
  register_device_result.version_min = 1;

  srpcLayer.onRegisterResult(&register_device_result);

  EXPECT_EQ(sd.getCurrentStatus(), STATUS_DEVICE_IS_DISABLED);
}

TEST_F(SuplaDeviceTests, OnRegisterResultLocationDisabled) {
  NetworkMockWithMac net;
  SrpcMock srpc;
  NetworkClientMock *client = new NetworkClientMock;
  TimeInterfaceStub time;
  SuplaDeviceClass sd;
  Supla::Protocol::SuplaSrpc srpcLayer(&sd);

  EXPECT_CALL(srpc, srpc_dcs_async_set_activity_timeout(_, _)).Times(0);
  EXPECT_CALL(*client, stop()).Times(1);

  TSD_SuplaRegisterDeviceResult register_device_result{};
  register_device_result.result_code = SUPLA_RESULTCODE_LOCATION_DISABLED;
  register_device_result.activity_timeout = 45;
  register_device_result.version = 20;
  register_device_result.version_min = 1;

  srpcLayer.onRegisterResult(&register_device_result);

  EXPECT_EQ(sd.getCurrentStatus(), STATUS_LOCATION_IS_DISABLED);
}

TEST_F(SuplaDeviceTests, OnRegisterResultDeviceLimitExceeded) {
  NetworkMockWithMac net;
  SrpcMock srpc;
  NetworkClientMock *client = new NetworkClientMock;
  TimeInterfaceStub time;
  SuplaDeviceClass sd;
  Supla::Protocol::SuplaSrpc srpcLayer(&sd);

  EXPECT_CALL(srpc, srpc_dcs_async_set_activity_timeout(_, _)).Times(0);
  EXPECT_CALL(*client, stop()).Times(1);

  TSD_SuplaRegisterDeviceResult register_device_result{};
  register_device_result.result_code = SUPLA_RESULTCODE_DEVICE_LIMITEXCEEDED;
  register_device_result.activity_timeout = 45;
  register_device_result.version = 20;
  register_device_result.version_min = 1;

  srpcLayer.onRegisterResult(&register_device_result);

  EXPECT_EQ(sd.getCurrentStatus(), STATUS_DEVICE_LIMIT_EXCEEDED);
}

TEST_F(SuplaDeviceTests, OnRegisterResultGuidError) {
  NetworkMockWithMac net;
  SrpcMock srpc;
  NetworkClientMock *client = new NetworkClientMock;
  TimeInterfaceStub time;
  SuplaDeviceClass sd;
  Supla::Protocol::SuplaSrpc srpcLayer(&sd);

  EXPECT_CALL(srpc, srpc_dcs_async_set_activity_timeout(_, _)).Times(0);
  EXPECT_CALL(*client, stop()).Times(1);

  TSD_SuplaRegisterDeviceResult register_device_result{};
  register_device_result.result_code = SUPLA_RESULTCODE_GUID_ERROR;
  register_device_result.activity_timeout = 45;
  register_device_result.version = 20;
  register_device_result.version_min = 1;

  srpcLayer.onRegisterResult(&register_device_result);

  EXPECT_EQ(sd.getCurrentStatus(), STATUS_INVALID_GUID);
}

TEST_F(SuplaDeviceTests, OnRegisterResultAuthKeyError) {
  NetworkMockWithMac net;
  SrpcMock srpc;
  NetworkClientMock *client = new NetworkClientMock;
  TimeInterfaceStub time;
  SuplaDeviceClass sd;
  Supla::Protocol::SuplaSrpc srpcLayer(&sd);

  EXPECT_CALL(srpc, srpc_dcs_async_set_activity_timeout(_, _)).Times(0);
  EXPECT_CALL(*client, stop()).Times(1);

  TSD_SuplaRegisterDeviceResult register_device_result{};
  register_device_result.result_code = SUPLA_RESULTCODE_AUTHKEY_ERROR;
  register_device_result.activity_timeout = 45;
  register_device_result.version = 20;
  register_device_result.version_min = 1;

  srpcLayer.onRegisterResult(&register_device_result);

  EXPECT_EQ(sd.getCurrentStatus(), STATUS_INVALID_AUTHKEY);
}

TEST_F(SuplaDeviceTests, OnRegisterResultRegistrationDisabled) {
  NetworkMockWithMac net;
  SrpcMock srpc;
  NetworkClientMock *client = new NetworkClientMock;
  TimeInterfaceStub time;
  SuplaDeviceClass sd;
  Supla::Protocol::SuplaSrpc srpcLayer(&sd);

  EXPECT_CALL(srpc, srpc_dcs_async_set_activity_timeout(_, _)).Times(0);
  EXPECT_CALL(*client, stop()).Times(1);

  TSD_SuplaRegisterDeviceResult register_device_result{};
  register_device_result.result_code = SUPLA_RESULTCODE_REGISTRATION_DISABLED;
  register_device_result.activity_timeout = 45;
  register_device_result.version = 20;
  register_device_result.version_min = 1;

  srpcLayer.onRegisterResult(&register_device_result);

  EXPECT_EQ(sd.getCurrentStatus(), STATUS_REGISTRATION_DISABLED);
}

TEST_F(SuplaDeviceTests, OnRegisterResultNoLocationAvailable) {
  NetworkMockWithMac net;
  SrpcMock srpc;
  NetworkClientMock *client = new NetworkClientMock;
  TimeInterfaceStub time;
  SuplaDeviceClass sd;
  Supla::Protocol::SuplaSrpc srpcLayer(&sd);

  EXPECT_CALL(srpc, srpc_dcs_async_set_activity_timeout(_, _)).Times(0);
  EXPECT_CALL(*client, stop()).Times(1);

  TSD_SuplaRegisterDeviceResult register_device_result{};
  register_device_result.result_code = SUPLA_RESULTCODE_NO_LOCATION_AVAILABLE;
  register_device_result.activity_timeout = 45;
  register_device_result.version = 20;
  register_device_result.version_min = 1;

  srpcLayer.onRegisterResult(&register_device_result);

  EXPECT_EQ(sd.getCurrentStatus(), STATUS_NO_LOCATION_AVAILABLE);
}

TEST_F(SuplaDeviceTests, OnRegisterResultUnknownError) {
  NetworkMockWithMac net;
  SrpcMock srpc;
  NetworkClientMock *client = new NetworkClientMock;
  TimeInterfaceStub time;
  SuplaDeviceClass sd;
  Supla::Protocol::SuplaSrpc srpcLayer(&sd);

  EXPECT_CALL(srpc, srpc_dcs_async_set_activity_timeout(_, _)).Times(0);
  EXPECT_CALL(*client, stop()).Times(1);

  TSD_SuplaRegisterDeviceResult register_device_result{};
  register_device_result.result_code = 666;
  register_device_result.activity_timeout = 45;
  register_device_result.version = 20;
  register_device_result.version_min = 1;

  srpcLayer.onRegisterResult(&register_device_result);

  EXPECT_EQ(sd.getCurrentStatus(), STATUS_UNKNOWN_ERROR);
}

TEST_F(SuplaDeviceTests, GenerateHostnameTests) {
  NetworkMockWithMac net;
  SuplaDeviceClass sd;
  char buf[200];
  EXPECT_STREQ(Supla::Channel::reg_dev.Name, "");
  sd.setName("Supla Device");

  EXPECT_CALL(net, getMacAddr(_)).WillRepeatedly(Return(true));

  EXPECT_STREQ(Supla::Channel::reg_dev.Name, "Supla Device");

  sd.generateHostname(buf, 6);
  EXPECT_STREQ(buf, "SUPLA-DEVICE-000000000000");

  sd.generateHostname(buf, 1);
  EXPECT_STREQ(buf, "SUPLA-DEVICE-00");

  sd.generateHostname(buf, 2);
  EXPECT_STREQ(buf, "SUPLA-DEVICE-0000");

  sd.generateHostname(buf, 3);
  EXPECT_STREQ(buf, "SUPLA-DEVICE-000000");

  sd.generateHostname(buf, 4);
  EXPECT_STREQ(buf, "SUPLA-DEVICE-00000000");

  sd.generateHostname(buf, 5);
  EXPECT_STREQ(buf, "SUPLA-DEVICE-0000000000");

  sd.generateHostname(buf, 6);
  EXPECT_STREQ(buf, "SUPLA-DEVICE-000000000000");

  sd.generateHostname(buf, 7);
  EXPECT_STREQ(buf, "SUPLA-DEVICE-000000000000");

  sd.generateHostname(buf, 0);
  EXPECT_STREQ(buf, "SUPLA-DEVICE");

  sd.generateHostname(buf, -1);
  EXPECT_STREQ(buf, "SUPLA-DEVICE");

  sd.setName("SuPlA Is SuPeR");
  sd.generateHostname(buf, -1);
  EXPECT_STREQ(buf, "SUPLA-IS-SUPER");

  sd.setName("SuPlA Is SuPeR Even with a very long device name");
  sd.generateHostname(buf, 6);
  EXPECT_STREQ(buf, "SUPLA-IS-SUPER-EVE-000000000000");

  sd.generateHostname(buf, 2);
  EXPECT_STREQ(buf, "SUPLA-IS-SUPER-EVEN-WITH-A-0000");

  memset(&(Supla::Channel::reg_dev), 0, sizeof(Supla::Channel::reg_dev));
  sd.generateHostname(buf, 2);
  EXPECT_STREQ(buf, "SUPLA-DEVICE-0000");

  sd.setName("- - TEST   DupliCate");
  sd.generateHostname(buf, 2);
  EXPECT_STREQ(buf, "TEST-DUPLICATE-0000");

  /*
  sd.setName("My Device 2.54");
  sd.generateHostname(buf, 2);
  EXPECT_STREQ(buf, "SUPLA-MY-DEVICE-2-54-0000");
  */
}

TEST_F(SuplaDeviceTests, GenerateHostnameForOHTests) {
  NetworkMockWithMac net;
  SuplaDeviceClass sd;
  char buf[200];
  EXPECT_STREQ(Supla::Channel::reg_dev.Name, "");
  sd.setName("OH! Amazing!! Device");

  EXPECT_CALL(net, getMacAddr(_)).WillRepeatedly(Return(true));

  EXPECT_STREQ(Supla::Channel::reg_dev.Name, "OH! Amazing!! Device");

  sd.generateHostname(buf, 6);
  EXPECT_STREQ(buf, "SUPLA-AMAZING-DEVI-000000000000");

  sd.setName("OH!Really???");
  sd.generateHostname(buf, 6);
  EXPECT_STREQ(buf, "SUPLA-REALLY-000000000000");
}

TEST_F(SuplaDeviceTests, GenerateHostnameWithCustomPrefixTests) {
  NetworkMockWithMac net;
  SuplaDeviceClass sd;
  char buf[200];
  EXPECT_STREQ(Supla::Channel::reg_dev.Name, "");
  sd.setName("Amazing Device");

  EXPECT_CALL(net, getMacAddr(_)).WillRepeatedly(Return(true));

  EXPECT_STREQ(Supla::Channel::reg_dev.Name, "Amazing Device");

  sd.generateHostname(buf, 6);
  EXPECT_STREQ(buf, "AMAZING-DEVICE-000000000000");

  char prefix[] = "My prefix";
  sd.setCustomHostnamePrefix(prefix);

  sd.generateHostname(buf, 6);
  EXPECT_STREQ(buf, "MY-PREFIX-000000000000");

  sd.generateHostname(buf, 7);
  EXPECT_STREQ(buf, "MY-PREFIX-000000000000");

  sd.generateHostname(buf, 0);
  EXPECT_STREQ(buf, "MY-PREFIX");

  sd.generateHostname(buf, -1);
  EXPECT_STREQ(buf, "MY-PREFIX");

  sd.setName("SuPlA Is SuPeR");
  sd.generateHostname(buf, -1);
  EXPECT_STREQ(buf, "MY-PREFIX");

  sd.setCustomHostnamePrefix(nullptr);

  sd.generateHostname(buf, 6);
  EXPECT_STREQ(buf, "SUPLA-IS-SUPER-000000000000");

  char emptyPrefix[1] = {};
  sd.setCustomHostnamePrefix(emptyPrefix);

  sd.generateHostname(buf, 6);
  EXPECT_STREQ(buf, "SUPLA-IS-SUPER-000000000000");

  memset(&(Supla::Channel::reg_dev), 0, sizeof(Supla::Channel::reg_dev));
  sd.generateHostname(buf, 2);
  EXPECT_STREQ(buf, "SUPLA-DEVICE-0000");

  /*
  sd.setName("SuplaDevice 3.14");
  sd.generateHostname(buf, 2);
  EXPECT_STREQ(buf, "SUPLA-DEVICE-3-14-0000");

  sd.setName("My Device 2.54");
  sd.generateHostname(buf, 2);
  EXPECT_STREQ(buf, "SUPLA-MY-DEVICE-2-54-0000");
  */
}

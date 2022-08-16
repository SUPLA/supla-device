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

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <arduino_mock.h>
#include <srpc_mock.h>
#include <timer_mock.h>
#include <SuplaDevice.h>
#include <supla/clock/clock.h>
#include <supla/storage/storage.h>
#include <element_mock.h>
#include <board_mock.h>

using ::testing::Return;
using ::testing::_;
using ::testing::DoAll;
using ::testing::Assign;
using ::testing::ReturnPointee;

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

class SimpleTime : public TimeInterface {
  public:
    SimpleTime() : value(0) {}

    virtual unsigned long millis() override {
      return value;
    }

    void advance(int advanceMs) {
      value += advanceMs;
    }

    unsigned long value;
};

class StorageMock2: public Supla::Storage {
 public:
  MOCK_METHOD(bool, init, (), (override));
  MOCK_METHOD(bool, prepareState, (bool), (override));
  MOCK_METHOD(bool, finalizeSaveState, (), (override));
  MOCK_METHOD(void, commit, (), (override));
  MOCK_METHOD(int, readStorage, (unsigned int, unsigned char *, int, bool), (override));
  MOCK_METHOD(int, writeStorage, (unsigned int, const unsigned char *, int), (override));

};

class NetworkMock : public Supla::Network {
  public:
    NetworkMock() : Supla::Network(nullptr) {};
  MOCK_METHOD(int, read, (void *, int ), (override));
  MOCK_METHOD(int, write, (void *, int ), (override));
  MOCK_METHOD(int, connect, (const char *, int), (override));
  MOCK_METHOD(bool, connected, (), (override));
  MOCK_METHOD(void, disconnect, (), (override));
  MOCK_METHOD(void, setup, (), (override));
  MOCK_METHOD(void, setTimeout, (int), (override));

  MOCK_METHOD(bool, isReady, (), (override));
  MOCK_METHOD(bool, iterate, (), (override));

};

class SuplaDeviceTestsFullStartup : public SuplaDeviceTests {
  protected:
    SrpcMock srpc;
    NetworkMock net;
    TimerMock timer;
    SimpleTime time;
    SuplaDeviceClass sd;
    ElementMock el1;
    ElementMock el2;
    BoardMock board;

    virtual void SetUp() {
      SuplaDeviceTests::SetUp();

      int dummy;

      EXPECT_CALL(el1, onInit());
      EXPECT_CALL(el2, onInit());

      EXPECT_CALL(timer, initTimers());
      EXPECT_CALL(net, setup());
      EXPECT_CALL(srpc, srpc_params_init(_));
      EXPECT_CALL(srpc, srpc_init(_)).WillOnce(Return(&dummy));
      EXPECT_CALL(srpc, srpc_set_proto_version(&dummy, 16));

      char GUID[SUPLA_GUID_SIZE] = {1};
      char AUTHKEY[SUPLA_AUTHKEY_SIZE] = {2};
      EXPECT_TRUE(sd.begin(GUID, "supla.rulez", "superman@supla.org", AUTHKEY));
      EXPECT_EQ(sd.getCurrentStatus(), STATUS_INITIALIZED);
    }

    virtual void TearDown() {
      SuplaDeviceTests::TearDown();
    }
};

using ::testing::AtLeast;

TEST_F(SuplaDeviceTestsFullStartup, NoNetworkShouldCallSetupAgain) {
  EXPECT_CALL(net, isReady()).WillRepeatedly(Return(false));
  EXPECT_CALL(net, setup()).Times(2);
  EXPECT_CALL(el1, iterateAlways()).Times(AtLeast(1));
  EXPECT_CALL(el2, iterateAlways()).Times(AtLeast(1));

  for (int i = 0; i < 131*10; i++) {
    sd.iterate();
    time.advance(100);
  }
  EXPECT_EQ(sd.getCurrentStatus(), STATUS_NETWORK_DISCONNECTED);
}

TEST_F(SuplaDeviceTestsFullStartup, FailedConnectionShouldSetupNetworkAgain) {
  EXPECT_CALL(net, isReady()).WillRepeatedly(Return(true));
  EXPECT_CALL(net, connected()).WillRepeatedly(Return(false));
  EXPECT_CALL(net, connect(_, _)).WillRepeatedly(Return(0));
  EXPECT_CALL(net, disconnect()).Times(AtLeast(1));
  EXPECT_CALL(net, iterate()).WillRepeatedly(Return(true));

  EXPECT_CALL(net, setup()).Times(1);
  EXPECT_CALL(el1, iterateAlways()).Times(AtLeast(1));
  EXPECT_CALL(el2, iterateAlways()).Times(AtLeast(1));

  for (int i = 0; i < 61*10; i++) {
    sd.iterate();
    time.advance(100);
  }
  EXPECT_EQ(sd.getCurrentStatus(), STATUS_SERVER_DISCONNECTED);
}

TEST_F(SuplaDeviceTestsFullStartup, SrpcFailureShouldCallDisconnect) {
  EXPECT_CALL(net, isReady()).WillRepeatedly(Return(true));
  EXPECT_CALL(net, connected()).WillOnce(Return(false)).WillRepeatedly(Return(false));
  EXPECT_CALL(net, connect(_, _)).WillRepeatedly(Return(1));
  EXPECT_CALL(net, iterate()).Times(1);
  EXPECT_CALL(srpc, srpc_iterate(_)).WillOnce(Return(SUPLA_RESULT_FALSE));

  EXPECT_CALL(net, disconnect()).Times(1);

  EXPECT_CALL(el1, iterateAlways()).Times(AtLeast(1));
  EXPECT_CALL(el2, iterateAlways()).Times(AtLeast(1));

  sd.iterate();
  EXPECT_EQ(sd.getCurrentStatus(), STATUS_ITERATE_FAIL);
}

TEST_F(SuplaDeviceTestsFullStartup, NoReplyForDeviceRegistrationShoudResetConnection) {
  bool isConnected = false;
  EXPECT_CALL(net, isReady()).WillRepeatedly(Return(true));
  EXPECT_CALL(net, connected()).WillRepeatedly(ReturnPointee(&isConnected));
  EXPECT_CALL(net, connect(_, _)).WillRepeatedly(DoAll(Assign(&isConnected, true), Return(1)));

  EXPECT_CALL(net, iterate()).Times(AtLeast(1));
  EXPECT_CALL(srpc, srpc_iterate(_)).WillRepeatedly(Return(SUPLA_RESULT_TRUE));

  EXPECT_CALL(el1, iterateAlways()).Times(AtLeast(1));
  EXPECT_CALL(el2, iterateAlways()).Times(AtLeast(1));

  EXPECT_CALL(net, disconnect()).WillOnce(Assign(&isConnected, false));

  EXPECT_CALL(srpc, srpc_ds_async_registerdevice_e(_, _)).Times(2);

  for (int i = 0; i < 11*10; i++) {
    sd.iterate();
    time.advance(100);
  }
  EXPECT_EQ(sd.getCurrentStatus(), STATUS_SERVER_DISCONNECTED);
  for (int i = 0; i < 2*10; i++) {
    sd.iterate();
    time.advance(100);
  }
  EXPECT_EQ(sd.getCurrentStatus(), STATUS_REGISTER_IN_PROGRESS);
}


TEST_F(SuplaDeviceTestsFullStartup, SuccessfulStartup) {
  bool isConnected = false;
  EXPECT_CALL(net, isReady()).WillRepeatedly(Return(true));
  EXPECT_CALL(net, connected()).WillRepeatedly(ReturnPointee(&isConnected));
  EXPECT_CALL(net, connect(_, _)).WillRepeatedly(DoAll(Assign(&isConnected, true), Return(1)));

  EXPECT_CALL(net, iterate()).Times(AtLeast(1));
  EXPECT_CALL(srpc, srpc_iterate(_)).WillRepeatedly(Return(SUPLA_RESULT_TRUE));

  EXPECT_CALL(el1, iterateAlways()).Times(35);
  EXPECT_CALL(el2, iterateAlways()).Times(35);

  EXPECT_CALL(el1, onRegistered());
  EXPECT_CALL(el2, onRegistered());

  EXPECT_CALL(srpc, srpc_ds_async_registerdevice_e(_, _)).Times(1);
  EXPECT_CALL(srpc, srpc_dcs_async_set_activity_timeout(_, _)).Times(1);
  EXPECT_CALL(srpc, srpc_dcs_async_ping_server(_)).Times(2);

//  EXPECT_CALL(net, ping(_)).WillRepeatedly(Return(true));
  EXPECT_CALL(el1, iterateConnected(_)).Times(30).WillRepeatedly(Return(true));
  EXPECT_CALL(el2, iterateConnected(_)).Times(30).WillRepeatedly(Return(true));

  EXPECT_EQ(sd.getCurrentStatus(), STATUS_INITIALIZED);
  for (int i = 0; i < 5; i++) {
    sd.iterate();
    time.advance(1000);
  }
  EXPECT_EQ(sd.getCurrentStatus(), STATUS_REGISTER_IN_PROGRESS);

  TSD_SuplaRegisterDeviceResult register_device_result{};
  register_device_result.result_code = SUPLA_RESULTCODE_TRUE;
  register_device_result.activity_timeout = 45;
  register_device_result.version = 16;
  register_device_result.version_min = 1;

  auto srpcLayer = sd.getSrpcLayer();
  srpcLayer->onRegisterResult(&register_device_result);
  time.advance(100);

  EXPECT_EQ(sd.getCurrentStatus(), STATUS_REGISTERED_AND_READY);

  for (int i = 0; i < 30; i++) {
    sd.iterate();
    time.advance(1000);
  }

  EXPECT_EQ(sd.getCurrentStatus(), STATUS_REGISTERED_AND_READY);
}

TEST_F(SuplaDeviceTestsFullStartup, NoNetworkShouldCallSetupAgainAndResetDev) {
  EXPECT_CALL(net, isReady()).WillRepeatedly(Return(false));
  EXPECT_CALL(net, setup()).Times(1);
  EXPECT_CALL(el1, iterateAlways()).Times(AtLeast(1));
  EXPECT_CALL(el2, iterateAlways()).Times(AtLeast(1));
  // In tests we can't reset board, so this method will be called few times
  EXPECT_CALL(board, deviceSoftwareReset()).Times(AtLeast(1));
  EXPECT_CALL(el1, onSaveState()).Times(AtLeast(1));
  EXPECT_CALL(el2, onSaveState()).Times(AtLeast(1));
  sd.setAutomaticResetOnConnectionProblem(100);

  for (int i = 0; i < 102*10; i++) {
    sd.iterate();
    time.advance(100);
  }

  EXPECT_EQ(sd.getCurrentStatus(), STATUS_SOFTWARE_RESET);
}


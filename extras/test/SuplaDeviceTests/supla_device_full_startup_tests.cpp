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
#include <board_mock.h>
#include <config_mock.h>
#include <element_mock.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <network_client_mock.h>
#include <network_mock.h>
#include <srpc_mock.h>
#include <supla/clock/clock.h>
#include <supla/protocol/supla_srpc.h>
#include <supla/sensor/therm_hygro_press_meter.h>
#include <supla/storage/storage.h>
#include <timer_mock.h>
#include <simple_time.h>

// Update this value if you change default Proto Version
const int defaultProtoVersion = 21;

using ::testing::_;
using ::testing::Assign;
using ::testing::DoAll;
using ::testing::Return;
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
  NetworkClientMock *client = nullptr;

  virtual void SetUp() {
    client = new NetworkClientMock;  // it will be destroyed in
                                     // Supla::Protocol::SuplaSrpc
    SuplaDeviceTests::SetUp();

    EXPECT_CALL(el1, onInit());
    EXPECT_CALL(el2, onInit());

    EXPECT_CALL(timer, initTimers());

    char GUID[SUPLA_GUID_SIZE] = {1};
    char AUTHKEY[SUPLA_AUTHKEY_SIZE] = {2};
    EXPECT_TRUE(sd.begin(GUID, "supla.rulez", "superman@supla.org", AUTHKEY));
    EXPECT_EQ(sd.getCurrentStatus(), STATUS_INITIALIZED);
  }

  virtual void TearDown() {
    SuplaDeviceTests::TearDown();
  }
};

class SuplaDeviceElementWithSecondaryChannel
    : public SuplaDeviceTestsFullStartup {
 protected:
  Supla::Sensor::ThermHygroPressMeter thpm;
};

class SuplaDeviceTestsFullStartupManual : public SuplaDeviceTests {
  protected:
    SrpcMock srpc;
    NetworkMock net;
    TimerMock timer;
    SimpleTime time;
    SuplaDeviceClass sd;
    ElementMock el1;
    ElementMock el2;
    BoardMock board;
    NetworkClientMock *client = nullptr;

    virtual void SetUp() {
      client = new NetworkClientMock;  // it will be destroyed in
                                       // Supla::Protocol::SuplaSrpc
      SuplaDeviceTests::SetUp();
    }

    virtual void TearDown() {
      SuplaDeviceTests::TearDown();
    }
};

using ::testing::AtLeast;

TEST_F(SuplaDeviceTestsFullStartup, NoNetworkShouldCallSetupAgain) {
  EXPECT_CALL(net, isReady()).WillRepeatedly(Return(false));
  EXPECT_CALL(net, setup()).Times(3);
  EXPECT_CALL(*client, stop()).Times(2);
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
  EXPECT_CALL(*client, connected()).WillRepeatedly(Return(false));
  EXPECT_CALL(*client, connectImp(_, _)).WillRepeatedly(Return(0));
  EXPECT_CALL(*client, stop()).Times(AtLeast(1));
  EXPECT_CALL(net, iterate()).WillRepeatedly(Return(true));

  EXPECT_CALL(net, setup()).Times(2);
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
  EXPECT_CALL(*client, connected()).WillOnce(Return(false)).WillRepeatedly(Return(false));
  EXPECT_CALL(*client, connectImp(_, _)).WillRepeatedly(Return(1));
  EXPECT_CALL(net, setup()).Times(1);
  EXPECT_CALL(net, iterate()).Times(1);
  EXPECT_CALL(srpc, srpc_iterate(_)).WillOnce(Return(SUPLA_RESULT_FALSE));
  EXPECT_CALL(srpc, srpc_params_init(_));
  int dummy;
  EXPECT_CALL(srpc, srpc_init(_)).WillOnce(Return(&dummy));
  EXPECT_CALL(srpc, srpc_set_proto_version(&dummy, defaultProtoVersion));
  EXPECT_CALL(srpc, srpc_free(_));

  EXPECT_CALL(*client, stop()).Times(1);

  EXPECT_CALL(el1, iterateAlways()).Times(AtLeast(1));
  EXPECT_CALL(el2, iterateAlways()).Times(AtLeast(1));

  sd.iterate();
  EXPECT_EQ(sd.getCurrentStatus(), STATUS_ITERATE_FAIL);
}

TEST_F(SuplaDeviceTestsFullStartup, NoReplyForDeviceRegistrationShoudResetConnection) {
  bool isConnected = false;
  EXPECT_CALL(net, isReady()).WillRepeatedly(Return(true));
  EXPECT_CALL(*client, connected()).WillRepeatedly(ReturnPointee(&isConnected));
  EXPECT_CALL(*client, connectImp(_, _)).WillRepeatedly(DoAll(Assign(&isConnected, true), Return(1)));

  EXPECT_CALL(net, setup()).Times(1);
  EXPECT_CALL(net, iterate()).Times(AtLeast(1));
  EXPECT_CALL(srpc, srpc_iterate(_)).WillRepeatedly(Return(SUPLA_RESULT_TRUE));
  EXPECT_CALL(srpc, srpc_params_init(_)).Times(AtLeast(1));
  int dummy;
  EXPECT_CALL(srpc, srpc_init(_)).WillRepeatedly(Return(&dummy));
  EXPECT_CALL(srpc, srpc_set_proto_version(&dummy, defaultProtoVersion)).Times(AtLeast(1));
  EXPECT_CALL(srpc, srpc_free(_));

  EXPECT_CALL(el1, iterateAlways()).Times(AtLeast(1));
  EXPECT_CALL(el2, iterateAlways()).Times(AtLeast(1));

  EXPECT_CALL(*client, stop()).WillOnce(Assign(&isConnected, false));

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
  EXPECT_CALL(*client, connected()).WillRepeatedly(ReturnPointee(&isConnected));
  EXPECT_CALL(*client, connectImp(_, _)).WillRepeatedly(DoAll(Assign(&isConnected, true), Return(1)));

  EXPECT_CALL(net, setup()).Times(1);
  EXPECT_CALL(net, iterate()).Times(AtLeast(1));
  EXPECT_CALL(srpc, srpc_iterate(_)).WillRepeatedly(Return(SUPLA_RESULT_TRUE));
  EXPECT_CALL(srpc, srpc_params_init(_));
  int dummy;
  EXPECT_CALL(srpc, srpc_init(_)).WillOnce(Return(&dummy));
  EXPECT_CALL(srpc, srpc_set_proto_version(&dummy, defaultProtoVersion));

  EXPECT_CALL(el1, iterateAlways()).Times(35);
  EXPECT_CALL(el2, iterateAlways()).Times(35);

  EXPECT_CALL(el1, onRegistered(_));
  EXPECT_CALL(el2, onRegistered(_));

  EXPECT_CALL(srpc, srpc_ds_async_registerdevice_e(_, _)).Times(1);
  EXPECT_CALL(srpc, srpc_dcs_async_set_activity_timeout(_, _)).Times(1);
  EXPECT_CALL(srpc, srpc_dcs_async_ping_server(_)).Times(2);

//  EXPECT_CALL(net, ping(_)).WillRepeatedly(Return(true));
  EXPECT_CALL(el1, iterateConnected()).Times(30).WillRepeatedly(Return(true));
  EXPECT_CALL(el2, iterateConnected()).Times(30).WillRepeatedly(Return(true));

  EXPECT_EQ(sd.getCurrentStatus(), STATUS_INITIALIZED);
  for (int i = 0; i < 5; i++) {
    sd.iterate();
    time.advance(1000);
  }
  EXPECT_EQ(sd.getCurrentStatus(), STATUS_REGISTER_IN_PROGRESS);

  TSD_SuplaRegisterDeviceResult register_device_result{};
  register_device_result.result_code = SUPLA_RESULTCODE_TRUE;
  register_device_result.activity_timeout = 45;
  register_device_result.version = 20;
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
  EXPECT_CALL(net, setup()).Times(2);
  EXPECT_CALL(*client, stop()).Times(1);
  EXPECT_CALL(el1, iterateAlways()).Times(AtLeast(1));
  EXPECT_CALL(el2, iterateAlways()).Times(AtLeast(1));
  // In tests we can't reset board, so this method will be called few times
  EXPECT_CALL(board, deviceSoftwareReset()).Times(AtLeast(1));
  EXPECT_CALL(el1, onSaveState()).Times(0);
  EXPECT_CALL(el2, onSaveState()).Times(0);
  sd.setAutomaticResetOnConnectionProblem(100);

  for (int i = 0; i < 102*10; i++) {
    sd.iterate();
    time.advance(100);
  }

  EXPECT_EQ(sd.getCurrentStatus(), STATUS_SOFTWARE_RESET);
}

TEST_F(SuplaDeviceTestsFullStartupManual,
    SslDisabledNoConfig) {
  net.setSSLEnabled(false);
  EXPECT_EQ(Supla::Storage::ConfigInstance(), nullptr);
  int dummy = 0;
  sd.setActivityTimeout(45);

  EXPECT_CALL(el1, onLoadConfig(_)).Times(0);
  EXPECT_CALL(el2, onLoadConfig(_)).Times(0);
  EXPECT_CALL(el1, onInit()).Times(1);
  EXPECT_CALL(el2, onInit()).Times(1);
  EXPECT_CALL(el1, onRegistered(_));
  EXPECT_CALL(el2, onRegistered(_));

  EXPECT_CALL(timer, initTimers());
  EXPECT_CALL(srpc, srpc_params_init(_));
  EXPECT_CALL(srpc, srpc_init(_)).WillOnce(Return(&dummy));
  EXPECT_CALL(srpc, srpc_set_proto_version(&dummy, 18));

  char GUID[SUPLA_GUID_SIZE] = {1};
  char AUTHKEY[SUPLA_AUTHKEY_SIZE] = {2};
  EXPECT_TRUE(sd.begin(GUID, "supla.rulez", "superman@supla.org", AUTHKEY, 18));
  EXPECT_EQ(sd.getCurrentStatus(), STATUS_INITIALIZED);

  EXPECT_CALL(net, isReady()).WillRepeatedly(Return(true));
  EXPECT_CALL(net, setup()).Times(1);
  EXPECT_CALL(net, iterate()).Times(AtLeast(1));
  EXPECT_CALL(srpc, srpc_iterate(_)).WillRepeatedly(Return(SUPLA_RESULT_TRUE));
  EXPECT_CALL(*client, stop()).Times(0);

  EXPECT_CALL(*client, connected()).WillOnce(Return(false))
    .WillRepeatedly(Return(true));
  // SSL disabled, port should be 2015
  EXPECT_CALL(*client, connectImp(_, 2015)).WillRepeatedly(Return(1));
  EXPECT_CALL(srpc, srpc_ds_async_registerdevice_e(_, _)).Times(1);

  EXPECT_CALL(el1, iterateAlways()).Times(AtLeast(1));
  EXPECT_CALL(el2, iterateAlways()).Times(AtLeast(1));
  EXPECT_CALL(el1, iterateConnected()).Times(AtLeast(1))
        .WillOnce(Return(false))
        .WillRepeatedly(Return(true));
  EXPECT_CALL(el2, iterateConnected()).Times(AtLeast(1));

  EXPECT_CALL(board, deviceSoftwareReset()).Times(0);
//  EXPECT_CALL(el1, onSaveState()).Times(AtLeast(1));
//  EXPECT_CALL(el2, onSaveState()).Times(AtLeast(1));

  EXPECT_EQ(sd.getCurrentStatus(), STATUS_INITIALIZED);
  for (int i = 0; i < 5; i++) {
    sd.iterate();
    time.advance(100);
  }
  EXPECT_EQ(sd.getCurrentStatus(), STATUS_REGISTER_IN_PROGRESS);

  TSD_SuplaRegisterDeviceResult register_device_result{};
  register_device_result.result_code = SUPLA_RESULTCODE_TRUE;
  register_device_result.activity_timeout = 45;
  register_device_result.version = 20;
  register_device_result.version_min = 1;

  auto srpcLayer = sd.getSrpcLayer();
  srpcLayer->onRegisterResult(&register_device_result);
  time.advance(100);

  EXPECT_EQ(sd.getCurrentStatus(), STATUS_REGISTERED_AND_READY);

  for (int i = 0; i < 10; i++) {
    sd.iterate();
    time.advance(100);
  }

  EXPECT_EQ(sd.getCurrentStatus(), STATUS_REGISTERED_AND_READY);
  EXPECT_EQ(client->getRootCACert(), nullptr);
}

TEST_F(SuplaDeviceTestsFullStartupManual,
    SslDisabledCAConfiguredNoConfig) {
  net.setSSLEnabled(false);
  EXPECT_EQ(Supla::Storage::ConfigInstance(), nullptr);
  int dummy = 0;
  sd.setActivityTimeout(45);
  const char myCA1[] = "test CA1";
  const char myCA2[] = "test CA2";
  sd.setSuplaCACert(myCA1);
  sd.setSupla3rdPartyCACert(myCA2);

  EXPECT_CALL(el1, onLoadConfig(_)).Times(0);
  EXPECT_CALL(el2, onLoadConfig(_)).Times(0);
  EXPECT_CALL(el1, onInit()).Times(1);
  EXPECT_CALL(el2, onInit()).Times(1);
  EXPECT_CALL(el1, onRegistered(_));
  EXPECT_CALL(el2, onRegistered(_));

  EXPECT_CALL(timer, initTimers());
  EXPECT_CALL(srpc, srpc_params_init(_));
  EXPECT_CALL(srpc, srpc_init(_)).WillOnce(Return(&dummy));
  EXPECT_CALL(srpc, srpc_set_proto_version(&dummy, 18));

  char GUID[SUPLA_GUID_SIZE] = {1};
  char AUTHKEY[SUPLA_AUTHKEY_SIZE] = {2};
  EXPECT_TRUE(sd.begin(GUID, "supla.rulez", "superman@supla.org", AUTHKEY, 18));
  EXPECT_EQ(sd.getCurrentStatus(), STATUS_INITIALIZED);

  EXPECT_CALL(net, isReady()).WillRepeatedly(Return(true));
  EXPECT_CALL(net, setup()).Times(1);
  EXPECT_CALL(net, iterate()).Times(AtLeast(1));
  EXPECT_CALL(srpc, srpc_iterate(_)).WillRepeatedly(Return(SUPLA_RESULT_TRUE));
  EXPECT_CALL(*client, stop()).Times(0);

  EXPECT_CALL(*client, connected()).WillOnce(Return(false))
    .WillRepeatedly(Return(true));
  // SSL disabled, port should be 2015
  EXPECT_CALL(*client, connectImp(_, 2015)).WillRepeatedly(Return(1));
  EXPECT_CALL(srpc, srpc_ds_async_registerdevice_e(_, _)).Times(1);

  EXPECT_CALL(el1, iterateAlways()).Times(AtLeast(1));
  EXPECT_CALL(el2, iterateAlways()).Times(AtLeast(1));
  EXPECT_CALL(el1, iterateConnected()).Times(AtLeast(1))
        .WillOnce(Return(false))
        .WillRepeatedly(Return(true));
  EXPECT_CALL(el2, iterateConnected()).Times(AtLeast(1));

  EXPECT_CALL(board, deviceSoftwareReset()).Times(0);
//  EXPECT_CALL(el1, onSaveState()).Times(AtLeast(1));
//  EXPECT_CALL(el2, onSaveState()).Times(AtLeast(1));

  EXPECT_EQ(sd.getCurrentStatus(), STATUS_INITIALIZED);
  for (int i = 0; i < 5; i++) {
    sd.iterate();
    time.advance(100);
  }
  EXPECT_EQ(sd.getCurrentStatus(), STATUS_REGISTER_IN_PROGRESS);

  TSD_SuplaRegisterDeviceResult register_device_result{};
  register_device_result.result_code = SUPLA_RESULTCODE_TRUE;
  register_device_result.activity_timeout = 45;
  register_device_result.version = 20;
  register_device_result.version_min = 1;

  auto srpcLayer = sd.getSrpcLayer();
  srpcLayer->onRegisterResult(&register_device_result);
  time.advance(100);

  EXPECT_EQ(sd.getCurrentStatus(), STATUS_REGISTERED_AND_READY);

  for (int i = 0; i < 10; i++) {
    sd.iterate();
    time.advance(100);
  }

  EXPECT_EQ(sd.getCurrentStatus(), STATUS_REGISTERED_AND_READY);
  EXPECT_EQ(client->getRootCACert(), nullptr);
}

TEST_F(SuplaDeviceTestsFullStartupManual,
    SslEnabledNoCANoConfig) {
  net.setSSLEnabled(true);
  EXPECT_EQ(Supla::Storage::ConfigInstance(), nullptr);
  int dummy = 0;
  sd.setActivityTimeout(45);

  EXPECT_CALL(el1, onLoadConfig(_)).Times(0);
  EXPECT_CALL(el2, onLoadConfig(_)).Times(0);
  EXPECT_CALL(el1, onInit()).Times(1);
  EXPECT_CALL(el2, onInit()).Times(1);
  EXPECT_CALL(el1, onRegistered(_));
  EXPECT_CALL(el2, onRegistered(_));

  EXPECT_CALL(timer, initTimers());
  EXPECT_CALL(srpc, srpc_params_init(_));
  EXPECT_CALL(srpc, srpc_init(_)).WillOnce(Return(&dummy));
  EXPECT_CALL(srpc, srpc_set_proto_version(&dummy, 18));

  char GUID[SUPLA_GUID_SIZE] = {1};
  char AUTHKEY[SUPLA_AUTHKEY_SIZE] = {2};
  EXPECT_TRUE(sd.begin(GUID, "supla.rulez", "superman@supla.org", AUTHKEY, 18));
  EXPECT_EQ(sd.getCurrentStatus(), STATUS_INITIALIZED);

  EXPECT_CALL(net, isReady()).WillRepeatedly(Return(true));
  EXPECT_CALL(net, setup()).Times(1);
  EXPECT_CALL(net, iterate()).Times(AtLeast(1));
  EXPECT_CALL(srpc, srpc_iterate(_)).WillRepeatedly(Return(SUPLA_RESULT_TRUE));
  EXPECT_CALL(*client, stop()).Times(0);

  EXPECT_CALL(*client, connected()).WillOnce(Return(false))
    .WillRepeatedly(Return(true));
  // Port should be 2016
  EXPECT_CALL(*client, connectImp(_, 2016)).WillRepeatedly(Return(1));
  EXPECT_CALL(srpc, srpc_ds_async_registerdevice_e(_, _)).Times(1);

  EXPECT_CALL(el1, iterateAlways()).Times(AtLeast(1));
  EXPECT_CALL(el2, iterateAlways()).Times(AtLeast(1));
  EXPECT_CALL(el1, iterateConnected()).Times(AtLeast(1))
        .WillOnce(Return(false))
        .WillRepeatedly(Return(true));
  EXPECT_CALL(el2, iterateConnected()).Times(AtLeast(1));

  EXPECT_CALL(board, deviceSoftwareReset()).Times(0);
//  EXPECT_CALL(el1, onSaveState()).Times(AtLeast(1));
//  EXPECT_CALL(el2, onSaveState()).Times(AtLeast(1));

  EXPECT_EQ(sd.getCurrentStatus(), STATUS_INITIALIZED);
  for (int i = 0; i < 5; i++) {
    sd.iterate();
    time.advance(100);
  }
  EXPECT_EQ(sd.getCurrentStatus(), STATUS_REGISTER_IN_PROGRESS);

  TSD_SuplaRegisterDeviceResult register_device_result{};
  register_device_result.result_code = SUPLA_RESULTCODE_TRUE;
  register_device_result.activity_timeout = 45;
  register_device_result.version = 20;
  register_device_result.version_min = 1;

  auto srpcLayer = sd.getSrpcLayer();
  srpcLayer->onRegisterResult(&register_device_result);
  time.advance(100);

  EXPECT_EQ(sd.getCurrentStatus(), STATUS_REGISTERED_AND_READY);

  for (int i = 0; i < 10; i++) {
    sd.iterate();
    time.advance(100);
  }

  EXPECT_EQ(sd.getCurrentStatus(), STATUS_REGISTERED_AND_READY);
  EXPECT_EQ(client->getRootCACert(), nullptr);
}

TEST_F(SuplaDeviceTestsFullStartupManual,
    SslEnabledSuplaCANoConfig) {
  net.setSSLEnabled(true);
  const char myCA1[] = "test CA1";
  const char myCA2[] = "test CA2";
  sd.setSuplaCACert(myCA1);
  sd.setSupla3rdPartyCACert(myCA2);

  EXPECT_EQ(Supla::Storage::ConfigInstance(), nullptr);
  int dummy = 0;
  sd.setActivityTimeout(45);

  EXPECT_CALL(el1, onLoadConfig(_)).Times(0);
  EXPECT_CALL(el2, onLoadConfig(_)).Times(0);
  EXPECT_CALL(el1, onInit()).Times(1);
  EXPECT_CALL(el2, onInit()).Times(1);
  EXPECT_CALL(el1, onRegistered(_));
  EXPECT_CALL(el2, onRegistered(_));

  EXPECT_CALL(timer, initTimers());
  EXPECT_CALL(srpc, srpc_params_init(_));
  EXPECT_CALL(srpc, srpc_init(_)).WillOnce(Return(&dummy));
  EXPECT_CALL(srpc, srpc_set_proto_version(&dummy, 18));

  char GUID[SUPLA_GUID_SIZE] = {1};
  char AUTHKEY[SUPLA_AUTHKEY_SIZE] = {2};
  EXPECT_TRUE(sd.begin(GUID, "ok.supla.org", "superman@supla.org", AUTHKEY, 18));
  EXPECT_EQ(sd.getCurrentStatus(), STATUS_INITIALIZED);

  EXPECT_CALL(net, isReady()).WillRepeatedly(Return(true));
  EXPECT_CALL(net, setup()).Times(1);
  EXPECT_CALL(net, iterate()).Times(AtLeast(1));
  EXPECT_CALL(srpc, srpc_iterate(_)).WillRepeatedly(Return(SUPLA_RESULT_TRUE));
  EXPECT_CALL(*client, stop()).Times(0);

  EXPECT_CALL(*client, connected()).WillOnce(Return(false))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(*client, connectImp(_, 2016)).WillRepeatedly(Return(1));
  EXPECT_CALL(srpc, srpc_ds_async_registerdevice_e(_, _)).Times(1);

  EXPECT_CALL(el1, iterateAlways()).Times(AtLeast(1));
  EXPECT_CALL(el2, iterateAlways()).Times(AtLeast(1));
  EXPECT_CALL(el1, iterateConnected()).Times(AtLeast(1))
        .WillOnce(Return(false))
        .WillRepeatedly(Return(true));
  EXPECT_CALL(el2, iterateConnected()).Times(AtLeast(1));

  EXPECT_CALL(board, deviceSoftwareReset()).Times(0);
//  EXPECT_CALL(el1, onSaveState()).Times(AtLeast(1));
//  EXPECT_CALL(el2, onSaveState()).Times(AtLeast(1));

  EXPECT_EQ(sd.getCurrentStatus(), STATUS_INITIALIZED);
  for (int i = 0; i < 5; i++) {
    sd.iterate();
    time.advance(100);
  }
  EXPECT_EQ(sd.getCurrentStatus(), STATUS_REGISTER_IN_PROGRESS);

  TSD_SuplaRegisterDeviceResult register_device_result{};
  register_device_result.result_code = SUPLA_RESULTCODE_TRUE;
  register_device_result.activity_timeout = 45;
  register_device_result.version = 20;
  register_device_result.version_min = 1;

  auto srpcLayer = sd.getSrpcLayer();
  srpcLayer->onRegisterResult(&register_device_result);
  time.advance(100);

  EXPECT_EQ(sd.getCurrentStatus(), STATUS_REGISTERED_AND_READY);

  for (int i = 0; i < 10; i++) {
    sd.iterate();
    time.advance(100);
  }

  EXPECT_EQ(sd.getCurrentStatus(), STATUS_REGISTERED_AND_READY);
  EXPECT_EQ(client->getRootCACert(), myCA1);
}

TEST_F(SuplaDeviceTestsFullStartupManual,
    SslEnabledSuplaCAPrivateServerNoConfig) {
  net.setSSLEnabled(true);
  const char myCA1[] = "test CA1";
  const char myCA2[] = "test CA2";
  sd.setSuplaCACert(myCA1);
  sd.setSupla3rdPartyCACert(myCA2);

  EXPECT_EQ(Supla::Storage::ConfigInstance(), nullptr);
  int dummy = 0;
  sd.setActivityTimeout(45);

  EXPECT_CALL(el1, onLoadConfig(_)).Times(0);
  EXPECT_CALL(el2, onLoadConfig(_)).Times(0);
  EXPECT_CALL(el1, onInit()).Times(1);
  EXPECT_CALL(el2, onInit()).Times(1);
  EXPECT_CALL(el1, onRegistered(_));
  EXPECT_CALL(el2, onRegistered(_));

  EXPECT_CALL(timer, initTimers());
  EXPECT_CALL(srpc, srpc_params_init(_));
  EXPECT_CALL(srpc, srpc_init(_)).WillOnce(Return(&dummy));
  EXPECT_CALL(srpc, srpc_set_proto_version(&dummy, 18));

  char GUID[SUPLA_GUID_SIZE] = {1};
  char AUTHKEY[SUPLA_AUTHKEY_SIZE] = {2};
  EXPECT_TRUE(sd.begin(GUID, "supla.rulez", "superman@supla.priv", AUTHKEY, 18));
  EXPECT_EQ(sd.getCurrentStatus(), STATUS_INITIALIZED);

  EXPECT_CALL(net, isReady()).WillRepeatedly(Return(true));
  EXPECT_CALL(net, setup()).Times(1);
  EXPECT_CALL(net, iterate()).Times(AtLeast(1));
  EXPECT_CALL(srpc, srpc_iterate(_)).WillRepeatedly(Return(SUPLA_RESULT_TRUE));
  EXPECT_CALL(*client, stop()).Times(0);

  EXPECT_CALL(*client, connected()).WillOnce(Return(false))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(*client, connectImp(_, 2016)).WillRepeatedly(Return(1));
  EXPECT_CALL(srpc, srpc_ds_async_registerdevice_e(_, _)).Times(1);

  EXPECT_CALL(el1, iterateAlways()).Times(AtLeast(1));
  EXPECT_CALL(el2, iterateAlways()).Times(AtLeast(1));
  EXPECT_CALL(el1, iterateConnected()).Times(AtLeast(1))
        .WillOnce(Return(false))
        .WillRepeatedly(Return(true));
  EXPECT_CALL(el2, iterateConnected()).Times(AtLeast(1));

  EXPECT_CALL(board, deviceSoftwareReset()).Times(0);
//  EXPECT_CALL(el1, onSaveState()).Times(AtLeast(1));
//  EXPECT_CALL(el2, onSaveState()).Times(AtLeast(1));

  EXPECT_EQ(sd.getCurrentStatus(), STATUS_INITIALIZED);
  for (int i = 0; i < 5; i++) {
    sd.iterate();
    time.advance(100);
  }
  EXPECT_EQ(sd.getCurrentStatus(), STATUS_REGISTER_IN_PROGRESS);

  TSD_SuplaRegisterDeviceResult register_device_result{};
  register_device_result.result_code = SUPLA_RESULTCODE_TRUE;
  register_device_result.activity_timeout = 45;
  register_device_result.version = 20;
  register_device_result.version_min = 1;

  auto srpcLayer = sd.getSrpcLayer();
  srpcLayer->onRegisterResult(&register_device_result);
  time.advance(100);

  EXPECT_EQ(sd.getCurrentStatus(), STATUS_REGISTERED_AND_READY);

  for (int i = 0; i < 10; i++) {
    sd.iterate();
    time.advance(100);
  }

  EXPECT_EQ(sd.getCurrentStatus(), STATUS_REGISTERED_AND_READY);
  EXPECT_EQ(client->getRootCACert(), myCA2);
}

TEST_F(SuplaDeviceTestsFullStartupManual,
    SslEnabledOnlyOnceCASetNoConfig) {
  net.setSSLEnabled(true);
  const char myCA1[] = "test CA1";
  const char myCA2[] = "test CA2";
  sd.setSuplaCACert(myCA1);
//  sd.setSupla3rdPartyCACert(myCA2);

  EXPECT_EQ(Supla::Storage::ConfigInstance(), nullptr);
  int dummy = 0;
  sd.setActivityTimeout(45);

  EXPECT_CALL(el1, onLoadConfig(_)).Times(0);
  EXPECT_CALL(el2, onLoadConfig(_)).Times(0);
  EXPECT_CALL(el1, onInit()).Times(1);
  EXPECT_CALL(el2, onInit()).Times(1);
  EXPECT_CALL(el1, onRegistered(_));
  EXPECT_CALL(el2, onRegistered(_));

  EXPECT_CALL(timer, initTimers());
  EXPECT_CALL(srpc, srpc_params_init(_));
  EXPECT_CALL(srpc, srpc_init(_)).WillOnce(Return(&dummy));
  EXPECT_CALL(srpc, srpc_set_proto_version(&dummy, 18));

  char GUID[SUPLA_GUID_SIZE] = {1};
  char AUTHKEY[SUPLA_AUTHKEY_SIZE] = {2};
  EXPECT_TRUE(sd.begin(GUID, "supla.rulez", "superman@supla.priv", AUTHKEY, 18));
  EXPECT_EQ(sd.getCurrentStatus(), STATUS_INITIALIZED);

  EXPECT_CALL(net, isReady()).WillRepeatedly(Return(true));
  EXPECT_CALL(net, setup()).Times(1);
  EXPECT_CALL(net, iterate()).Times(AtLeast(1));
  EXPECT_CALL(srpc, srpc_iterate(_)).WillRepeatedly(Return(SUPLA_RESULT_TRUE));
  EXPECT_CALL(*client, stop()).Times(0);

  EXPECT_CALL(*client, connected()).WillOnce(Return(false))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(*client, connectImp(_, 2016)).WillRepeatedly(Return(1));
  EXPECT_CALL(srpc, srpc_ds_async_registerdevice_e(_, _)).Times(1);

  EXPECT_CALL(el1, iterateAlways()).Times(AtLeast(1));
  EXPECT_CALL(el2, iterateAlways()).Times(AtLeast(1));
  EXPECT_CALL(el1, iterateConnected()).Times(AtLeast(1))
        .WillOnce(Return(false))
        .WillRepeatedly(Return(true));
  EXPECT_CALL(el2, iterateConnected()).Times(AtLeast(1));

  EXPECT_CALL(board, deviceSoftwareReset()).Times(0);
//  EXPECT_CALL(el1, onSaveState()).Times(AtLeast(1));
//  EXPECT_CALL(el2, onSaveState()).Times(AtLeast(1));

  EXPECT_EQ(sd.getCurrentStatus(), STATUS_INITIALIZED);
  for (int i = 0; i < 5; i++) {
    sd.iterate();
    time.advance(100);
  }
  EXPECT_EQ(sd.getCurrentStatus(), STATUS_REGISTER_IN_PROGRESS);

  TSD_SuplaRegisterDeviceResult register_device_result{};
  register_device_result.result_code = SUPLA_RESULTCODE_TRUE;
  register_device_result.activity_timeout = 45;
  register_device_result.version = 20;
  register_device_result.version_min = 1;

  auto srpcLayer = sd.getSrpcLayer();
  srpcLayer->onRegisterResult(&register_device_result);
  time.advance(100);

  EXPECT_EQ(sd.getCurrentStatus(), STATUS_REGISTERED_AND_READY);

  for (int i = 0; i < 10; i++) {
    sd.iterate();
    time.advance(100);
  }

  EXPECT_EQ(sd.getCurrentStatus(), STATUS_REGISTERED_AND_READY);
  EXPECT_EQ(client->getRootCACert(), myCA1);
}

TEST_F(SuplaDeviceTestsFullStartupManual,
    SslEnabledOnlyOnceCASetv2NoConfig) {
  net.setSSLEnabled(true);
  const char myCA1[] = "test CA1";
  const char myCA2[] = "test CA2";
//  sd.setSuplaCACert(myCA1);
  sd.setSupla3rdPartyCACert(myCA2);

  EXPECT_EQ(Supla::Storage::ConfigInstance(), nullptr);
  int dummy = 0;
  sd.setActivityTimeout(45);

  EXPECT_CALL(el1, onLoadConfig(_)).Times(0);
  EXPECT_CALL(el2, onLoadConfig(_)).Times(0);
  EXPECT_CALL(el1, onInit()).Times(1);
  EXPECT_CALL(el2, onInit()).Times(1);
  EXPECT_CALL(el1, onRegistered(_));
  EXPECT_CALL(el2, onRegistered(_));

  EXPECT_CALL(timer, initTimers());
  EXPECT_CALL(srpc, srpc_params_init(_));
  EXPECT_CALL(srpc, srpc_init(_)).WillOnce(Return(&dummy));
  EXPECT_CALL(srpc, srpc_set_proto_version(&dummy, 18));

  char GUID[SUPLA_GUID_SIZE] = {1};
  char AUTHKEY[SUPLA_AUTHKEY_SIZE] = {2};
  EXPECT_TRUE(sd.begin(GUID, "test.supla.org", "superman@supla.org", AUTHKEY, 18));
  EXPECT_EQ(sd.getCurrentStatus(), STATUS_INITIALIZED);

  EXPECT_CALL(net, isReady()).WillRepeatedly(Return(true));
  EXPECT_CALL(net, setup()).Times(1);
  EXPECT_CALL(net, iterate()).Times(AtLeast(1));
  EXPECT_CALL(srpc, srpc_iterate(_)).WillRepeatedly(Return(SUPLA_RESULT_TRUE));
  EXPECT_CALL(*client, stop()).Times(0);

  EXPECT_CALL(*client, connected()).WillOnce(Return(false))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(*client, connectImp(_, 2016)).WillRepeatedly(Return(1));
  EXPECT_CALL(srpc, srpc_ds_async_registerdevice_e(_, _)).Times(1);

  EXPECT_CALL(el1, iterateAlways()).Times(AtLeast(1));
  EXPECT_CALL(el2, iterateAlways()).Times(AtLeast(1));
  EXPECT_CALL(el1, iterateConnected()).Times(AtLeast(1))
        .WillOnce(Return(false))
        .WillRepeatedly(Return(true));
  EXPECT_CALL(el2, iterateConnected()).Times(AtLeast(1));

  EXPECT_CALL(board, deviceSoftwareReset()).Times(0);
//  EXPECT_CALL(el1, onSaveState()).Times(AtLeast(1));
//  EXPECT_CALL(el2, onSaveState()).Times(AtLeast(1));

  EXPECT_EQ(sd.getCurrentStatus(), STATUS_INITIALIZED);
  for (int i = 0; i < 5; i++) {
    sd.iterate();
    time.advance(100);
  }
  EXPECT_EQ(sd.getCurrentStatus(), STATUS_REGISTER_IN_PROGRESS);

  TSD_SuplaRegisterDeviceResult register_device_result{};
  register_device_result.result_code = SUPLA_RESULTCODE_TRUE;
  register_device_result.activity_timeout = 45;
  register_device_result.version = 20;
  register_device_result.version_min = 1;

  auto srpcLayer = sd.getSrpcLayer();
  srpcLayer->onRegisterResult(&register_device_result);
  time.advance(100);

  EXPECT_EQ(sd.getCurrentStatus(), STATUS_REGISTERED_AND_READY);

  for (int i = 0; i < 10; i++) {
    sd.iterate();
    time.advance(100);
  }

  EXPECT_EQ(sd.getCurrentStatus(), STATUS_REGISTERED_AND_READY);
  EXPECT_EQ(client->getRootCACert(), myCA2);
}

TEST_F(SuplaDeviceElementWithSecondaryChannel, SuccessfulStartup) {
  bool isConnected = false;
  EXPECT_CALL(net, isReady()).WillRepeatedly(Return(true));
  EXPECT_CALL(*client, connected()).WillRepeatedly(ReturnPointee(&isConnected));
  EXPECT_CALL(*client, connectImp(_, _)).WillRepeatedly(DoAll(Assign(&isConnected, true), Return(1)));

  EXPECT_CALL(net, setup()).Times(1);
  EXPECT_CALL(net, iterate()).Times(AtLeast(1));
  EXPECT_CALL(srpc, srpc_iterate(_)).WillRepeatedly(Return(SUPLA_RESULT_TRUE));
  EXPECT_CALL(srpc, srpc_params_init(_)).Times(AtLeast(1));
  int dummy;
  EXPECT_CALL(srpc, srpc_init(_)).WillRepeatedly(Return(&dummy));
  EXPECT_CALL(srpc, srpc_set_proto_version(&dummy, defaultProtoVersion)).Times(AtLeast(1));
  EXPECT_CALL(srpc, srpc_free(_)).Times(0);

  EXPECT_CALL(el1, iterateAlways()).Times(35);
  EXPECT_CALL(el2, iterateAlways()).Times(35);

  EXPECT_CALL(el1, onRegistered(_));
  EXPECT_CALL(el2, onRegistered(_));

  EXPECT_CALL(srpc, srpc_ds_async_registerdevice_e(_, _)).Times(1);
  EXPECT_CALL(srpc, srpc_dcs_async_set_activity_timeout(_, _)).Times(1);
  EXPECT_CALL(srpc, srpc_dcs_async_ping_server(_)).Times(2);

//  EXPECT_CALL(net, ping(_)).WillRepeatedly(Return(true));
  EXPECT_CALL(el1, iterateConnected()).Times(30).WillRepeatedly(Return(true));
  EXPECT_CALL(el2, iterateConnected()).Times(30).WillRepeatedly(Return(true));

  // ThermHygroPress meter has two channels and it will send valueChanged
  EXPECT_CALL(srpc, valueChanged(_, 0, _, 0, 0));
  EXPECT_CALL(srpc, valueChanged(_, 1, _, 0, 0));

  EXPECT_EQ(sd.getCurrentStatus(), STATUS_INITIALIZED);
  for (int i = 0; i < 5; i++) {
    sd.iterate();
    time.advance(1000);
  }
  EXPECT_EQ(sd.getCurrentStatus(), STATUS_REGISTER_IN_PROGRESS);

  TSD_SuplaRegisterDeviceResult register_device_result{};
  register_device_result.result_code = SUPLA_RESULTCODE_TRUE;
  register_device_result.activity_timeout = 45;
  register_device_result.version = 20;
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

TEST_F(SuplaDeviceElementWithSecondaryChannel, SleepingChannel) {
  bool isConnected = false;
  EXPECT_CALL(net, isReady()).WillRepeatedly(Return(true));
  EXPECT_CALL(*client, connected()).WillRepeatedly(ReturnPointee(&isConnected));
  EXPECT_CALL(*client, connectImp(_, _)).WillRepeatedly(DoAll(Assign(&isConnected, true), Return(1)));

  EXPECT_CALL(net, setup()).Times(1);
  EXPECT_CALL(net, iterate()).Times(AtLeast(1));
  EXPECT_CALL(srpc, srpc_iterate(_)).WillRepeatedly(Return(SUPLA_RESULT_TRUE));
  EXPECT_CALL(srpc, srpc_params_init(_)).Times(AtLeast(1));
  int dummy;
  EXPECT_CALL(srpc, srpc_init(_)).WillRepeatedly(Return(&dummy));
  EXPECT_CALL(srpc, srpc_set_proto_version(&dummy, defaultProtoVersion)).Times(AtLeast(1));
  EXPECT_CALL(srpc, srpc_free(_)).Times(0);

  EXPECT_CALL(el1, iterateAlways()).Times(35);
  EXPECT_CALL(el2, iterateAlways()).Times(35);

  EXPECT_CALL(el1, onRegistered(_));
  EXPECT_CALL(el2, onRegistered(_));

  EXPECT_CALL(srpc, srpc_ds_async_registerdevice_e(_, _)).Times(1);
  EXPECT_CALL(srpc, srpc_dcs_async_set_activity_timeout(_, _)).Times(1);
  EXPECT_CALL(srpc, srpc_dcs_async_ping_server(_)).Times(2);

//  EXPECT_CALL(net, ping(_)).WillRepeatedly(Return(true));
  EXPECT_CALL(el1, iterateConnected()).Times(30).WillRepeatedly(Return(true));
  EXPECT_CALL(el2, iterateConnected()).Times(30).WillRepeatedly(Return(true));

  // ThermHygroPress meter has two channels and it will send valueChanged
  EXPECT_CALL(srpc, valueChanged(_, 0, _, 0, 0));
  EXPECT_CALL(srpc, valueChanged(_, 1, _, 0, 0));

  EXPECT_EQ(sd.getCurrentStatus(), STATUS_INITIALIZED);
  for (int i = 0; i < 5; i++) {
    sd.iterate();
    time.advance(1000);
  }
  EXPECT_EQ(sd.getCurrentStatus(), STATUS_REGISTER_IN_PROGRESS);

  TSD_SuplaRegisterDeviceResult register_device_result{};
  register_device_result.result_code = SUPLA_RESULTCODE_TRUE;
  register_device_result.activity_timeout = 45;
  register_device_result.version = 20;
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

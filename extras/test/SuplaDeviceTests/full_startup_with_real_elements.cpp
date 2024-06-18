/*
   Copyright (C) AC SOFTWARE SP. Z O.O

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
#include <supla/sensor/virtual_binary.h>
#include <supla/sensor/virtual_thermometer.h>

using ::testing::_;
using ::testing::Assign;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::ReturnPointee;
using ::testing::AtLeast;
using ::testing::ElementsAreArray;

class FullStartupWithRealElements : public ::testing::Test {
 protected:
  SrpcMock srpc;
  NetworkMock net;
  TimerMock timer;
  SimpleTime time;
  SuplaDeviceClass sd;
  BoardMock board;
  NetworkClientMock *client = nullptr;

  virtual void SetUp() {
    Supla::Channel::resetToDefaults();

    client = new NetworkClientMock;  // it will be destroyed in
                                     // Supla::Protocol::SuplaSrpc
  }
  virtual void TearDown() {
    Supla::Channel::resetToDefaults();
  }
};

TEST_F(FullStartupWithRealElements, SleepingBinarySensor) {
  int dummy;

  Supla::Sensor::VirtualBinary binarySensor;
  Supla::Sensor::VirtualThermometer thermometer;
  int validityTimeSec = 10;
  binarySensor.getChannel()->setValidityTimeSec(validityTimeSec);

  EXPECT_CALL(timer, initTimers());
  EXPECT_CALL(srpc, srpc_params_init(_));
  EXPECT_CALL(srpc, srpc_init(_)).WillOnce(Return(&dummy));
  EXPECT_CALL(srpc, srpc_set_proto_version(&dummy, 23));

  char GUID[SUPLA_GUID_SIZE] = {1};
  char AUTHKEY[SUPLA_AUTHKEY_SIZE] = {2};
  EXPECT_TRUE(sd.begin(GUID, "supla.rulez", "superman@supla.org", AUTHKEY, 18));
  EXPECT_EQ(sd.getCurrentStatus(), STATUS_INITIALIZED);

  EXPECT_CALL(net, isReady()).WillRepeatedly(Return(true));

  bool isConnected = false;
  EXPECT_CALL(*client, connected()).WillRepeatedly(ReturnPointee(&isConnected));
  EXPECT_CALL(*client, connectImp(_, _))
      .WillRepeatedly(DoAll(Assign(&isConnected, true), Return(1)));

  EXPECT_CALL(net, setup()).Times(1);
  EXPECT_CALL(net, iterate()).Times(AtLeast(1));
  EXPECT_CALL(srpc, srpc_iterate(_)).WillRepeatedly(Return(SUPLA_RESULT_TRUE));

  EXPECT_CALL(srpc, srpc_ds_async_registerdevice_in_chunks(_, _)).Times(1);
  EXPECT_CALL(srpc, srpc_dcs_async_set_activity_timeout(_, _)).Times(1);
  EXPECT_CALL(srpc, srpc_dcs_async_ping_server(_)).Times(2);

  char value[SUPLA_CHANNELVALUE_SIZE] = {};
  value[0] = 0;

  // for binary sensor: channel value is send only if channel is sleeping
  EXPECT_CALL(srpc,
              valueChanged(_, 0, ElementsAreArray(value), 0, validityTimeSec))
      .Times(1);
  // expectation for thermomter (channel value is always send)
  EXPECT_CALL(srpc,
              valueChanged(_, 1, _, 0, 0))
      .Times(1);
  EXPECT_CALL(srpc, srpc_csd_async_channel_state_result(_, _)).Times(1);

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

TEST_F(FullStartupWithRealElements, SleepingThermometer) {
  int dummy;

  Supla::Sensor::VirtualBinary binarySensor;
  Supla::Sensor::VirtualThermometer thermometer;
  int validityTimeSec = 10;
  thermometer.getChannel()->setValidityTimeSec(validityTimeSec);

  EXPECT_CALL(timer, initTimers());
  EXPECT_CALL(srpc, srpc_params_init(_));
  EXPECT_CALL(srpc, srpc_init(_)).WillOnce(Return(&dummy));
  EXPECT_CALL(srpc, srpc_set_proto_version(&dummy, 23));

  char GUID[SUPLA_GUID_SIZE] = {1};
  char AUTHKEY[SUPLA_AUTHKEY_SIZE] = {2};
  EXPECT_TRUE(sd.begin(GUID, "supla.rulez", "superman@supla.org", AUTHKEY, 18));
  EXPECT_EQ(sd.getCurrentStatus(), STATUS_INITIALIZED);

  EXPECT_CALL(net, isReady()).WillRepeatedly(Return(true));

  bool isConnected = false;
  EXPECT_CALL(*client, connected()).WillRepeatedly(ReturnPointee(&isConnected));
  EXPECT_CALL(*client, connectImp(_, _))
      .WillRepeatedly(DoAll(Assign(&isConnected, true), Return(1)));

  EXPECT_CALL(net, setup()).Times(1);
  EXPECT_CALL(net, iterate()).Times(AtLeast(1));
  EXPECT_CALL(srpc, srpc_iterate(_)).WillRepeatedly(Return(SUPLA_RESULT_TRUE));

  EXPECT_CALL(srpc, srpc_ds_async_registerdevice_in_chunks(_, _)).Times(1);
  EXPECT_CALL(srpc, srpc_dcs_async_set_activity_timeout(_, _)).Times(1);
  EXPECT_CALL(srpc, srpc_dcs_async_ping_server(_)).Times(2);

  char value[SUPLA_CHANNELVALUE_SIZE] = {};
  value[0] = 0;

  // for binary sensor: channel value is not send
  EXPECT_CALL(srpc,
              valueChanged(_, 0, ElementsAreArray(value), 0, 0))
      .Times(0);
  // expectation for thermomter: channel value is send with validity time sec
  EXPECT_CALL(srpc,
              valueChanged(_, 1, _, 0, validityTimeSec))
      .Times(1);
  EXPECT_CALL(srpc, srpc_csd_async_channel_state_result(_, _)).Times(1);

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

TEST_F(FullStartupWithRealElements, SleepingThermometerAndBinarySensor) {
  int dummy;

  Supla::Sensor::VirtualBinary binarySensor;
  Supla::Sensor::VirtualThermometer thermometer;
  int validityTimeSec = 10;
  int validityTimeSec2 = 20;
  thermometer.getChannel()->setValidityTimeSec(validityTimeSec);
  binarySensor.getChannel()->setValidityTimeSec(validityTimeSec2);

  EXPECT_CALL(timer, initTimers());
  EXPECT_CALL(srpc, srpc_params_init(_));
  EXPECT_CALL(srpc, srpc_init(_)).WillOnce(Return(&dummy));
  EXPECT_CALL(srpc, srpc_set_proto_version(&dummy, 23));

  char GUID[SUPLA_GUID_SIZE] = {1};
  char AUTHKEY[SUPLA_AUTHKEY_SIZE] = {2};
  EXPECT_TRUE(sd.begin(GUID, "supla.rulez", "superman@supla.org", AUTHKEY, 18));
  EXPECT_EQ(sd.getCurrentStatus(), STATUS_INITIALIZED);

  EXPECT_CALL(net, isReady()).WillRepeatedly(Return(true));

  bool isConnected = false;
  EXPECT_CALL(*client, connected()).WillRepeatedly(ReturnPointee(&isConnected));
  EXPECT_CALL(*client, connectImp(_, _))
      .WillRepeatedly(DoAll(Assign(&isConnected, true), Return(1)));

  EXPECT_CALL(net, setup()).Times(1);
  EXPECT_CALL(net, iterate()).Times(AtLeast(1));
  EXPECT_CALL(srpc, srpc_iterate(_)).WillRepeatedly(Return(SUPLA_RESULT_TRUE));

  EXPECT_CALL(srpc, srpc_ds_async_registerdevice_in_chunks(_, _)).Times(1);
  EXPECT_CALL(srpc, srpc_dcs_async_set_activity_timeout(_, _)).Times(1);
  EXPECT_CALL(srpc, srpc_dcs_async_ping_server(_)).Times(2);

  char value[SUPLA_CHANNELVALUE_SIZE] = {};
  value[0] = 0;

  // for binary sensor: channel value is send
  EXPECT_CALL(srpc,
              valueChanged(_, 0, ElementsAreArray(value), 0, validityTimeSec2))
      .Times(1);
  // expectation for thermomter: channel value is send with validity time sec
  EXPECT_CALL(srpc,
              valueChanged(_, 1, _, 0, validityTimeSec))
      .Times(1);
  // channel state should be send 2x because we have 2 sleeping channels
  EXPECT_CALL(srpc, srpc_csd_async_channel_state_result(_, _)).Times(2);

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


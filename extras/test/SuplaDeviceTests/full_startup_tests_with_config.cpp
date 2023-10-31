/*
 * Copyright (C) AC SOFTWARE SP. Z O.O
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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
#include "supla/protocol/supla_srpc.h"
#include <network_client_mock.h>
#include <config_mock.h>
#include <supla/sensor/therm_hygro_press_meter.h>
#include <simple_time.h>
#include <network_mock.h>
#include <string>
#include <mqtt_mock.h>

using ::testing::Return;
using ::testing::_;
using ::testing::AtLeast;
using ::testing::InSequence;

class DeviceStatusInterface {
  public:
    DeviceStatusInterface() {
      deviceStatusPtr = this;
    }
    virtual ~DeviceStatusInterface() {
      deviceStatusPtr = nullptr;
    }

    virtual void status(int status, std::string msg) = 0;

    static DeviceStatusInterface *deviceStatusPtr;
};

class DeviceStatusMock : public DeviceStatusInterface {
  public:
    MOCK_METHOD(void, status, (int status, std::string msg), (override));
};

DeviceStatusInterface *DeviceStatusInterface::deviceStatusPtr = nullptr;

void statusImpl(int status, const char *msg) {
  ASSERT_NE(DeviceStatusInterface::deviceStatusPtr, nullptr);
  DeviceStatusInterface::deviceStatusPtr->status(status, std::string(msg));
}

const char myCA1[] = "test CA1";
const char myCA2[] = "test CA2";

class FullStartupWithConfig : public ::testing::Test {
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
    DeviceStatusMock statusMock;
    ConfigMock cfg;

    virtual void SetUp() {
      client = new NetworkClientMock;  // it will be destroyed in
                                       // Supla::Protocol::SuplaSrpc
      sd.setStatusFuncImpl(statusImpl);
      Supla::Channel::lastCommunicationTimeMs = 0;
      memset(&(Supla::Channel::reg_dev), 0, sizeof(Supla::Channel::reg_dev));
      sd.setSuplaCACert(myCA1);
      sd.setSupla3rdPartyCACert(myCA2);
      sd.setActivityTimeout(45);
      EXPECT_CALL(cfg, init()).WillOnce(Return(true));
      EXPECT_CALL(cfg, isConfigModeSupported()).WillRepeatedly(Return(true));
      EXPECT_CALL(net, isWifiConfigRequired()).WillRepeatedly(Return(true));
    }

    virtual void TearDown() {
      Supla::Channel::lastCommunicationTimeMs = 0;
      memset(&(Supla::Channel::reg_dev), 0, sizeof(Supla::Channel::reg_dev));
    }
};

TEST_F(FullStartupWithConfig, WithConfigSslDisabled) {
  int dummy = 0;
  {
    InSequence s;
    EXPECT_CALL(statusMock, status(STATUS_INITIALIZED, _)).Times(1);
    EXPECT_CALL(statusMock, status(STATUS_REGISTER_IN_PROGRESS, _)).Times(1);
    EXPECT_CALL(statusMock, status(STATUS_REGISTERED_AND_READY, _)).Times(1);
  }

  EXPECT_CALL(cfg, getDeviceName(_)).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getGUID(_)).WillRepeatedly([] (char *guid) {
      char GUID[SUPLA_GUID_SIZE] = {1};
      memcpy(guid, GUID, SUPLA_GUID_SIZE);
      return true;
    });
  EXPECT_CALL(cfg, getAuthKey(_)).WillRepeatedly([] (char *auth) {
      char AUTHKEY[SUPLA_AUTHKEY_SIZE] = {2};
      memcpy(auth, AUTHKEY, SUPLA_AUTHKEY_SIZE);
      return true;
    });
  EXPECT_CALL(cfg, getDeviceMode())
    .WillRepeatedly(Return(Supla::DEVICE_MODE_NOT_SET));
  EXPECT_CALL(cfg, getSuplaServer(_)).WillRepeatedly([] (char *server) {
      char temp[] = "mega.supla.org";
      memcpy(server, temp, strlen(temp));
      return true;
    });
  EXPECT_CALL(cfg, getEmail(_)).WillRepeatedly([] (char *mail) {
      char temp[] = "ceo@supla.org";
      memcpy(mail, temp, strlen(temp));
      return true;
    });
  // ssl disabled in config is realized by setting port to 2015
  EXPECT_CALL(cfg, getSuplaServerPort()).WillRepeatedly(Return(2015));
  EXPECT_CALL(cfg, saveIfNeeded()).Times(AtLeast(1));
  EXPECT_CALL(cfg, getUInt8(_, _)).WillRepeatedly([] (const char *key,
        uint8_t *buf) {
      if (strcmp(key, "security_level") == 0) {
        *buf = 0;
        return true;
      }
      return false;
    });
  EXPECT_CALL(cfg, getWiFiSSID(_)).WillRepeatedly([] (char *ssid) {
      char temp[] = "ssid_test";
      memcpy(ssid, temp, strlen(temp));
      return true;
    });
  EXPECT_CALL(cfg, getWiFiPassword(_)).WillRepeatedly([] (char *pass) {
      char temp[] = "pass_test";
      memcpy(pass, temp, strlen(temp));
      return true;
    });

  EXPECT_CALL(cfg, isSuplaCommProtocolEnabled()).WillRepeatedly(Return(true));

  EXPECT_CALL(el1, onLoadConfig(_)).Times(1);
  EXPECT_CALL(el2, onLoadConfig(_)).Times(1);
  EXPECT_CALL(el1, onInit()).Times(1);
  EXPECT_CALL(el2, onInit()).Times(1);
  EXPECT_CALL(el1, onRegistered(_));
  EXPECT_CALL(el2, onRegistered(_));

  EXPECT_CALL(timer, initTimers());
  EXPECT_CALL(srpc, srpc_params_init(_));
  EXPECT_CALL(srpc, srpc_init(_)).WillOnce(Return(&dummy));
  EXPECT_CALL(srpc, srpc_set_proto_version(&dummy, 18));

  EXPECT_TRUE(sd.begin(18));
  EXPECT_EQ(sd.getCurrentStatus(), STATUS_INITIALIZED);

  EXPECT_CALL(net, isReady()).WillRepeatedly(Return(true));
  EXPECT_CALL(net, setup()).Times(1);
  EXPECT_CALL(net, iterate()).Times(AtLeast(1));
  EXPECT_CALL(srpc, srpc_iterate(_)).WillRepeatedly(Return(SUPLA_RESULT_TRUE));
  EXPECT_CALL(*client, stop()).Times(0);

  EXPECT_CALL(*client, connected()).WillOnce(Return(false))
    .WillRepeatedly(Return(true));
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

TEST_F(FullStartupWithConfig,
    WithConfigSslEnabledManually) {
  int dummy = 0;
  {
    InSequence s;
    EXPECT_CALL(statusMock, status(STATUS_INITIALIZED, _)).Times(1);
    EXPECT_CALL(statusMock, status(STATUS_REGISTER_IN_PROGRESS, _)).Times(1);
    EXPECT_CALL(statusMock, status(STATUS_REGISTERED_AND_READY, _)).Times(1);
  }

  EXPECT_CALL(cfg, getDeviceName(_)).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getGUID(_)).WillRepeatedly([] (char *guid) {
      char GUID[SUPLA_GUID_SIZE] = {1};
      memcpy(guid, GUID, SUPLA_GUID_SIZE);
      return true;
    });
  EXPECT_CALL(cfg, getAuthKey(_)).WillRepeatedly([] (char *auth) {
      char AUTHKEY[SUPLA_AUTHKEY_SIZE] = {2};
      memcpy(auth, AUTHKEY, SUPLA_AUTHKEY_SIZE);
      return true;
    });
  EXPECT_CALL(cfg, getDeviceMode())
    .WillRepeatedly(Return(Supla::DEVICE_MODE_NOT_SET));
  EXPECT_CALL(cfg, getSuplaServer(_)).WillRepeatedly([] (char *server) {
      char temp[] = "mega.supla.org";
      memcpy(server, temp, strlen(temp));
      return true;
    });
  EXPECT_CALL(cfg, getEmail(_)).WillRepeatedly([] (char *mail) {
      char temp[] = "ceo@supla.org";
      memcpy(mail, temp, strlen(temp));
      return true;
    });
  // ssl enabled in config is realized by setting port to 2016
  EXPECT_CALL(cfg, getSuplaServerPort()).WillRepeatedly(Return(2016));
  EXPECT_CALL(cfg, saveIfNeeded()).Times(AtLeast(1));
  EXPECT_CALL(cfg, getUInt8(_, _)).WillRepeatedly([] (const char *key,
        uint8_t *buf) {
      if (strcmp(key, "security_level") == 0) {
        *buf = 0;
        return true;
      }
      return false;
    });
  EXPECT_CALL(cfg, getWiFiSSID(_)).WillRepeatedly([] (char *ssid) {
      char temp[] = "ssid_test";
      memcpy(ssid, temp, strlen(temp));
      return true;
    });
  EXPECT_CALL(cfg, getWiFiPassword(_)).WillRepeatedly([] (char *pass) {
      char temp[] = "pass_test";
      memcpy(pass, temp, strlen(temp));
      return true;
    });

  EXPECT_CALL(cfg, isSuplaCommProtocolEnabled()).WillRepeatedly(Return(true));


  EXPECT_CALL(el1, onLoadConfig(_)).Times(1);
  EXPECT_CALL(el2, onLoadConfig(_)).Times(1);
  EXPECT_CALL(el1, onInit()).Times(1);
  EXPECT_CALL(el2, onInit()).Times(1);
  EXPECT_CALL(el1, onRegistered(_));
  EXPECT_CALL(el2, onRegistered(_));

  EXPECT_CALL(timer, initTimers());
  EXPECT_CALL(srpc, srpc_params_init(_));
  EXPECT_CALL(srpc, srpc_init(_)).WillOnce(Return(&dummy));
  EXPECT_CALL(srpc, srpc_set_proto_version(&dummy, 18));

  EXPECT_TRUE(sd.begin(18));
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

TEST_F(FullStartupWithConfig,
    WithConfigSslEnabled) {
  int dummy = 0;
  {
    InSequence s;
    EXPECT_CALL(statusMock, status(STATUS_INITIALIZED, _)).Times(1);
    EXPECT_CALL(statusMock, status(STATUS_REGISTER_IN_PROGRESS, _)).Times(1);
    EXPECT_CALL(statusMock, status(STATUS_REGISTERED_AND_READY, _)).Times(1);
  }

  EXPECT_CALL(cfg, getDeviceName(_)).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getGUID(_)).WillRepeatedly([] (char *guid) {
      char GUID[SUPLA_GUID_SIZE] = {1};
      memcpy(guid, GUID, SUPLA_GUID_SIZE);
      return true;
    });
  EXPECT_CALL(cfg, getAuthKey(_)).WillRepeatedly([] (char *auth) {
      char AUTHKEY[SUPLA_AUTHKEY_SIZE] = {2};
      memcpy(auth, AUTHKEY, SUPLA_AUTHKEY_SIZE);
      return true;
    });
  EXPECT_CALL(cfg, getDeviceMode())
    .WillRepeatedly(Return(Supla::DEVICE_MODE_NOT_SET));
  EXPECT_CALL(cfg, getSuplaServer(_)).WillRepeatedly([] (char *server) {
      char temp[] = "mega.supla.org";
      memcpy(server, temp, strlen(temp));
      return true;
    });
  EXPECT_CALL(cfg, getEmail(_)).WillRepeatedly([] (char *mail) {
      char temp[] = "ceo@supla.org";
      memcpy(mail, temp, strlen(temp));
      return true;
    });
  // ssl enabled in config is realized by setting port to -1 (auto/default)
  EXPECT_CALL(cfg, getSuplaServerPort()).WillRepeatedly(Return(-1));
  EXPECT_CALL(cfg, saveIfNeeded()).Times(AtLeast(1));
  EXPECT_CALL(cfg, getUInt8(_, _)).WillRepeatedly([] (const char *key,
        uint8_t *buf) {
      if (strcmp(key, "security_level") == 0) {
        *buf = 0;
        return true;
      }
      return false;
    });
  EXPECT_CALL(cfg, getWiFiSSID(_)).WillRepeatedly([] (char *ssid) {
      char temp[] = "ssid_test";
      memcpy(ssid, temp, strlen(temp));
      return true;
    });
  EXPECT_CALL(cfg, getWiFiPassword(_)).WillRepeatedly([] (char *pass) {
      char temp[] = "pass_test";
      memcpy(pass, temp, strlen(temp));
      return true;
    });


  EXPECT_CALL(cfg, isSuplaCommProtocolEnabled()).WillRepeatedly(Return(true));


  EXPECT_CALL(el1, onLoadConfig(_)).Times(1);
  EXPECT_CALL(el2, onLoadConfig(_)).Times(1);
  EXPECT_CALL(el1, onInit()).Times(1);
  EXPECT_CALL(el2, onInit()).Times(1);
  EXPECT_CALL(el1, onRegistered(_));
  EXPECT_CALL(el2, onRegistered(_));

  EXPECT_CALL(timer, initTimers());
  EXPECT_CALL(srpc, srpc_params_init(_));
  EXPECT_CALL(srpc, srpc_init(_)).WillOnce(Return(&dummy));
  EXPECT_CALL(srpc, srpc_set_proto_version(&dummy, 18));

  EXPECT_TRUE(sd.begin(18));
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

TEST_F(FullStartupWithConfig,
    WithConfigSslEnabledPrivateServer) {
  int dummy = 0;
  {
    InSequence s;
    EXPECT_CALL(statusMock, status(STATUS_INITIALIZED, _)).Times(1);
    EXPECT_CALL(statusMock, status(STATUS_REGISTER_IN_PROGRESS, _)).Times(1);
    EXPECT_CALL(statusMock, status(STATUS_REGISTERED_AND_READY, _)).Times(1);
  }

  EXPECT_CALL(cfg, getDeviceName(_)).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getGUID(_)).WillRepeatedly([] (char *guid) {
      char GUID[SUPLA_GUID_SIZE] = {1};
      memcpy(guid, GUID, SUPLA_GUID_SIZE);
      return true;
    });
  EXPECT_CALL(cfg, getAuthKey(_)).WillRepeatedly([] (char *auth) {
      char AUTHKEY[SUPLA_AUTHKEY_SIZE] = {2};
      memcpy(auth, AUTHKEY, SUPLA_AUTHKEY_SIZE);
      return true;
    });
  EXPECT_CALL(cfg, getDeviceMode())
    .WillRepeatedly(Return(Supla::DEVICE_MODE_NOT_SET));
  EXPECT_CALL(cfg, getSuplaServer(_)).WillRepeatedly([] (char *server) {
      char temp[] = "mega.supla.priv";
      memcpy(server, temp, strlen(temp));
      return true;
    });
  EXPECT_CALL(cfg, getEmail(_)).WillRepeatedly([] (char *mail) {
      char temp[] = "ceo@supla.org";
      memcpy(mail, temp, strlen(temp));
      return true;
    });
  // ssl enabled in config is realized by setting port to -1 (auto/default)
  EXPECT_CALL(cfg, getSuplaServerPort()).WillRepeatedly(Return(-1));
  EXPECT_CALL(cfg, saveIfNeeded()).Times(AtLeast(1));
  EXPECT_CALL(cfg, getUInt8(_, _)).WillRepeatedly([] (const char *key,
        uint8_t *buf) {
      if (strcmp(key, "security_level") == 0) {
        *buf = 0;
        return true;
      }
      return false;
    });
  EXPECT_CALL(cfg, getWiFiSSID(_)).WillRepeatedly([] (char *ssid) {
      char temp[] = "ssid_test";
      memcpy(ssid, temp, strlen(temp));
      return true;
    });
  EXPECT_CALL(cfg, getWiFiPassword(_)).WillRepeatedly([] (char *pass) {
      char temp[] = "pass_test";
      memcpy(pass, temp, strlen(temp));
      return true;
    });


  EXPECT_CALL(cfg, isSuplaCommProtocolEnabled()).WillRepeatedly(Return(true));


  EXPECT_CALL(el1, onLoadConfig(_)).Times(1);
  EXPECT_CALL(el2, onLoadConfig(_)).Times(1);
  EXPECT_CALL(el1, onInit()).Times(1);
  EXPECT_CALL(el2, onInit()).Times(1);
  EXPECT_CALL(el1, onRegistered(_));
  EXPECT_CALL(el2, onRegistered(_));

  EXPECT_CALL(timer, initTimers());
  EXPECT_CALL(srpc, srpc_params_init(_));
  EXPECT_CALL(srpc, srpc_init(_)).WillOnce(Return(&dummy));
  EXPECT_CALL(srpc, srpc_set_proto_version(&dummy, 18));

  EXPECT_TRUE(sd.begin(18));
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

TEST_F(FullStartupWithConfig,
    WithConfigSslEnabledCustomCA) {
  int dummy = 0;
  {
    InSequence s;
    EXPECT_CALL(statusMock, status(STATUS_INITIALIZED, _)).Times(1);
    EXPECT_CALL(statusMock, status(STATUS_REGISTER_IN_PROGRESS, _)).Times(1);
    EXPECT_CALL(statusMock, status(STATUS_REGISTERED_AND_READY, _)).Times(1);
  }

  EXPECT_CALL(cfg, getDeviceName(_)).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getGUID(_)).WillRepeatedly([] (char *guid) {
      char GUID[SUPLA_GUID_SIZE] = {1};
      memcpy(guid, GUID, SUPLA_GUID_SIZE);
      return true;
    });
  EXPECT_CALL(cfg, getAuthKey(_)).WillRepeatedly([] (char *auth) {
      char AUTHKEY[SUPLA_AUTHKEY_SIZE] = {2};
      memcpy(auth, AUTHKEY, SUPLA_AUTHKEY_SIZE);
      return true;
    });
  EXPECT_CALL(cfg, getDeviceMode())
    .WillRepeatedly(Return(Supla::DEVICE_MODE_NOT_SET));
  EXPECT_CALL(cfg, getSuplaServer(_)).WillRepeatedly([] (char *server) {
      char temp[] = "mega.supla.org";
      memcpy(server, temp, strlen(temp));
      return true;
    });
  EXPECT_CALL(cfg, getEmail(_)).WillRepeatedly([] (char *mail) {
      char temp[] = "ceo@supla.org";
      memcpy(mail, temp, strlen(temp));
      return true;
    });
  // ssl enabled in config is realized by setting port to -1 (auto/default)
  EXPECT_CALL(cfg, getSuplaServerPort()).WillRepeatedly(Return(-1));
  EXPECT_CALL(cfg, saveIfNeeded()).Times(AtLeast(1));
  EXPECT_CALL(cfg, getUInt8(_, _)).WillRepeatedly([] (const char *key,
        uint8_t *buf) {
      if (strcmp(key, "security_level") == 0) {
        *buf = 1;
        return true;
      }
      EXPECT_TRUE(false);
      return false;
    });
  EXPECT_CALL(cfg, getCustomCASize())
    .WillRepeatedly(Return(strlen("custom ca")));
  EXPECT_CALL(cfg, getCustomCA(_, _)).WillRepeatedly(
    [] (char *ca, int maxSize) {
      char temp[] = "custom ca";
      EXPECT_GE(maxSize, strlen(temp));
      memcpy(ca, temp, strlen(temp));
      return true;
    });
  EXPECT_CALL(cfg, getWiFiSSID(_)).WillRepeatedly([] (char *ssid) {
      char temp[] = "ssid_test";
      memcpy(ssid, temp, strlen(temp));
      return true;
    });
  EXPECT_CALL(cfg, getWiFiPassword(_)).WillRepeatedly([] (char *pass) {
      char temp[] = "pass_test";
      memcpy(pass, temp, strlen(temp));
      return true;
    });

  EXPECT_CALL(cfg, isSuplaCommProtocolEnabled()).WillRepeatedly(Return(true));

  EXPECT_CALL(el1, onLoadConfig(_)).Times(1);
  EXPECT_CALL(el2, onLoadConfig(_)).Times(1);
  EXPECT_CALL(el1, onInit()).Times(1);
  EXPECT_CALL(el2, onInit()).Times(1);
  EXPECT_CALL(el1, onRegistered(_));
  EXPECT_CALL(el2, onRegistered(_));

  EXPECT_CALL(timer, initTimers());
  EXPECT_CALL(srpc, srpc_params_init(_));
  EXPECT_CALL(srpc, srpc_init(_)).WillOnce(Return(&dummy));
  EXPECT_CALL(srpc, srpc_set_proto_version(&dummy, 18));

  EXPECT_TRUE(sd.begin(18));
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
  EXPECT_STREQ(client->getRootCACert(), "custom ca");
}

TEST_F(FullStartupWithConfig,
    WithConfigSslEnabledCustomCASelectedButMissing) {
  int dummy = 0;
  {
    InSequence s;
    EXPECT_CALL(statusMock, status(STATUS_INITIALIZED, _)).Times(1);
    EXPECT_CALL(statusMock, status(STATUS_REGISTER_IN_PROGRESS, _)).Times(1);
    EXPECT_CALL(statusMock, status(STATUS_REGISTERED_AND_READY, _)).Times(1);
  }

  EXPECT_CALL(cfg, getDeviceName(_)).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getGUID(_)).WillRepeatedly([] (char *guid) {
      char GUID[SUPLA_GUID_SIZE] = {1};
      memcpy(guid, GUID, SUPLA_GUID_SIZE);
      return true;
    });
  EXPECT_CALL(cfg, getAuthKey(_)).WillRepeatedly([] (char *auth) {
      char AUTHKEY[SUPLA_AUTHKEY_SIZE] = {2};
      memcpy(auth, AUTHKEY, SUPLA_AUTHKEY_SIZE);
      return true;
    });
  EXPECT_CALL(cfg, getDeviceMode())
    .WillRepeatedly(Return(Supla::DEVICE_MODE_NOT_SET));
  EXPECT_CALL(cfg, getSuplaServer(_)).WillRepeatedly([] (char *server) {
      char temp[] = "mega.supla.org";
      memcpy(server, temp, strlen(temp));
      return true;
    });
  EXPECT_CALL(cfg, getEmail(_)).WillRepeatedly([] (char *mail) {
      char temp[] = "ceo@supla.org";
      memcpy(mail, temp, strlen(temp));
      return true;
    });
  // ssl enabled in config is realized by setting port to -1 (auto/default)
  EXPECT_CALL(cfg, getSuplaServerPort()).WillRepeatedly(Return(-1));
  EXPECT_CALL(cfg, saveIfNeeded()).Times(AtLeast(1));
  EXPECT_CALL(cfg, getUInt8(_, _)).WillRepeatedly([] (const char *key,
        uint8_t *buf) {
      if (strcmp(key, "security_level") == 0) {
        *buf = 1;
        return true;
      }
      EXPECT_TRUE(false);
      return false;
    });
  EXPECT_CALL(cfg, getCustomCASize())
    .WillRepeatedly(Return(0));
  EXPECT_CALL(cfg, getCustomCA(_, _)).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getWiFiSSID(_)).WillRepeatedly([] (char *ssid) {
      char temp[] = "ssid_test";
      memcpy(ssid, temp, strlen(temp));
      return true;
    });
  EXPECT_CALL(cfg, getWiFiPassword(_)).WillRepeatedly([] (char *pass) {
      char temp[] = "pass_test";
      memcpy(pass, temp, strlen(temp));
      return true;
    });

  EXPECT_CALL(cfg, isSuplaCommProtocolEnabled()).WillRepeatedly(Return(true));

  EXPECT_CALL(el1, onLoadConfig(_)).Times(1);
  EXPECT_CALL(el2, onLoadConfig(_)).Times(1);
  EXPECT_CALL(el1, onInit()).Times(1);
  EXPECT_CALL(el2, onInit()).Times(1);
  EXPECT_CALL(el1, onRegistered(_));
  EXPECT_CALL(el2, onRegistered(_));

  EXPECT_CALL(timer, initTimers());
  EXPECT_CALL(srpc, srpc_params_init(_));
  EXPECT_CALL(srpc, srpc_init(_)).WillOnce(Return(&dummy));
  EXPECT_CALL(srpc, srpc_set_proto_version(&dummy, 18));

  EXPECT_TRUE(sd.begin(18));
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
  // "SUPLA" string is set as a dummy CA value (invalid), so connection in
  // production code will fail
  EXPECT_STREQ(client->getRootCACert(), "SUPLA");
}

TEST_F(FullStartupWithConfig,
    WithConfigSslEnabledInsecure) {
  int dummy = 0;
  {
    InSequence s;
    EXPECT_CALL(statusMock, status(STATUS_INITIALIZED, _)).Times(1);
    EXPECT_CALL(statusMock, status(STATUS_REGISTER_IN_PROGRESS, _)).Times(1);
    EXPECT_CALL(statusMock, status(STATUS_REGISTERED_AND_READY, _)).Times(1);
  }

  EXPECT_CALL(cfg, getDeviceName(_)).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getGUID(_)).WillRepeatedly([] (char *guid) {
      char GUID[SUPLA_GUID_SIZE] = {1};
      memcpy(guid, GUID, SUPLA_GUID_SIZE);
      return true;
    });
  EXPECT_CALL(cfg, getAuthKey(_)).WillRepeatedly([] (char *auth) {
      char AUTHKEY[SUPLA_AUTHKEY_SIZE] = {2};
      memcpy(auth, AUTHKEY, SUPLA_AUTHKEY_SIZE);
      return true;
    });
  EXPECT_CALL(cfg, getDeviceMode())
    .WillRepeatedly(Return(Supla::DEVICE_MODE_NOT_SET));
  EXPECT_CALL(cfg, getSuplaServer(_)).WillRepeatedly([] (char *server) {
      char temp[] = "mega.supla.org";
      memcpy(server, temp, strlen(temp));
      return true;
    });
  EXPECT_CALL(cfg, getEmail(_)).WillRepeatedly([] (char *mail) {
      char temp[] = "ceo@supla.org";
      memcpy(mail, temp, strlen(temp));
      return true;
    });
  // ssl enabled in config is realized by setting port to -1 (auto/default)
  EXPECT_CALL(cfg, getSuplaServerPort()).WillRepeatedly(Return(-1));
  EXPECT_CALL(cfg, saveIfNeeded()).Times(AtLeast(1));
  EXPECT_CALL(cfg, getUInt8(_, _)).WillRepeatedly([] (const char *key,
        uint8_t *buf) {
      if (strcmp(key, "security_level") == 0) {
        *buf = 2;
        return true;
      }
      EXPECT_TRUE(false);
      return false;
    });
  EXPECT_CALL(cfg, getWiFiSSID(_)).WillRepeatedly([] (char *ssid) {
      char temp[] = "ssid_test";
      memcpy(ssid, temp, strlen(temp));
      return true;
    });
  EXPECT_CALL(cfg, getWiFiPassword(_)).WillRepeatedly([] (char *pass) {
      char temp[] = "pass_test";
      memcpy(pass, temp, strlen(temp));
      return true;
    });

  EXPECT_CALL(cfg, isSuplaCommProtocolEnabled()).WillRepeatedly(Return(true));

  EXPECT_CALL(el1, onLoadConfig(_)).Times(1);
  EXPECT_CALL(el2, onLoadConfig(_)).Times(1);
  EXPECT_CALL(el1, onInit()).Times(1);
  EXPECT_CALL(el2, onInit()).Times(1);
  EXPECT_CALL(el1, onRegistered(_));
  EXPECT_CALL(el2, onRegistered(_));

  EXPECT_CALL(timer, initTimers());
  EXPECT_CALL(srpc, srpc_params_init(_));
  EXPECT_CALL(srpc, srpc_init(_)).WillOnce(Return(&dummy));
  EXPECT_CALL(srpc, srpc_set_proto_version(&dummy, 18));

  EXPECT_TRUE(sd.begin(18));
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
  EXPECT_EQ(client->getRootCACert(), nullptr);
}

TEST_F(FullStartupWithConfig, OfflineModeOneProto) {
  int dummy = 0;
  sd.allowWorkInOfflineMode();
  {
    InSequence s;
    EXPECT_CALL(statusMock, status(STATUS_UNKNOWN_SERVER_ADDRESS, _)).Times(1);
    EXPECT_CALL(statusMock, status(STATUS_MISSING_CREDENTIALS, _)).Times(1);
    EXPECT_CALL(statusMock, status(STATUS_INITIALIZED, _)).Times(1);
    EXPECT_CALL(statusMock, status(STATUS_OFFLINE_MODE, _)).Times(1);
  }

  EXPECT_CALL(cfg, getDeviceName(_)).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getGUID(_)).WillRepeatedly([] (char *guid) {
      char GUID[SUPLA_GUID_SIZE] = {1};
      memcpy(guid, GUID, SUPLA_GUID_SIZE);
      return true;
    });
  EXPECT_CALL(cfg, getAuthKey(_)).WillRepeatedly([] (char *auth) {
      char AUTHKEY[SUPLA_AUTHKEY_SIZE] = {2};
      memcpy(auth, AUTHKEY, SUPLA_AUTHKEY_SIZE);
      return true;
    });
  EXPECT_CALL(cfg, getDeviceMode())
    .WillRepeatedly(Return(Supla::DEVICE_MODE_NOT_SET));
  EXPECT_CALL(cfg, getSuplaServer(_)).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getEmail(_)).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getSuplaServerPort()).WillRepeatedly(Return(2016));
  EXPECT_CALL(cfg, saveIfNeeded()).Times(AtLeast(1));
  EXPECT_CALL(cfg, getUInt8(_, _)).WillRepeatedly([] (const char *key,
        uint8_t *buf) {
      if (strcmp(key, "security_level") == 0) {
        *buf = 0;
        return true;
      }
      return false;
    });
  EXPECT_CALL(cfg, getWiFiSSID(_)).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getWiFiPassword(_)).WillRepeatedly(Return(false));

  EXPECT_CALL(cfg, isSuplaCommProtocolEnabled()).WillRepeatedly(Return(true));

  EXPECT_CALL(el1, onLoadConfig(_)).Times(1);
  EXPECT_CALL(el2, onLoadConfig(_)).Times(1);
  EXPECT_CALL(el1, onInit()).Times(1);
  EXPECT_CALL(el2, onInit()).Times(1);
  EXPECT_CALL(el1, onRegistered(_)).Times(0);
  EXPECT_CALL(el2, onRegistered(_)).Times(0);

  EXPECT_CALL(timer, initTimers());
//  EXPECT_CALL(srpc, srpc_params_init(_));
//  EXPECT_CALL(srpc, srpc_init(_)).WillOnce(Return(&dummy));
//  EXPECT_CALL(srpc, srpc_set_proto_version(&dummy, 18));

  EXPECT_TRUE(sd.begin(18));
  EXPECT_EQ(sd.getCurrentStatus(), STATUS_OFFLINE_MODE);

  EXPECT_CALL(net, isReady()).Times(0);
  EXPECT_CALL(net, setup()).Times(0);
  EXPECT_CALL(net, iterate()).Times(AtLeast(0));
  EXPECT_CALL(srpc, srpc_iterate(_)).Times(0);
  EXPECT_CALL(*client, stop()).Times(0);

  EXPECT_CALL(*client, connected()).Times(0);
  EXPECT_CALL(*client, connectImp(_, 2016)).Times(0);
  EXPECT_CALL(srpc, srpc_ds_async_registerdevice_e(_, _)).Times(0);

  EXPECT_CALL(el1, iterateAlways()).Times(AtLeast(1));
  EXPECT_CALL(el2, iterateAlways()).Times(AtLeast(1));
  EXPECT_CALL(el1, iterateConnected()).Times(0);
  EXPECT_CALL(el2, iterateConnected()).Times(0);

  EXPECT_CALL(board, deviceSoftwareReset()).Times(0);

  for (int i = 0; i < 5; i++) {
    sd.iterate();
    time.advance(100);
  }
  EXPECT_EQ(sd.getCurrentStatus(), STATUS_OFFLINE_MODE);
}

TEST_F(FullStartupWithConfig, OfflineModeProtoDisabled) {
  int dummy = 0;
  sd.allowWorkInOfflineMode();
  {
    InSequence s;
    EXPECT_CALL(statusMock, status(STATUS_ALL_PROTOCOLS_DISABLED, _)).Times(1);
    EXPECT_CALL(statusMock, status(STATUS_INITIALIZED, _)).Times(1);
    EXPECT_CALL(statusMock, status(STATUS_OFFLINE_MODE, _)).Times(1);
  }

  EXPECT_CALL(cfg, getDeviceName(_)).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getGUID(_)).WillRepeatedly([] (char *guid) {
      char GUID[SUPLA_GUID_SIZE] = {1};
      memcpy(guid, GUID, SUPLA_GUID_SIZE);
      return true;
    });
  EXPECT_CALL(cfg, getAuthKey(_)).WillRepeatedly([] (char *auth) {
      char AUTHKEY[SUPLA_AUTHKEY_SIZE] = {2};
      memcpy(auth, AUTHKEY, SUPLA_AUTHKEY_SIZE);
      return true;
    });
  EXPECT_CALL(cfg, getDeviceMode())
    .WillRepeatedly(Return(Supla::DEVICE_MODE_NOT_SET));
  EXPECT_CALL(cfg, getSuplaServer(_)).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getEmail(_)).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getSuplaServerPort()).WillRepeatedly(Return(2016));
  EXPECT_CALL(cfg, saveIfNeeded()).Times(AtLeast(1));
  EXPECT_CALL(cfg, getUInt8(_, _)).WillRepeatedly([] (const char *key,
        uint8_t *buf) {
      if (strcmp(key, "security_level") == 0) {
        *buf = 0;
        return true;
      }
      return false;
    });
  EXPECT_CALL(cfg, getWiFiSSID(_)).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getWiFiPassword(_)).WillRepeatedly(Return(false));

  EXPECT_CALL(cfg, isSuplaCommProtocolEnabled()).WillRepeatedly(Return(false));

  EXPECT_CALL(el1, onLoadConfig(_)).Times(1);
  EXPECT_CALL(el2, onLoadConfig(_)).Times(1);
  EXPECT_CALL(el1, onInit()).Times(1);
  EXPECT_CALL(el2, onInit()).Times(1);
  EXPECT_CALL(el1, onRegistered(_)).Times(0);
  EXPECT_CALL(el2, onRegistered(_)).Times(0);

  EXPECT_CALL(timer, initTimers());
  EXPECT_CALL(srpc, srpc_params_init(_)).Times(0);
  EXPECT_CALL(srpc, srpc_init(_)).Times(0);
  EXPECT_CALL(srpc, srpc_set_proto_version(&dummy, 18)).Times(0);

  EXPECT_TRUE(sd.begin(18));
  EXPECT_EQ(sd.getCurrentStatus(), STATUS_OFFLINE_MODE);

  EXPECT_CALL(net, isReady()).Times(0);
  EXPECT_CALL(net, setup()).Times(0);
  EXPECT_CALL(net, iterate()).Times(AtLeast(0));
  EXPECT_CALL(srpc, srpc_iterate(_)).Times(0);
  EXPECT_CALL(*client, stop()).Times(0);

  EXPECT_CALL(*client, connected()).Times(0);
  EXPECT_CALL(*client, connectImp(_, 2016)).Times(0);
  EXPECT_CALL(srpc, srpc_ds_async_registerdevice_e(_, _)).Times(0);

  EXPECT_CALL(el1, iterateAlways()).Times(AtLeast(1));
  EXPECT_CALL(el2, iterateAlways()).Times(AtLeast(1));
  EXPECT_CALL(el1, iterateConnected()).Times(0);
  EXPECT_CALL(el2, iterateConnected()).Times(0);

  EXPECT_CALL(board, deviceSoftwareReset()).Times(0);

  for (int i = 0; i < 5; i++) {
    sd.iterate();
    time.advance(100);
  }
  EXPECT_EQ(sd.getCurrentStatus(), STATUS_OFFLINE_MODE);
}

TEST_F(FullStartupWithConfig, OfflineModeSuplaOnMqttOff) {
  int dummy = 0;
  MqttMock mqtt(&sd);
  sd.allowWorkInOfflineMode();
  {
    InSequence s;
    EXPECT_CALL(statusMock, status(STATUS_UNKNOWN_SERVER_ADDRESS, _)).Times(1);
    EXPECT_CALL(statusMock, status(STATUS_MISSING_CREDENTIALS, _)).Times(1);
    EXPECT_CALL(statusMock, status(STATUS_INITIALIZED, _)).Times(1);
    EXPECT_CALL(statusMock, status(STATUS_OFFLINE_MODE, _)).Times(1);
  }

  EXPECT_CALL(cfg, isSuplaCommProtocolEnabled()).WillRepeatedly(Return(true));
  EXPECT_CALL(cfg, isMqttCommProtocolEnabled()).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getMqttPrefix(_)).Times(0);

  EXPECT_CALL(cfg, getDeviceName(_)).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getGUID(_)).WillRepeatedly([] (char *guid) {
      char GUID[SUPLA_GUID_SIZE] = {1};
      memcpy(guid, GUID, SUPLA_GUID_SIZE);
      return true;
    });
  EXPECT_CALL(cfg, getAuthKey(_)).WillRepeatedly([] (char *auth) {
      char AUTHKEY[SUPLA_AUTHKEY_SIZE] = {2};
      memcpy(auth, AUTHKEY, SUPLA_AUTHKEY_SIZE);
      return true;
    });
  EXPECT_CALL(cfg, getDeviceMode())
    .WillRepeatedly(Return(Supla::DEVICE_MODE_NOT_SET));
  EXPECT_CALL(cfg, getSuplaServer(_)).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getEmail(_)).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getSuplaServerPort()).WillRepeatedly(Return(2016));
  EXPECT_CALL(cfg, saveIfNeeded()).Times(AtLeast(1));
  EXPECT_CALL(cfg, getUInt8(_, _)).WillRepeatedly([] (const char *key,
        uint8_t *buf) {
      if (strcmp(key, "security_level") == 0) {
        *buf = 0;
        return true;
      }
      return false;
    });
  EXPECT_CALL(cfg, getWiFiSSID(_)).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getWiFiPassword(_)).WillRepeatedly(Return(false));

  EXPECT_CALL(el1, onLoadConfig(_)).Times(1);
  EXPECT_CALL(el2, onLoadConfig(_)).Times(1);
  EXPECT_CALL(el1, onInit()).Times(1);
  EXPECT_CALL(el2, onInit()).Times(1);
  EXPECT_CALL(el1, onRegistered(_)).Times(0);
  EXPECT_CALL(el2, onRegistered(_)).Times(0);

  EXPECT_CALL(timer, initTimers());

  EXPECT_TRUE(sd.begin(18));
  EXPECT_EQ(sd.getCurrentStatus(), STATUS_OFFLINE_MODE);

  EXPECT_CALL(net, isReady()).Times(0);
  EXPECT_CALL(net, setup()).Times(0);
  EXPECT_CALL(net, iterate()).Times(AtLeast(0));
  EXPECT_CALL(srpc, srpc_iterate(_)).Times(0);
  EXPECT_CALL(*client, stop()).Times(0);

  EXPECT_CALL(*client, connected()).Times(0);
  EXPECT_CALL(*client, connectImp(_, 2016)).Times(0);
  EXPECT_CALL(srpc, srpc_ds_async_registerdevice_e(_, _)).Times(0);

  EXPECT_CALL(el1, iterateAlways()).Times(AtLeast(1));
  EXPECT_CALL(el2, iterateAlways()).Times(AtLeast(1));
  EXPECT_CALL(el1, iterateConnected()).Times(0);
  EXPECT_CALL(el2, iterateConnected()).Times(0);

  EXPECT_CALL(board, deviceSoftwareReset()).Times(0);

  for (int i = 0; i < 5; i++) {
    sd.iterate();
    time.advance(100);
  }
  EXPECT_EQ(sd.getCurrentStatus(), STATUS_OFFLINE_MODE);
}

TEST_F(FullStartupWithConfig, OfflineModeSuplaOffMqttOff) {
  int dummy = 0;
  MqttMock mqtt(&sd);
  sd.allowWorkInOfflineMode();
  {
    InSequence s;
    EXPECT_CALL(statusMock, status(STATUS_ALL_PROTOCOLS_DISABLED, _)).Times(1);
    EXPECT_CALL(statusMock, status(STATUS_INITIALIZED, _)).Times(1);
    EXPECT_CALL(statusMock, status(STATUS_OFFLINE_MODE, _)).Times(1);
  }

  EXPECT_CALL(cfg, isSuplaCommProtocolEnabled()).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, isMqttCommProtocolEnabled()).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getMqttPrefix(_)).Times(0);

  EXPECT_CALL(cfg, getDeviceName(_)).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getGUID(_)).WillRepeatedly([] (char *guid) {
      char GUID[SUPLA_GUID_SIZE] = {1};
      memcpy(guid, GUID, SUPLA_GUID_SIZE);
      return true;
    });
  EXPECT_CALL(cfg, getAuthKey(_)).WillRepeatedly([] (char *auth) {
      char AUTHKEY[SUPLA_AUTHKEY_SIZE] = {2};
      memcpy(auth, AUTHKEY, SUPLA_AUTHKEY_SIZE);
      return true;
    });
  EXPECT_CALL(cfg, getDeviceMode())
    .WillRepeatedly(Return(Supla::DEVICE_MODE_NOT_SET));
  EXPECT_CALL(cfg, getSuplaServer(_)).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getEmail(_)).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getSuplaServerPort()).WillRepeatedly(Return(2016));
  EXPECT_CALL(cfg, saveIfNeeded()).Times(AtLeast(1));
  EXPECT_CALL(cfg, getUInt8(_, _)).WillRepeatedly([] (const char *key,
        uint8_t *buf) {
      if (strcmp(key, "security_level") == 0) {
        *buf = 0;
        return true;
      }
      return false;
    });
  EXPECT_CALL(cfg, getWiFiSSID(_)).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getWiFiPassword(_)).WillRepeatedly(Return(false));

  EXPECT_CALL(el1, onLoadConfig(_)).Times(1);
  EXPECT_CALL(el2, onLoadConfig(_)).Times(1);
  EXPECT_CALL(el1, onInit()).Times(1);
  EXPECT_CALL(el2, onInit()).Times(1);
  EXPECT_CALL(el1, onRegistered(_)).Times(0);
  EXPECT_CALL(el2, onRegistered(_)).Times(0);

  EXPECT_CALL(timer, initTimers());
  EXPECT_CALL(srpc, srpc_params_init(_)).Times(0);
  EXPECT_CALL(srpc, srpc_init(_)).Times(0);
  EXPECT_CALL(srpc, srpc_set_proto_version(&dummy, 18)).Times(0);

  EXPECT_TRUE(sd.begin(18));
  EXPECT_EQ(sd.getCurrentStatus(), STATUS_OFFLINE_MODE);

  EXPECT_CALL(net, isReady()).Times(0);
  EXPECT_CALL(net, setup()).Times(0);
  EXPECT_CALL(net, iterate()).Times(AtLeast(0));
  EXPECT_CALL(srpc, srpc_iterate(_)).Times(0);
  EXPECT_CALL(*client, stop()).Times(0);

  EXPECT_CALL(*client, connected()).Times(0);
  EXPECT_CALL(*client, connectImp(_, 2016)).Times(0);
  EXPECT_CALL(srpc, srpc_ds_async_registerdevice_e(_, _)).Times(0);

  EXPECT_CALL(el1, iterateAlways()).Times(AtLeast(1));
  EXPECT_CALL(el2, iterateAlways()).Times(AtLeast(1));
  EXPECT_CALL(el1, iterateConnected()).Times(0);
  EXPECT_CALL(el2, iterateConnected()).Times(0);

  EXPECT_CALL(board, deviceSoftwareReset()).Times(0);

  for (int i = 0; i < 5; i++) {
    sd.iterate();
    time.advance(100);
  }
  EXPECT_EQ(sd.getCurrentStatus(), STATUS_OFFLINE_MODE);
}

TEST_F(FullStartupWithConfig, OfflineModeSuplaOffMqttOn) {
  int dummy = 0;
  MqttMock mqtt(&sd);
  sd.allowWorkInOfflineMode();
  {
    InSequence s;
    EXPECT_CALL(statusMock, status(STATUS_UNKNOWN_SERVER_ADDRESS, _)).Times(1);
    EXPECT_CALL(statusMock, status(STATUS_INITIALIZED, _)).Times(1);
    EXPECT_CALL(statusMock, status(STATUS_OFFLINE_MODE, _)).Times(1);
  }

  EXPECT_CALL(cfg, isSuplaCommProtocolEnabled()).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, isMqttCommProtocolEnabled()).WillRepeatedly(Return(true));

  EXPECT_CALL(cfg, getDeviceName(_)).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getGUID(_)).WillRepeatedly([] (char *guid) {
      char GUID[SUPLA_GUID_SIZE] = {1};
      memcpy(guid, GUID, SUPLA_GUID_SIZE);
      return true;
    });
  EXPECT_CALL(cfg, getAuthKey(_)).WillRepeatedly([] (char *auth) {
      char AUTHKEY[SUPLA_AUTHKEY_SIZE] = {2};
      memcpy(auth, AUTHKEY, SUPLA_AUTHKEY_SIZE);
      return true;
    });
  EXPECT_CALL(cfg, getDeviceMode())
    .WillRepeatedly(Return(Supla::DEVICE_MODE_NOT_SET));

  // Supla protocol configuration
  EXPECT_CALL(cfg, getSuplaServer(_)).Times(0);
  EXPECT_CALL(cfg, getEmail(_)).Times(0);
  EXPECT_CALL(cfg, getSuplaServerPort()).Times(0);
  EXPECT_CALL(cfg, getUInt8(_, _)).WillRepeatedly([] (const char *key,
        uint8_t *buf) {
      if (strcmp(key, "security_level") == 0) {
        *buf = 0;
        EXPECT_TRUE(false);
        return true;
      }
      return false;
    });
  EXPECT_CALL(cfg, getWiFiSSID(_)).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getWiFiPassword(_)).WillRepeatedly(Return(false));

  // MQTT protocol configuration
  EXPECT_CALL(cfg, getMqttPrefix(_)).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getMqttServer(_)).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getMqttServerPort()).WillRepeatedly(Return(1883));
  EXPECT_CALL(cfg, isMqttTlsEnabled()).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, isMqttRetainEnabled()).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getMqttQos()).WillRepeatedly(Return(0));
  EXPECT_CALL(cfg, isMqttAuthEnabled()).WillRepeatedly(Return(false));

  EXPECT_CALL(cfg, saveIfNeeded()).Times(AtLeast(1));

  EXPECT_CALL(el1, onLoadConfig(_)).Times(1);
  EXPECT_CALL(el2, onLoadConfig(_)).Times(1);
  EXPECT_CALL(el1, onInit()).Times(1);
  EXPECT_CALL(el2, onInit()).Times(1);
  EXPECT_CALL(el1, onRegistered(_)).Times(0);
  EXPECT_CALL(el2, onRegistered(_)).Times(0);

  EXPECT_CALL(timer, initTimers());
  EXPECT_CALL(srpc, srpc_params_init(_)).Times(0);
  EXPECT_CALL(srpc, srpc_init(_)).Times(0);
  EXPECT_CALL(srpc, srpc_set_proto_version(&dummy, 18)).Times(0);

  EXPECT_TRUE(sd.begin(18));
  EXPECT_EQ(sd.getCurrentStatus(), STATUS_OFFLINE_MODE);

  EXPECT_CALL(net, isReady()).Times(0);
  EXPECT_CALL(net, setup()).Times(0);
  EXPECT_CALL(net, iterate()).Times(AtLeast(0));
  EXPECT_CALL(srpc, srpc_iterate(_)).Times(0);
  EXPECT_CALL(*client, stop()).Times(0);

  EXPECT_CALL(*client, connected()).Times(0);
  EXPECT_CALL(*client, connectImp(_, 2016)).Times(0);
  EXPECT_CALL(srpc, srpc_ds_async_registerdevice_e(_, _)).Times(0);

  EXPECT_CALL(el1, iterateAlways()).Times(AtLeast(1));
  EXPECT_CALL(el2, iterateAlways()).Times(AtLeast(1));
  EXPECT_CALL(el1, iterateConnected()).Times(0);
  EXPECT_CALL(el2, iterateConnected()).Times(0);

  EXPECT_CALL(board, deviceSoftwareReset()).Times(0);

  for (int i = 0; i < 5; i++) {
    sd.iterate();
    time.advance(100);
  }
  EXPECT_EQ(sd.getCurrentStatus(), STATUS_OFFLINE_MODE);
}

TEST_F(FullStartupWithConfig, OfflineModeOneProtoWifiSsidSet) {
  int dummy = 0;
  sd.allowWorkInOfflineMode();
  {
    InSequence s;
    EXPECT_CALL(statusMock, status(STATUS_UNKNOWN_SERVER_ADDRESS, _)).Times(1);
    EXPECT_CALL(statusMock, status(STATUS_MISSING_CREDENTIALS, _)).Times(1);
    EXPECT_CALL(statusMock, status(STATUS_INITIALIZED, _)).Times(1);
    EXPECT_CALL(statusMock, status(STATUS_CONFIG_MODE, _)).Times(1);
  }

  EXPECT_CALL(cfg, getDeviceName(_)).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getGUID(_)).WillRepeatedly([] (char *guid) {
      char GUID[SUPLA_GUID_SIZE] = {1};
      memcpy(guid, GUID, SUPLA_GUID_SIZE);
      return true;
    });
  EXPECT_CALL(cfg, getAuthKey(_)).WillRepeatedly([] (char *auth) {
      char AUTHKEY[SUPLA_AUTHKEY_SIZE] = {2};
      memcpy(auth, AUTHKEY, SUPLA_AUTHKEY_SIZE);
      return true;
    });
  EXPECT_CALL(cfg, getDeviceMode())
    .WillRepeatedly(Return(Supla::DEVICE_MODE_NOT_SET));
  EXPECT_CALL(cfg, getSuplaServer(_)).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getEmail(_)).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getSuplaServerPort()).WillRepeatedly(Return(2016));
  EXPECT_CALL(cfg, saveIfNeeded()).Times(AtLeast(1));
  EXPECT_CALL(cfg, getUInt8(_, _)).WillRepeatedly([] (const char *key,
        uint8_t *buf) {
      if (strcmp(key, "security_level") == 0) {
        *buf = 0;
        return true;
      }
      return false;
    });

  EXPECT_CALL(cfg, getWiFiSSID(_)).WillRepeatedly([] (char *ssid) {
      char temp[] = "test_ssid";
      memcpy(ssid, temp, strlen(temp));
      return true;
    });
  EXPECT_CALL(cfg, getWiFiPassword(_)).WillRepeatedly(Return(false));

  EXPECT_CALL(cfg, isSuplaCommProtocolEnabled()).WillRepeatedly(Return(true));

  EXPECT_CALL(el1, onLoadConfig(_)).Times(1);
  EXPECT_CALL(el2, onLoadConfig(_)).Times(1);
  EXPECT_CALL(el1, onInit()).Times(1);
  EXPECT_CALL(el2, onInit()).Times(1);
  EXPECT_CALL(el1, onRegistered(_)).Times(0);
  EXPECT_CALL(el2, onRegistered(_)).Times(0);

  EXPECT_CALL(timer, initTimers());

  EXPECT_CALL(net, isReady()).Times(0);
  EXPECT_CALL(net, setup()).Times(1);
  EXPECT_CALL(net, iterate()).Times(AtLeast(0));
  EXPECT_CALL(srpc, srpc_iterate(_)).Times(0);
  EXPECT_CALL(*client, stop()).Times(1);

  EXPECT_CALL(*client, connected()).Times(0);
  EXPECT_CALL(*client, connectImp(_, 2016)).Times(0);
  EXPECT_CALL(srpc, srpc_ds_async_registerdevice_e(_, _)).Times(0);

  EXPECT_CALL(el1, iterateAlways()).Times(AtLeast(1));
  EXPECT_CALL(el2, iterateAlways()).Times(AtLeast(1));
  EXPECT_CALL(el1, iterateConnected()).Times(0);
  EXPECT_CALL(el2, iterateConnected()).Times(0);

  EXPECT_CALL(board, deviceSoftwareReset()).Times(0);

  EXPECT_TRUE(sd.begin(18));
  EXPECT_EQ(sd.getCurrentStatus(), STATUS_CONFIG_MODE);

  for (int i = 0; i < 5; i++) {
    sd.iterate();
    time.advance(100);
  }
  EXPECT_EQ(sd.getCurrentStatus(), STATUS_CONFIG_MODE);
}

TEST_F(FullStartupWithConfig, OfflineModeOneProtoWifiSsidAndPassSet) {
  int dummy = 0;
  sd.allowWorkInOfflineMode();
  {
    InSequence s;
    EXPECT_CALL(statusMock, status(STATUS_UNKNOWN_SERVER_ADDRESS, _)).Times(1);
    EXPECT_CALL(statusMock, status(STATUS_MISSING_CREDENTIALS, _)).Times(1);
    EXPECT_CALL(statusMock, status(STATUS_INITIALIZED, _)).Times(1);
    EXPECT_CALL(statusMock, status(STATUS_CONFIG_MODE, _)).Times(1);
  }

  EXPECT_CALL(cfg, getDeviceName(_)).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getGUID(_)).WillRepeatedly([] (char *guid) {
      char GUID[SUPLA_GUID_SIZE] = {1};
      memcpy(guid, GUID, SUPLA_GUID_SIZE);
      return true;
    });
  EXPECT_CALL(cfg, getAuthKey(_)).WillRepeatedly([] (char *auth) {
      char AUTHKEY[SUPLA_AUTHKEY_SIZE] = {2};
      memcpy(auth, AUTHKEY, SUPLA_AUTHKEY_SIZE);
      return true;
    });
  EXPECT_CALL(cfg, getDeviceMode())
    .WillRepeatedly(Return(Supla::DEVICE_MODE_NOT_SET));
  EXPECT_CALL(cfg, getSuplaServer(_)).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getEmail(_)).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getSuplaServerPort()).WillRepeatedly(Return(2016));
  EXPECT_CALL(cfg, saveIfNeeded()).Times(AtLeast(1));
  EXPECT_CALL(cfg, getUInt8(_, _)).WillRepeatedly([] (const char *key,
        uint8_t *buf) {
      if (strcmp(key, "security_level") == 0) {
        *buf = 0;
        return true;
      }
      return false;
    });

  EXPECT_CALL(cfg, getWiFiSSID(_)).WillRepeatedly([] (char *ssid) {
      char temp[] = "test_ssid";
      memcpy(ssid, temp, strlen(temp));
      return true;
    });
  EXPECT_CALL(cfg, getWiFiPassword(_)).WillRepeatedly([] (char *pass) {
      char temp[] = "test_pass";
      memcpy(pass, temp, strlen(temp));
      return true;
    });

  EXPECT_CALL(cfg, isSuplaCommProtocolEnabled()).WillRepeatedly(Return(true));

  EXPECT_CALL(el1, onLoadConfig(_)).Times(1);
  EXPECT_CALL(el2, onLoadConfig(_)).Times(1);
  EXPECT_CALL(el1, onInit()).Times(1);
  EXPECT_CALL(el2, onInit()).Times(1);
  EXPECT_CALL(el1, onRegistered(_)).Times(0);
  EXPECT_CALL(el2, onRegistered(_)).Times(0);

  EXPECT_CALL(timer, initTimers());

  EXPECT_CALL(net, isReady()).Times(0);
  EXPECT_CALL(net, setup()).Times(1);
  EXPECT_CALL(net, iterate()).Times(AtLeast(0));
  EXPECT_CALL(srpc, srpc_iterate(_)).Times(0);
  EXPECT_CALL(*client, stop()).Times(1);

  EXPECT_CALL(*client, connected()).Times(0);
  EXPECT_CALL(*client, connectImp(_, 2016)).Times(0);
  EXPECT_CALL(srpc, srpc_ds_async_registerdevice_e(_, _)).Times(0);

  EXPECT_CALL(el1, iterateAlways()).Times(AtLeast(1));
  EXPECT_CALL(el2, iterateAlways()).Times(AtLeast(1));
  EXPECT_CALL(el1, iterateConnected()).Times(0);
  EXPECT_CALL(el2, iterateConnected()).Times(0);

  EXPECT_CALL(board, deviceSoftwareReset()).Times(0);

  EXPECT_TRUE(sd.begin(18));
  EXPECT_EQ(sd.getCurrentStatus(), STATUS_CONFIG_MODE);

  for (int i = 0; i < 5; i++) {
    sd.iterate();
    time.advance(100);
  }
  EXPECT_EQ(sd.getCurrentStatus(), STATUS_CONFIG_MODE);
}

TEST_F(FullStartupWithConfig, OfflineModeOneProtoWifiPassSet) {
  // When only password is configured, it is considered as empty config, because
  // password can't be cleared manually via www (only factory reset will clear
  // it).
  int dummy = 0;
  sd.allowWorkInOfflineMode();
  {
    InSequence s;
    EXPECT_CALL(statusMock, status(STATUS_UNKNOWN_SERVER_ADDRESS, _)).Times(1);
    EXPECT_CALL(statusMock, status(STATUS_MISSING_CREDENTIALS, _)).Times(1);
    EXPECT_CALL(statusMock, status(STATUS_INITIALIZED, _)).Times(1);
    EXPECT_CALL(statusMock, status(STATUS_OFFLINE_MODE, _)).Times(1);
  }

  EXPECT_CALL(cfg, getDeviceName(_)).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getGUID(_)).WillRepeatedly([] (char *guid) {
      char GUID[SUPLA_GUID_SIZE] = {1};
      memcpy(guid, GUID, SUPLA_GUID_SIZE);
      return true;
    });
  EXPECT_CALL(cfg, getAuthKey(_)).WillRepeatedly([] (char *auth) {
      char AUTHKEY[SUPLA_AUTHKEY_SIZE] = {2};
      memcpy(auth, AUTHKEY, SUPLA_AUTHKEY_SIZE);
      return true;
    });
  EXPECT_CALL(cfg, getDeviceMode())
    .WillRepeatedly(Return(Supla::DEVICE_MODE_NOT_SET));
  EXPECT_CALL(cfg, getSuplaServer(_)).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getEmail(_)).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getSuplaServerPort()).WillRepeatedly(Return(2016));
  EXPECT_CALL(cfg, saveIfNeeded()).Times(AtLeast(1));
  EXPECT_CALL(cfg, getUInt8(_, _)).WillRepeatedly([] (const char *key,
        uint8_t *buf) {
      if (strcmp(key, "security_level") == 0) {
        *buf = 0;
        return true;
      }
      return false;
    });

  EXPECT_CALL(cfg, getWiFiSSID(_)).WillRepeatedly([] (char *ssid) {
      char temp[] = "";
      memcpy(ssid, temp, strlen(temp));
      return true;
    });
  EXPECT_CALL(cfg, getWiFiPassword(_)).WillRepeatedly([] (char *pass) {
      char temp[] = "test_pass";
      memcpy(pass, temp, strlen(temp));
      return true;
    });

  EXPECT_CALL(cfg, isSuplaCommProtocolEnabled()).WillRepeatedly(Return(true));

  EXPECT_CALL(el1, onLoadConfig(_)).Times(1);
  EXPECT_CALL(el2, onLoadConfig(_)).Times(1);
  EXPECT_CALL(el1, onInit()).Times(1);
  EXPECT_CALL(el2, onInit()).Times(1);
  EXPECT_CALL(el1, onRegistered(_)).Times(0);
  EXPECT_CALL(el2, onRegistered(_)).Times(0);

  EXPECT_CALL(timer, initTimers());

  EXPECT_CALL(net, isReady()).Times(0);
  EXPECT_CALL(net, setup()).Times(0);
  EXPECT_CALL(net, iterate()).Times(AtLeast(0));
  EXPECT_CALL(srpc, srpc_iterate(_)).Times(0);
  EXPECT_CALL(*client, stop()).Times(0);

  EXPECT_CALL(*client, connected()).Times(0);
  EXPECT_CALL(*client, connectImp(_, 2016)).Times(0);
  EXPECT_CALL(srpc, srpc_ds_async_registerdevice_e(_, _)).Times(0);

  EXPECT_CALL(el1, iterateAlways()).Times(AtLeast(1));
  EXPECT_CALL(el2, iterateAlways()).Times(AtLeast(1));
  EXPECT_CALL(el1, iterateConnected()).Times(0);
  EXPECT_CALL(el2, iterateConnected()).Times(0);

  EXPECT_CALL(board, deviceSoftwareReset()).Times(0);

  EXPECT_TRUE(sd.begin(18));
  EXPECT_EQ(sd.getCurrentStatus(), STATUS_OFFLINE_MODE);

  for (int i = 0; i < 5; i++) {
    sd.iterate();
    time.advance(100);
  }
  EXPECT_EQ(sd.getCurrentStatus(), STATUS_OFFLINE_MODE);
}

TEST_F(FullStartupWithConfig, OfflineModeOneProtoServerSet) {
  int dummy = 0;
  sd.allowWorkInOfflineMode();
  {
    InSequence s;
//    EXPECT_CALL(statusMock, status(STATUS_UNKNOWN_SERVER_ADDRESS, _)).Times(1);
    EXPECT_CALL(statusMock, status(STATUS_MISSING_CREDENTIALS, _)).Times(1);
    EXPECT_CALL(statusMock, status(STATUS_INITIALIZED, _)).Times(1);
    EXPECT_CALL(statusMock, status(STATUS_CONFIG_MODE, _)).Times(1);
  }

  EXPECT_CALL(cfg, getDeviceName(_)).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getGUID(_)).WillRepeatedly([] (char *guid) {
      char GUID[SUPLA_GUID_SIZE] = {1};
      memcpy(guid, GUID, SUPLA_GUID_SIZE);
      return true;
    });
  EXPECT_CALL(cfg, getAuthKey(_)).WillRepeatedly([] (char *auth) {
      char AUTHKEY[SUPLA_AUTHKEY_SIZE] = {2};
      memcpy(auth, AUTHKEY, SUPLA_AUTHKEY_SIZE);
      return true;
    });
  EXPECT_CALL(cfg, getDeviceMode())
    .WillRepeatedly(Return(Supla::DEVICE_MODE_NOT_SET));
  EXPECT_CALL(cfg, getSuplaServer(_)).WillRepeatedly([] (char *server) {
      char temp[] = "server.supla.org";
      memcpy(server, temp, strlen(temp));
      return true;
    });
  EXPECT_CALL(cfg, getEmail(_)).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getSuplaServerPort()).WillRepeatedly(Return(2016));
  EXPECT_CALL(cfg, saveIfNeeded()).Times(AtLeast(1));
  EXPECT_CALL(cfg, getUInt8(_, _)).WillRepeatedly([] (const char *key,
        uint8_t *buf) {
      if (strcmp(key, "security_level") == 0) {
        *buf = 0;
        return true;
      }
      return false;
    });

  EXPECT_CALL(cfg, getWiFiSSID(_)).WillRepeatedly([] (char *ssid) {
      char temp[] = "";
      memcpy(ssid, temp, strlen(temp));
      return true;
    });
  EXPECT_CALL(cfg, getWiFiPassword(_)).WillRepeatedly([] (char *pass) {
      char temp[] = "test_pass";
      memcpy(pass, temp, strlen(temp));
      return true;
    });

  EXPECT_CALL(cfg, isSuplaCommProtocolEnabled()).WillRepeatedly(Return(true));

  EXPECT_CALL(el1, onLoadConfig(_)).Times(1);
  EXPECT_CALL(el2, onLoadConfig(_)).Times(1);
  EXPECT_CALL(el1, onInit()).Times(1);
  EXPECT_CALL(el2, onInit()).Times(1);
  EXPECT_CALL(el1, onRegistered(_)).Times(0);
  EXPECT_CALL(el2, onRegistered(_)).Times(0);

  EXPECT_CALL(timer, initTimers());

  EXPECT_CALL(net, isReady()).Times(0);
  EXPECT_CALL(net, setup()).Times(1);
  EXPECT_CALL(net, iterate()).Times(AtLeast(0));
  EXPECT_CALL(srpc, srpc_iterate(_)).Times(0);
  EXPECT_CALL(*client, stop()).Times(1);

  EXPECT_CALL(*client, connected()).Times(0);
  EXPECT_CALL(*client, connectImp(_, 2016)).Times(0);
  EXPECT_CALL(srpc, srpc_ds_async_registerdevice_e(_, _)).Times(0);

  EXPECT_CALL(el1, iterateAlways()).Times(AtLeast(1));
  EXPECT_CALL(el2, iterateAlways()).Times(AtLeast(1));
  EXPECT_CALL(el1, iterateConnected()).Times(0);
  EXPECT_CALL(el2, iterateConnected()).Times(0);

  EXPECT_CALL(board, deviceSoftwareReset()).Times(0);

  EXPECT_TRUE(sd.begin(18));

  for (int i = 0; i < 5; i++) {
    sd.iterate();
    time.advance(100);
  }
}

TEST_F(FullStartupWithConfig, OfflineModeOneProtoEmailSet) {
  int dummy = 0;
  sd.allowWorkInOfflineMode();
  {
    InSequence s;
    EXPECT_CALL(statusMock, status(STATUS_UNKNOWN_SERVER_ADDRESS, _)).Times(1);
//    EXPECT_CALL(statusMock, status(STATUS_MISSING_CREDENTIALS, _)).Times(1);
    EXPECT_CALL(statusMock, status(STATUS_INITIALIZED, _)).Times(1);
    EXPECT_CALL(statusMock, status(STATUS_CONFIG_MODE, _)).Times(1);
  }

  EXPECT_CALL(cfg, getDeviceName(_)).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getGUID(_)).WillRepeatedly([] (char *guid) {
      char GUID[SUPLA_GUID_SIZE] = {1};
      memcpy(guid, GUID, SUPLA_GUID_SIZE);
      return true;
    });
  EXPECT_CALL(cfg, getAuthKey(_)).WillRepeatedly([] (char *auth) {
      char AUTHKEY[SUPLA_AUTHKEY_SIZE] = {2};
      memcpy(auth, AUTHKEY, SUPLA_AUTHKEY_SIZE);
      return true;
    });
  EXPECT_CALL(cfg, getDeviceMode())
    .WillRepeatedly(Return(Supla::DEVICE_MODE_NOT_SET));
  EXPECT_CALL(cfg, getEmail(_)).WillRepeatedly([] (char *mail) {
      char temp[] = "server@supla.org";
      memcpy(mail, temp, strlen(temp));
      return true;
    });
  EXPECT_CALL(cfg, getSuplaServer(_)).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getSuplaServerPort()).WillRepeatedly(Return(2016));
  EXPECT_CALL(cfg, saveIfNeeded()).Times(AtLeast(1));
  EXPECT_CALL(cfg, getUInt8(_, _)).WillRepeatedly([] (const char *key,
        uint8_t *buf) {
      if (strcmp(key, "security_level") == 0) {
        *buf = 0;
        return true;
      }
      return false;
    });

  EXPECT_CALL(cfg, getWiFiSSID(_)).WillRepeatedly([] (char *ssid) {
      char temp[] = "";
      memcpy(ssid, temp, strlen(temp));
      return true;
    });
  EXPECT_CALL(cfg, getWiFiPassword(_)).WillRepeatedly([] (char *pass) {
      char temp[] = "test_pass";
      memcpy(pass, temp, strlen(temp));
      return true;
    });

  EXPECT_CALL(cfg, isSuplaCommProtocolEnabled()).WillRepeatedly(Return(true));

  EXPECT_CALL(el1, onLoadConfig(_)).Times(1);
  EXPECT_CALL(el2, onLoadConfig(_)).Times(1);
  EXPECT_CALL(el1, onInit()).Times(1);
  EXPECT_CALL(el2, onInit()).Times(1);
  EXPECT_CALL(el1, onRegistered(_)).Times(0);
  EXPECT_CALL(el2, onRegistered(_)).Times(0);

  EXPECT_CALL(timer, initTimers());

  EXPECT_CALL(net, isReady()).Times(0);
  EXPECT_CALL(net, setup()).Times(1);
  EXPECT_CALL(net, iterate()).Times(AtLeast(0));
  EXPECT_CALL(srpc, srpc_iterate(_)).Times(0);
  EXPECT_CALL(*client, stop()).Times(1);

  EXPECT_CALL(*client, connected()).Times(0);
  EXPECT_CALL(*client, connectImp(_, 2016)).Times(0);
  EXPECT_CALL(srpc, srpc_ds_async_registerdevice_e(_, _)).Times(0);

  EXPECT_CALL(el1, iterateAlways()).Times(AtLeast(1));
  EXPECT_CALL(el2, iterateAlways()).Times(AtLeast(1));
  EXPECT_CALL(el1, iterateConnected()).Times(0);
  EXPECT_CALL(el2, iterateConnected()).Times(0);

  EXPECT_CALL(board, deviceSoftwareReset()).Times(0);

  EXPECT_TRUE(sd.begin(18));

  for (int i = 0; i < 5; i++) {
    sd.iterate();
    time.advance(100);
  }
}

TEST_F(FullStartupWithConfig, OfflineModeOneProtoFullCfgSetWifiEnabled) {
  int dummy = 0;
  sd.allowWorkInOfflineMode();
  {
    InSequence s;
//    EXPECT_CALL(statusMock, status(STATUS_UNKNOWN_SERVER_ADDRESS, _)).Times(1);
//    EXPECT_CALL(statusMock, status(STATUS_MISSING_CREDENTIALS, _)).Times(1);
    EXPECT_CALL(statusMock, status(STATUS_INITIALIZED, _)).Times(1);
    EXPECT_CALL(statusMock, status(STATUS_REGISTER_IN_PROGRESS, _)).Times(1);
    EXPECT_CALL(statusMock, status(STATUS_REGISTERED_AND_READY, _)).Times(1);
  }

  EXPECT_CALL(cfg, getDeviceName(_)).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getGUID(_)).WillRepeatedly([] (char *guid) {
      char GUID[SUPLA_GUID_SIZE] = {1};
      memcpy(guid, GUID, SUPLA_GUID_SIZE);
      return true;
    });
  EXPECT_CALL(cfg, getAuthKey(_)).WillRepeatedly([] (char *auth) {
      char AUTHKEY[SUPLA_AUTHKEY_SIZE] = {2};
      memcpy(auth, AUTHKEY, SUPLA_AUTHKEY_SIZE);
      return true;
    });
  EXPECT_CALL(cfg, getDeviceMode())
    .WillRepeatedly(Return(Supla::DEVICE_MODE_NOT_SET));
  EXPECT_CALL(cfg, getEmail(_)).WillRepeatedly([] (char *mail) {
      char temp[] = "server@supla.org";
      memcpy(mail, temp, strlen(temp));
      return true;
    });
  EXPECT_CALL(cfg, getSuplaServer(_)).WillRepeatedly([] (char *server) {
      char temp[] = "server.supla.org";
      memcpy(server, temp, strlen(temp));
      return true;
    });
  EXPECT_CALL(cfg, getSuplaServerPort()).WillRepeatedly(Return(2016));
  EXPECT_CALL(cfg, saveIfNeeded()).Times(AtLeast(1));
  EXPECT_CALL(cfg, getUInt8(_, _)).WillRepeatedly([] (const char *key,
         uint8_t *buf) {
      if (strcmp(key, "security_level") == 0) {
        *buf = 0;
        return true;
      }
      return false;
    });

  EXPECT_CALL(cfg, getWiFiSSID(_)).WillRepeatedly([] (char *ssid) {
      char temp[] = "test_ssid";
      memcpy(ssid, temp, strlen(temp));
      return true;
    });
  EXPECT_CALL(cfg, getWiFiPassword(_)).WillRepeatedly([] (char *pass) {
      char temp[] = "test_pass";
      memcpy(pass, temp, strlen(temp));
      return true;
    });

  EXPECT_CALL(cfg, isSuplaCommProtocolEnabled()).WillRepeatedly(Return(true));

  EXPECT_CALL(el1, onLoadConfig(_)).Times(1);
  EXPECT_CALL(el2, onLoadConfig(_)).Times(1);
  EXPECT_CALL(el1, onInit()).Times(1);
  EXPECT_CALL(el2, onInit()).Times(1);
  EXPECT_CALL(el1, onRegistered(_)).Times(1);
  EXPECT_CALL(el2, onRegistered(_)).Times(1);

  EXPECT_CALL(timer, initTimers());
  EXPECT_CALL(srpc, srpc_params_init(_));
  EXPECT_CALL(srpc, srpc_init(_)).WillOnce(Return(&dummy));
  EXPECT_CALL(srpc, srpc_set_proto_version(&dummy, 18));

  EXPECT_CALL(net, isReady()).WillRepeatedly(Return(true));
  EXPECT_CALL(net, setup()).Times(1);
  EXPECT_CALL(net, iterate()).Times(AtLeast(1));
  EXPECT_CALL(srpc, srpc_iterate(_)).WillRepeatedly(Return(SUPLA_RESULT_TRUE));
  EXPECT_CALL(*client, stop()).Times(0);

  EXPECT_CALL(*client, connected()).WillOnce(Return(false))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(*client, connectImp(_, 2016)).WillOnce(Return(1));
  EXPECT_CALL(srpc, srpc_ds_async_registerdevice_e(_, _)).Times(1);

  EXPECT_CALL(el1, iterateAlways()).Times(AtLeast(1));
  EXPECT_CALL(el2, iterateAlways()).Times(AtLeast(1));
  EXPECT_CALL(el1, iterateConnected()).Times(AtLeast(1))
    .WillRepeatedly(Return(true));
  EXPECT_CALL(el2, iterateConnected()).Times(AtLeast(1))
    .WillRepeatedly(Return(true));;

  EXPECT_CALL(board, deviceSoftwareReset()).Times(0);

  EXPECT_TRUE(sd.begin(18));
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
}

TEST_F(FullStartupWithConfig, OfflineModeSuplaOffMqttOnEmailSet) {
  // Supla proto is disabled, so config from Supla (email) is ignored)
  int dummy = 0;
  MqttMock mqtt(&sd);
  sd.allowWorkInOfflineMode();
  {
    InSequence s;
    EXPECT_CALL(statusMock, status(STATUS_UNKNOWN_SERVER_ADDRESS, _)).Times(1);
//    EXPECT_CALL(statusMock, status(STATUS_MISSING_CREDENTIALS, _)).Times(1);
    EXPECT_CALL(statusMock, status(STATUS_INITIALIZED, _)).Times(1);
    EXPECT_CALL(statusMock, status(STATUS_OFFLINE_MODE, _)).Times(1);
  }

  EXPECT_CALL(cfg, isSuplaCommProtocolEnabled()).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, isMqttCommProtocolEnabled()).WillRepeatedly(Return(true));

  EXPECT_CALL(cfg, getDeviceName(_)).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getGUID(_)).WillRepeatedly([] (char *guid) {
      char GUID[SUPLA_GUID_SIZE] = {1};
      memcpy(guid, GUID, SUPLA_GUID_SIZE);
      return true;
    });
  EXPECT_CALL(cfg, getAuthKey(_)).WillRepeatedly([] (char *auth) {
      char AUTHKEY[SUPLA_AUTHKEY_SIZE] = {2};
      memcpy(auth, AUTHKEY, SUPLA_AUTHKEY_SIZE);
      return true;
    });
  EXPECT_CALL(cfg, getDeviceMode())
    .WillRepeatedly(Return(Supla::DEVICE_MODE_NOT_SET));
  EXPECT_CALL(cfg, getEmail(_)).WillRepeatedly([] (char *mail) {
      char temp[] = "server@supla.org";
      memcpy(mail, temp, strlen(temp));
      return true;
    });
  EXPECT_CALL(cfg, getSuplaServer(_)).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getSuplaServerPort()).WillRepeatedly(Return(2016));
  EXPECT_CALL(cfg, saveIfNeeded()).Times(AtLeast(1));
  EXPECT_CALL(cfg, getUInt8(_, _)).WillRepeatedly([] (const char *key,
        uint8_t *buf) {
      if (strcmp(key, "security_level") == 0) {
        *buf = 0;
        return true;
      }
      return false;
    });

  EXPECT_CALL(cfg, getWiFiSSID(_)).WillRepeatedly([] (char *ssid) {
      char temp[] = "";
      memcpy(ssid, temp, strlen(temp));
      return true;
    });
  EXPECT_CALL(cfg, getWiFiPassword(_)).WillRepeatedly([] (char *pass) {
      char temp[] = "test_pass";
      memcpy(pass, temp, strlen(temp));
      return true;
    });

  // MQTT protocol configuration
  EXPECT_CALL(cfg, getMqttPrefix(_)).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getMqttServer(_)).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getMqttServerPort()).WillRepeatedly(Return(1883));
  EXPECT_CALL(cfg, isMqttTlsEnabled()).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, isMqttRetainEnabled()).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getMqttQos()).WillRepeatedly(Return(0));
  EXPECT_CALL(cfg, isMqttAuthEnabled()).WillRepeatedly(Return(false));

  EXPECT_CALL(el1, onLoadConfig(_)).Times(1);
  EXPECT_CALL(el2, onLoadConfig(_)).Times(1);
  EXPECT_CALL(el1, onInit()).Times(1);
  EXPECT_CALL(el2, onInit()).Times(1);
  EXPECT_CALL(el1, onRegistered(_)).Times(0);
  EXPECT_CALL(el2, onRegistered(_)).Times(0);

  EXPECT_CALL(timer, initTimers());
  EXPECT_CALL(srpc, srpc_params_init(_)).Times(0);
  EXPECT_CALL(srpc, srpc_init(_)).Times(0);;
  EXPECT_CALL(srpc, srpc_set_proto_version(&dummy, 18)).Times(0);

  EXPECT_CALL(net, isReady()).Times(0);
  EXPECT_CALL(net, setup()).Times(0);
  EXPECT_CALL(net, iterate()).Times(AtLeast(0));
  EXPECT_CALL(srpc, srpc_iterate(_)).Times(0);
  EXPECT_CALL(*client, stop()).Times(0);

  EXPECT_CALL(*client, connected()).Times(0);
  EXPECT_CALL(*client, connectImp(_, 2016)).Times(0);
  EXPECT_CALL(srpc, srpc_ds_async_registerdevice_e(_, _)).Times(0);

  EXPECT_CALL(el1, iterateAlways()).Times(AtLeast(1));
  EXPECT_CALL(el2, iterateAlways()).Times(AtLeast(1));
  EXPECT_CALL(el1, iterateConnected()).Times(0);
  EXPECT_CALL(el2, iterateConnected()).Times(0);

  EXPECT_CALL(board, deviceSoftwareReset()).Times(0);

  EXPECT_TRUE(sd.begin(18));

  for (int i = 0; i < 5; i++) {
    sd.iterate();
    time.advance(100);
  }
}

TEST_F(FullStartupWithConfig, OfflineModeSuplaOnMqttOnEmailSet) {
  int dummy = 0;
  MqttMock mqtt(&sd);
  sd.allowWorkInOfflineMode();
  {
    InSequence s;
    EXPECT_CALL(statusMock, status(STATUS_UNKNOWN_SERVER_ADDRESS, _)).Times(1);
//    EXPECT_CALL(statusMock, status(STATUS_MISSING_CREDENTIALS, _)).Times(1);
    EXPECT_CALL(statusMock, status(STATUS_INITIALIZED, _)).Times(1);
    EXPECT_CALL(statusMock, status(STATUS_CONFIG_MODE, _)).Times(1);
  }

  EXPECT_CALL(cfg, isSuplaCommProtocolEnabled()).WillRepeatedly(Return(true));
  EXPECT_CALL(cfg, isMqttCommProtocolEnabled()).WillRepeatedly(Return(true));

  EXPECT_CALL(cfg, getDeviceName(_)).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getGUID(_)).WillRepeatedly([] (char *guid) {
      char GUID[SUPLA_GUID_SIZE] = {1};
      memcpy(guid, GUID, SUPLA_GUID_SIZE);
      return true;
    });
  EXPECT_CALL(cfg, getAuthKey(_)).WillRepeatedly([] (char *auth) {
      char AUTHKEY[SUPLA_AUTHKEY_SIZE] = {2};
      memcpy(auth, AUTHKEY, SUPLA_AUTHKEY_SIZE);
      return true;
    });
  EXPECT_CALL(cfg, getDeviceMode())
    .WillRepeatedly(Return(Supla::DEVICE_MODE_NOT_SET));
  EXPECT_CALL(cfg, getEmail(_)).WillRepeatedly([] (char *mail) {
      char temp[] = "server@supla.org";
      memcpy(mail, temp, strlen(temp));
      return true;
    });
  EXPECT_CALL(cfg, getSuplaServer(_)).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getSuplaServerPort()).WillRepeatedly(Return(2016));
  EXPECT_CALL(cfg, saveIfNeeded()).Times(AtLeast(1));
  EXPECT_CALL(cfg, getUInt8(_, _)).WillRepeatedly([] (const char *key,
        uint8_t *buf) {
      if (strcmp(key, "security_level") == 0) {
        *buf = 0;
        return true;
      }
      return false;
    });

  EXPECT_CALL(cfg, getWiFiSSID(_)).WillRepeatedly([] (char *ssid) {
      char temp[] = "";
      memcpy(ssid, temp, strlen(temp));
      return true;
    });
  EXPECT_CALL(cfg, getWiFiPassword(_)).WillRepeatedly([] (char *pass) {
      char temp[] = "test_pass";
      memcpy(pass, temp, strlen(temp));
      return true;
    });

  // MQTT protocol configuration
  EXPECT_CALL(cfg, getMqttPrefix(_)).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getMqttServer(_)).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getMqttServerPort()).WillRepeatedly(Return(1883));
  EXPECT_CALL(cfg, isMqttTlsEnabled()).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, isMqttRetainEnabled()).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getMqttQos()).WillRepeatedly(Return(0));
  EXPECT_CALL(cfg, isMqttAuthEnabled()).WillRepeatedly(Return(false));

  EXPECT_CALL(mqtt, disconnect()).Times(1);

  EXPECT_CALL(el1, onLoadConfig(_)).Times(1);
  EXPECT_CALL(el2, onLoadConfig(_)).Times(1);
  EXPECT_CALL(el1, onInit()).Times(1);
  EXPECT_CALL(el2, onInit()).Times(1);
  EXPECT_CALL(el1, onRegistered(_)).Times(0);
  EXPECT_CALL(el2, onRegistered(_)).Times(0);

  EXPECT_CALL(timer, initTimers());

  EXPECT_CALL(net, isReady()).Times(0);
  EXPECT_CALL(net, setup()).Times(1);
  EXPECT_CALL(net, iterate()).Times(AtLeast(0));
  EXPECT_CALL(srpc, srpc_iterate(_)).Times(0);
  EXPECT_CALL(*client, stop()).Times(1);

  EXPECT_CALL(*client, connected()).Times(0);
  EXPECT_CALL(*client, connectImp(_, 2016)).Times(0);
  EXPECT_CALL(srpc, srpc_ds_async_registerdevice_e(_, _)).Times(0);

  EXPECT_CALL(el1, iterateAlways()).Times(AtLeast(1));
  EXPECT_CALL(el2, iterateAlways()).Times(AtLeast(1));
  EXPECT_CALL(el1, iterateConnected()).Times(0);
  EXPECT_CALL(el2, iterateConnected()).Times(0);

  EXPECT_CALL(board, deviceSoftwareReset()).Times(0);

  EXPECT_TRUE(sd.begin(18));

  for (int i = 0; i < 5; i++) {
    sd.iterate();
    time.advance(100);
  }
}

TEST_F(FullStartupWithConfig, OfflineModeSuplaOnMqttOffMqttServerSet) {
  int dummy = 0;
  MqttMock mqtt(&sd);
  sd.allowWorkInOfflineMode();
  {
    InSequence s;
    EXPECT_CALL(statusMock, status(STATUS_UNKNOWN_SERVER_ADDRESS, _)).Times(1);
    EXPECT_CALL(statusMock, status(STATUS_MISSING_CREDENTIALS, _)).Times(1);
    EXPECT_CALL(statusMock, status(STATUS_INITIALIZED, _)).Times(1);
    EXPECT_CALL(statusMock, status(STATUS_OFFLINE_MODE, _)).Times(1);
  }

  EXPECT_CALL(cfg, isSuplaCommProtocolEnabled()).WillRepeatedly(Return(true));
  EXPECT_CALL(cfg, isMqttCommProtocolEnabled()).WillRepeatedly(Return(false));

  EXPECT_CALL(cfg, getDeviceName(_)).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getGUID(_)).WillRepeatedly([] (char *guid) {
      char GUID[SUPLA_GUID_SIZE] = {1};
      memcpy(guid, GUID, SUPLA_GUID_SIZE);
      return true;
    });
  EXPECT_CALL(cfg, getAuthKey(_)).WillRepeatedly([] (char *auth) {
      char AUTHKEY[SUPLA_AUTHKEY_SIZE] = {2};
      memcpy(auth, AUTHKEY, SUPLA_AUTHKEY_SIZE);
      return true;
    });
  EXPECT_CALL(cfg, getDeviceMode())
    .WillRepeatedly(Return(Supla::DEVICE_MODE_NOT_SET));
  EXPECT_CALL(cfg, getEmail(_)).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getSuplaServer(_)).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getSuplaServerPort()).WillRepeatedly(Return(2016));
  EXPECT_CALL(cfg, saveIfNeeded()).Times(AtLeast(1));
  EXPECT_CALL(cfg, getUInt8(_, _)).WillRepeatedly([] (const char *key,
        uint8_t *buf) {
      if (strcmp(key, "security_level") == 0) {
        *buf = 0;
        return true;
      }
      return false;
    });

  EXPECT_CALL(cfg, getWiFiSSID(_)).WillRepeatedly([] (char *ssid) {
      char temp[] = "";
      memcpy(ssid, temp, strlen(temp));
      return true;
    });
  EXPECT_CALL(cfg, getWiFiPassword(_)).WillRepeatedly([] (char *pass) {
      char temp[] = "test_pass";
      memcpy(pass, temp, strlen(temp));
      return true;
    });

  // MQTT protocol configuration
  EXPECT_CALL(cfg, getMqttPrefix(_)).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getMqttServer(_)).WillRepeatedly([] (char *server) {
      char temp[] = "server.mqtt.supla.org";
      memcpy(server, temp, strlen(temp));
      return true;
    });
  EXPECT_CALL(cfg, getMqttServerPort()).WillRepeatedly(Return(1883));
  EXPECT_CALL(cfg, isMqttTlsEnabled()).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, isMqttRetainEnabled()).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getMqttQos()).WillRepeatedly(Return(0));
  EXPECT_CALL(cfg, isMqttAuthEnabled()).WillRepeatedly(Return(false));

  EXPECT_CALL(mqtt, disconnect()).Times(0);

  EXPECT_CALL(el1, onLoadConfig(_)).Times(1);
  EXPECT_CALL(el2, onLoadConfig(_)).Times(1);
  EXPECT_CALL(el1, onInit()).Times(1);
  EXPECT_CALL(el2, onInit()).Times(1);
  EXPECT_CALL(el1, onRegistered(_)).Times(0);
  EXPECT_CALL(el2, onRegistered(_)).Times(0);

  EXPECT_CALL(timer, initTimers());

  EXPECT_CALL(net, isReady()).Times(0);
  EXPECT_CALL(net, setup()).Times(0);
  EXPECT_CALL(net, iterate()).Times(AtLeast(0));
  EXPECT_CALL(srpc, srpc_iterate(_)).Times(0);
  EXPECT_CALL(*client, stop()).Times(0);

  EXPECT_CALL(*client, connected()).Times(0);
  EXPECT_CALL(*client, connectImp(_, 2016)).Times(0);
  EXPECT_CALL(srpc, srpc_ds_async_registerdevice_e(_, _)).Times(0);

  EXPECT_CALL(el1, iterateAlways()).Times(AtLeast(1));
  EXPECT_CALL(el2, iterateAlways()).Times(AtLeast(1));
  EXPECT_CALL(el1, iterateConnected()).Times(0);
  EXPECT_CALL(el2, iterateConnected()).Times(0);

  EXPECT_CALL(board, deviceSoftwareReset()).Times(0);

  EXPECT_TRUE(sd.begin(18));

  for (int i = 0; i < 5; i++) {
    sd.iterate();
    time.advance(100);
  }
}

TEST_F(FullStartupWithConfig, OfflineModeSuplaOnMqttOnMqttPassSet) {
  // MQTT password is ignored in configuration check because it can't
  // be cleared on www.
  int dummy = 0;
  MqttMock mqtt(&sd);
  sd.allowWorkInOfflineMode();
  {
    InSequence s;
    // Supla config check
    EXPECT_CALL(statusMock,
        status(STATUS_UNKNOWN_SERVER_ADDRESS, "Missing server address"))
      .Times(1);
    EXPECT_CALL(statusMock,
        status(STATUS_MISSING_CREDENTIALS, "Missing email address")).Times(1);
    // Mqtt config check
    EXPECT_CALL(statusMock,
        status(STATUS_UNKNOWN_SERVER_ADDRESS, "MQTT: Missing server address"))
      .Times(1);
    EXPECT_CALL(statusMock,
        status(STATUS_MISSING_CREDENTIALS, "MQTT: Missing username")).Times(1);
    EXPECT_CALL(statusMock, status(STATUS_INITIALIZED, _)).Times(1);
    EXPECT_CALL(statusMock, status(STATUS_OFFLINE_MODE, _)).Times(1);
  }

  EXPECT_CALL(cfg, isSuplaCommProtocolEnabled()).WillRepeatedly(Return(true));
  EXPECT_CALL(cfg, isMqttCommProtocolEnabled()).WillRepeatedly(Return(true));

  EXPECT_CALL(cfg, getDeviceName(_)).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getGUID(_)).WillRepeatedly([] (char *guid) {
      char GUID[SUPLA_GUID_SIZE] = {1};
      memcpy(guid, GUID, SUPLA_GUID_SIZE);
      return true;
    });
  EXPECT_CALL(cfg, getAuthKey(_)).WillRepeatedly([] (char *auth) {
      char AUTHKEY[SUPLA_AUTHKEY_SIZE] = {2};
      memcpy(auth, AUTHKEY, SUPLA_AUTHKEY_SIZE);
      return true;
    });
  EXPECT_CALL(cfg, getDeviceMode())
    .WillRepeatedly(Return(Supla::DEVICE_MODE_NOT_SET));
  EXPECT_CALL(cfg, getEmail(_)).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getSuplaServer(_)).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getSuplaServerPort()).WillRepeatedly(Return(2016));
  EXPECT_CALL(cfg, saveIfNeeded()).Times(AtLeast(1));
  EXPECT_CALL(cfg, getUInt8(_, _)).WillRepeatedly([] (const char *key,
        uint8_t *buf) {
      if (strcmp(key, "security_level") == 0) {
        *buf = 0;
        return true;
      }
      return false;
    });

  EXPECT_CALL(cfg, getWiFiSSID(_)).WillRepeatedly([] (char *ssid) {
      char temp[] = "";
      memcpy(ssid, temp, strlen(temp));
      return true;
    });
  EXPECT_CALL(cfg, getWiFiPassword(_)).WillRepeatedly([] (char *pass) {
      char temp[] = "test_pass";
      memcpy(pass, temp, strlen(temp));
      return true;
    });

  // MQTT protocol configuration
  EXPECT_CALL(cfg, getMqttPrefix(_)).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getMqttServer(_)).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getMqttServerPort()).WillRepeatedly(Return(1883));
  EXPECT_CALL(cfg, isMqttTlsEnabled()).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, isMqttRetainEnabled()).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getMqttQos()).WillRepeatedly(Return(0));
  EXPECT_CALL(cfg, isMqttAuthEnabled()).WillRepeatedly(Return(true));
  EXPECT_CALL(cfg, getMqttUser(_)).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getMqttPassword(_)).WillRepeatedly([] (char *pass) {
      char temp[] = "mqtt_pass_test";
      memcpy(pass, temp, strlen(temp));
      return true;
    });

  EXPECT_CALL(mqtt, disconnect()).Times(0);

  EXPECT_CALL(el1, onLoadConfig(_)).Times(1);
  EXPECT_CALL(el2, onLoadConfig(_)).Times(1);
  EXPECT_CALL(el1, onInit()).Times(1);
  EXPECT_CALL(el2, onInit()).Times(1);
  EXPECT_CALL(el1, onRegistered(_)).Times(0);
  EXPECT_CALL(el2, onRegistered(_)).Times(0);

  EXPECT_CALL(timer, initTimers());

  EXPECT_CALL(net, isReady()).Times(0);
  EXPECT_CALL(net, setup()).Times(0);
  EXPECT_CALL(net, iterate()).Times(AtLeast(0));
  EXPECT_CALL(srpc, srpc_iterate(_)).Times(0);
  EXPECT_CALL(*client, stop()).Times(0);

  EXPECT_CALL(*client, connected()).Times(0);
  EXPECT_CALL(*client, connectImp(_, 2016)).Times(0);
  EXPECT_CALL(srpc, srpc_ds_async_registerdevice_e(_, _)).Times(0);

  EXPECT_CALL(el1, iterateAlways()).Times(AtLeast(1));
  EXPECT_CALL(el2, iterateAlways()).Times(AtLeast(1));
  EXPECT_CALL(el1, iterateConnected()).Times(0);
  EXPECT_CALL(el2, iterateConnected()).Times(0);

  EXPECT_CALL(board, deviceSoftwareReset()).Times(0);

  EXPECT_TRUE(sd.begin(18));

  for (int i = 0; i < 5; i++) {
    sd.iterate();
    time.advance(100);
  }
}


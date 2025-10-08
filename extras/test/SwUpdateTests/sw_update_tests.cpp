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
#include <gtest/gtest.h>
#include <network_client_mock.h>
#include <network_mock.h>
#include <simple_time.h>
#include <srpc_mock.h>
#include <supla/actions.h>
#include <supla/clock/clock.h>
#include <supla/io.h>
#include <supla/protocol/supla_srpc.h>
#include <supla/sensor/therm_hygro_press_meter.h>
#include <supla/storage/config.h>
#include <supla/storage/storage.h>
#include <sw_update_mock.h>
#include <timer_mock.h>

using ::testing::_;
using ::testing::AtLeast;
using ::testing::Return;

class SwUpdateTests : public ::testing::Test {
 protected:
  SrpcMock srpc;
  NetworkMock net;
  TimerMock timers;
  SimpleTime time;
  SuplaDeviceClass sd;
  BoardMock board;
  NetworkClientMock *client = nullptr;
  ConfigMock cfg;
  SwUpdateMock swUpdate;

  virtual void SetUp() {
    if (SuplaDevice.getClock()) {
      delete SuplaDevice.getClock();
    }
    client = new NetworkClientMock;  // it will be destroyed in
                                     // Supla::Protocol::SuplaSrpc
    Supla::Channel::resetToDefaults();
    EXPECT_CALL(cfg, init()).WillOnce(Return(true));
    EXPECT_CALL(cfg, isConfigModeSupported()).WillRepeatedly(Return(true));
    EXPECT_CALL(net, isWifiConfigRequired()).WillRepeatedly(Return(true));
  }

  virtual void TearDown() {
    Supla::Channel::resetToDefaults();
    client = nullptr;
  }

  void moveTime(int count) {
    for (int i = 0; i < count; i++) {
      sd.iterate();
      time.advance(100);
    }
  }
};

TEST_F(SwUpdateTests, FirmwareCheckAndNormalUpdate) {
  EXPECT_CALL(srpc, srpc_dcs_async_get_user_localtime(_))
      .WillRepeatedly(Return(SUPLA_RESULT_TRUE));

  EXPECT_CALL(timers, initTimers()).Times(1);
  EXPECT_CALL(cfg, getDeviceName(_)).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getGUID(_)).WillRepeatedly([](char *guid) {
    char GUID[SUPLA_GUID_SIZE] = {1};
    memcpy(guid, GUID, SUPLA_GUID_SIZE);
    return true;
  });
  EXPECT_CALL(cfg, getAuthKey(_)).WillRepeatedly([](char *auth) {
    char AUTHKEY[SUPLA_AUTHKEY_SIZE] = {2};
    memcpy(auth, AUTHKEY, SUPLA_AUTHKEY_SIZE);
    return true;
  });
  EXPECT_CALL(cfg, getSwUpdateServer(_)).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, isSwUpdateBeta()).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getDeviceMode())
      .WillRepeatedly(Return(Supla::DEVICE_MODE_NOT_SET));
  EXPECT_CALL(cfg, getSuplaServer(_)).WillRepeatedly([](char *server) {
    char temp[] = "mega.supla.org";
    memcpy(server, temp, strlen(temp));
    return true;
  });
  EXPECT_CALL(cfg, getEmail(_)).WillRepeatedly([](char *mail) {
    char temp[] = "ceo@supla.org";
    memcpy(mail, temp, strlen(temp));
    return true;
  });
  // ssl disabled in config is realized by setting port to 2015
  EXPECT_CALL(cfg, getSuplaServerPort()).WillRepeatedly(Return(2015));
  EXPECT_CALL(cfg, saveIfNeeded()).Times(AtLeast(1));

  EXPECT_CALL(cfg, getInt8(_, _))
      .WillRepeatedly([](const char *key, int8_t *buf) {
        if (strcmp(key, "swUpdNoCert") == 0) {
          *buf = 0;
          return true;
        }
        EXPECT_TRUE(false);
        return false;
      });

  EXPECT_CALL(cfg, getUInt8(_, _))
      .WillRepeatedly([](const char *key, uint8_t *buf) {
        if (strcmp(key, "security_level") == 0) {
          *buf = 0;
          return true;
        }
        if (strcmp(key, "ota_mode") == 0) {
          *buf = SUPLA_FIRMWARE_UPDATE_POLICY_ALL_ENABLED;
          return true;
        }
        return false;
      });
  EXPECT_CALL(cfg, getWiFiSSID(_)).WillRepeatedly([](char *ssid) {
    char temp[] = "ssid_test";
    memcpy(ssid, temp, strlen(temp));
    return true;
  });
  EXPECT_CALL(cfg, getWiFiPassword(_)).WillRepeatedly([](char *pass) {
    char temp[] = "pass_test";
    memcpy(pass, temp, strlen(temp));
    return true;
  });

  EXPECT_CALL(cfg, isSuplaCommProtocolEnabled()).WillRepeatedly(Return(true));

  int dummy = 0;

  EXPECT_CALL(srpc, srpc_params_init(_));
  EXPECT_CALL(srpc, srpc_init(_)).WillOnce(Return(&dummy));
  EXPECT_CALL(srpc, srpc_set_proto_version(&dummy, 28));

  // proto version at least 23 is enforced by s-d
  EXPECT_TRUE(sd.begin(28));
  EXPECT_EQ(sd.getCurrentStatus(), STATUS_INITIALIZED);

  EXPECT_CALL(net, isReady()).WillRepeatedly(Return(true));
  EXPECT_CALL(net, setup()).Times(1);
  EXPECT_CALL(net, iterate()).Times(AtLeast(1));
  EXPECT_CALL(srpc, srpc_iterate(_)).WillRepeatedly(Return(SUPLA_RESULT_TRUE));

  EXPECT_CALL(*client, connected())
      .WillOnce(Return(false))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*client, connectImp(_, 2015)).WillRepeatedly(Return(1));
  EXPECT_CALL(srpc, srpc_ds_async_registerdevice_in_chunks_g(_, _)).Times(1);
  EXPECT_CALL(srpc, srpc_dcs_async_set_activity_timeout(_, _)).Times(1);

  EXPECT_CALL(srpc, srpc_rd_free(_)).Times(AtLeast(1));

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

  moveTime(10);

  EXPECT_EQ(sd.getCurrentStatus(), STATUS_REGISTERED_AND_READY);
  EXPECT_EQ(client->getRootCACert(), nullptr);

  // unauthorized
  TSD_DeviceCalCfgRequest calcfgRequest = {};
  EXPECT_CALL(srpc, srpc_getdata(_, _, _))
      .WillOnce([&calcfgRequest](
                    void *, TsrpcReceivedData *rd, unsigned _supla_int_t) {
        memset(rd, 0, sizeof(TsrpcReceivedData));
        rd->data.sd_device_calcfg_request = &calcfgRequest;
        rd->call_id = SUPLA_SD_CALL_DEVICE_CALCFG_REQUEST;
        auto calcfg = rd->data.sd_device_calcfg_request;
        calcfg->ChannelNumber = -1;
        calcfg->Command = SUPLA_CALCFG_CMD_CHECK_FIRMWARE_UPDATE;
        return SUPLA_RESULT_TRUE;
      });
  EXPECT_CALL(srpc, srpc_ds_async_device_calcfg_result(_, _))
      .WillOnce([](void *, TDS_DeviceCalCfgResult *result) {
        EXPECT_EQ(result->ChannelNumber, -1);
        EXPECT_EQ(result->Command, SUPLA_CALCFG_CMD_CHECK_FIRMWARE_UPDATE);
        EXPECT_EQ(result->Result, SUPLA_CALCFG_RESULT_UNAUTHORIZED);
        return 0;
      });
  Supla::messageReceived(nullptr, 0, 0, srpcLayer, 28);

  // authorized but not supported
  EXPECT_CALL(srpc, srpc_getdata(_, _, _))
      .WillOnce([&calcfgRequest](
                    void *, TsrpcReceivedData *rd, unsigned _supla_int_t) {
        memset(rd, 0, sizeof(TsrpcReceivedData));
        rd->data.sd_device_calcfg_request = &calcfgRequest;
        rd->call_id = SUPLA_SD_CALL_DEVICE_CALCFG_REQUEST;
        auto calcfg = rd->data.sd_device_calcfg_request;
        calcfg->ChannelNumber = -1;
        calcfg->SuperUserAuthorized = 1;
        calcfg->Command = SUPLA_CALCFG_CMD_CHECK_FIRMWARE_UPDATE;
        return SUPLA_RESULT_TRUE;
      });
  EXPECT_CALL(srpc, srpc_ds_async_device_calcfg_result(_, _))
      .WillOnce([](void *, TDS_DeviceCalCfgResult *result) {
        EXPECT_EQ(result->ChannelNumber, -1);
        EXPECT_EQ(result->Command, SUPLA_CALCFG_CMD_CHECK_FIRMWARE_UPDATE);
        EXPECT_EQ(result->Result, SUPLA_CALCFG_RESULT_NOT_SUPPORTED);
        return 0;
      });
  Supla::messageReceived(nullptr, 0, 0, srpcLayer, 28);
  moveTime(10);

  // authorized and supported, no update available
  sd.setAutomaticFirmwareUpdateSupported(true);
  EXPECT_CALL(srpc, srpc_getdata(_, _, _))
      .WillOnce([&calcfgRequest](
                    void *, TsrpcReceivedData *rd, unsigned _supla_int_t) {
        memset(rd, 0, sizeof(TsrpcReceivedData));
        rd->data.sd_device_calcfg_request = &calcfgRequest;
        rd->call_id = SUPLA_SD_CALL_DEVICE_CALCFG_REQUEST;
        auto calcfg = rd->data.sd_device_calcfg_request;
        calcfg->ChannelNumber = -1;
        calcfg->SuperUserAuthorized = 1;
        calcfg->Command = SUPLA_CALCFG_CMD_CHECK_FIRMWARE_UPDATE;
        return SUPLA_RESULT_TRUE;
      });
  EXPECT_CALL(srpc, srpc_ds_async_device_calcfg_result(_, _))
      .WillOnce([](void *, TDS_DeviceCalCfgResult *result) {
        EXPECT_EQ(result->ChannelNumber, -1);
        EXPECT_EQ(result->Command, SUPLA_CALCFG_CMD_CHECK_FIRMWARE_UPDATE);
        EXPECT_EQ(result->Result, SUPLA_CALCFG_RESULT_TRUE);
        EXPECT_EQ(result->DataSize, 0);
        return 0;
      })
      .WillOnce([](void *, TDS_DeviceCalCfgResult *result) {
        EXPECT_EQ(result->ChannelNumber, -1);
        EXPECT_EQ(result->Command, SUPLA_CALCFG_CMD_CHECK_FIRMWARE_UPDATE);
        EXPECT_EQ(result->Result, SUPLA_CALCFG_RESULT_TRUE);
        EXPECT_EQ(result->DataSize, 123);
        EXPECT_EQ(reinterpret_cast<TCalCfg_FirmwareCheckResult *>(result->Data)
                      ->Result,
                  SUPLA_FIRMWARE_CHECK_RESULT_UPDATE_NOT_AVAILABLE);
        EXPECT_STREQ(
            reinterpret_cast<TCalCfg_FirmwareCheckResult *>(result->Data)
                ->SoftVer,
            "");
        // TODO(klew): add url check
        return 0;
      });
  EXPECT_CALL(swUpdate, iterate()).WillOnce([this]() {
    swUpdate.setAborted();
  });
  Supla::messageReceived(nullptr, 0, 0, srpcLayer, 28);
  moveTime(10);

  // authorized and supported, update available
  EXPECT_CALL(srpc, srpc_getdata(_, _, _))
      .WillOnce([&calcfgRequest](
                    void *, TsrpcReceivedData *rd, unsigned _supla_int_t) {
        memset(rd, 0, sizeof(TsrpcReceivedData));
        rd->data.sd_device_calcfg_request = &calcfgRequest;
        rd->call_id = SUPLA_SD_CALL_DEVICE_CALCFG_REQUEST;
        auto calcfg = rd->data.sd_device_calcfg_request;
        calcfg->ChannelNumber = -1;
        calcfg->SuperUserAuthorized = 1;
        calcfg->Command = SUPLA_CALCFG_CMD_CHECK_FIRMWARE_UPDATE;
        return SUPLA_RESULT_TRUE;
      });
  EXPECT_CALL(srpc, srpc_ds_async_device_calcfg_result(_, _))
      .WillOnce([](void *, TDS_DeviceCalCfgResult *result) {
        EXPECT_EQ(result->ChannelNumber, -1);
        EXPECT_EQ(result->Command, SUPLA_CALCFG_CMD_CHECK_FIRMWARE_UPDATE);
        EXPECT_EQ(result->Result, SUPLA_CALCFG_RESULT_TRUE);
        EXPECT_EQ(result->DataSize, 0);
        return 0;
      })
      .WillOnce([](void *, TDS_DeviceCalCfgResult *result) {
        EXPECT_EQ(result->ChannelNumber, -1);
        EXPECT_EQ(result->Command, SUPLA_CALCFG_CMD_CHECK_FIRMWARE_UPDATE);
        EXPECT_EQ(result->Result, SUPLA_CALCFG_RESULT_TRUE);
        EXPECT_EQ(result->DataSize, 123);
        EXPECT_EQ(reinterpret_cast<TCalCfg_FirmwareCheckResult *>(result->Data)
                      ->Result,
                  SUPLA_FIRMWARE_CHECK_RESULT_UPDATE_AVAILABLE);
        EXPECT_STREQ(
            reinterpret_cast<TCalCfg_FirmwareCheckResult *>(result->Data)
                ->SoftVer,
            "1.2.3");
        // TODO(klew): add url check
        return 0;
      });
  EXPECT_CALL(swUpdate, iterate()).WillOnce([this]() {
    swUpdate.setNewVersion("1.2.3");
    swUpdate.setAborted();
  });
  Supla::messageReceived(nullptr, 0, 0, srpcLayer, 28);
  moveTime(10);

  // authorized and supported, start update, new version not available
  EXPECT_CALL(srpc, srpc_getdata(_, _, _))
      .WillOnce([&calcfgRequest](
                    void *, TsrpcReceivedData *rd, unsigned _supla_int_t) {
        memset(rd, 0, sizeof(TsrpcReceivedData));
        rd->data.sd_device_calcfg_request = &calcfgRequest;
        rd->call_id = SUPLA_SD_CALL_DEVICE_CALCFG_REQUEST;
        auto calcfg = rd->data.sd_device_calcfg_request;
        calcfg->ChannelNumber = -1;
        calcfg->SuperUserAuthorized = 1;
        calcfg->Command = SUPLA_CALCFG_CMD_START_FIRMWARE_UPDATE;
        return SUPLA_RESULT_TRUE;
      });
  EXPECT_CALL(srpc, srpc_ds_async_device_calcfg_result(_, _))
      .WillOnce([](void *, TDS_DeviceCalCfgResult *result) {
        EXPECT_EQ(result->ChannelNumber, -1);
        EXPECT_EQ(result->Command, SUPLA_CALCFG_CMD_START_FIRMWARE_UPDATE);
        EXPECT_EQ(result->Result, SUPLA_CALCFG_RESULT_TRUE);
        EXPECT_EQ(result->DataSize, 0);
        return 0;
      });

  EXPECT_CALL(swUpdate, iterate())
      .WillOnce([this]() {
        swUpdate.setNewVersion("1.2.3");
        swUpdate.setAborted();
        EXPECT_FALSE(swUpdate.isSecurityOnly());
      })
      .WillOnce([this]() {
        swUpdate.setNewVersion("1.2.3");
        EXPECT_FALSE(swUpdate.isSecurityOnly());
        swUpdate.setFinished();
      });

  EXPECT_CALL(board, deviceSoftwareReset()).Times(1);
  EXPECT_CALL(*client, stop()).Times(1);
  EXPECT_CALL(srpc, srpc_free(_)).Times(1);
  EXPECT_CALL(cfg, setDeviceMode(Supla::DeviceMode::DEVICE_MODE_NORMAL))
      .Times(1);
  EXPECT_CALL(cfg, setSwUpdateBeta(false)).Times(1);
  EXPECT_CALL(cfg, commit()).Times(AtLeast(1));

  Supla::messageReceived(nullptr, 0, 0, srpcLayer, 28);
  moveTime(5);
}

TEST_F(SwUpdateTests, SecurityOnlyUpdate) {
  EXPECT_CALL(srpc, srpc_dcs_async_get_user_localtime(_))
      .WillRepeatedly(Return(SUPLA_RESULT_TRUE));
  EXPECT_CALL(timers, initTimers()).Times(1);
  EXPECT_CALL(cfg, getDeviceName(_)).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getGUID(_)).WillRepeatedly([](char *guid) {
    char GUID[SUPLA_GUID_SIZE] = {1};
    memcpy(guid, GUID, SUPLA_GUID_SIZE);
    return true;
  });
  EXPECT_CALL(cfg, getAuthKey(_)).WillRepeatedly([](char *auth) {
    char AUTHKEY[SUPLA_AUTHKEY_SIZE] = {2};
    memcpy(auth, AUTHKEY, SUPLA_AUTHKEY_SIZE);
    return true;
  });
  EXPECT_CALL(cfg, getSwUpdateServer(_)).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, isSwUpdateBeta()).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getDeviceMode())
      .WillRepeatedly(Return(Supla::DEVICE_MODE_NOT_SET));
  EXPECT_CALL(cfg, getSuplaServer(_)).WillRepeatedly([](char *server) {
    char temp[] = "mega.supla.org";
    memcpy(server, temp, strlen(temp));
    return true;
  });
  EXPECT_CALL(cfg, getEmail(_)).WillRepeatedly([](char *mail) {
    char temp[] = "ceo@supla.org";
    memcpy(mail, temp, strlen(temp));
    return true;
  });
  // ssl disabled in config is realized by setting port to 2015
  EXPECT_CALL(cfg, getSuplaServerPort()).WillRepeatedly(Return(2015));
  EXPECT_CALL(cfg, saveIfNeeded()).Times(AtLeast(1));

  EXPECT_CALL(cfg, getInt8(_, _))
      .WillRepeatedly([](const char *key, int8_t *buf) {
        if (strcmp(key, "swUpdNoCert") == 0) {
          *buf = 0;
          return true;
        }
        EXPECT_TRUE(false);
        return false;
      });

  EXPECT_CALL(cfg, getUInt8(_, _))
      .WillRepeatedly([](const char *key, uint8_t *buf) {
        if (strcmp(key, "security_level") == 0) {
          *buf = 0;
          return true;
        }
        if (strcmp(key, "ota_mode") == 0) {
          *buf = 1;
          return true;
        }
        return false;
      });
  EXPECT_CALL(cfg, getWiFiSSID(_)).WillRepeatedly([](char *ssid) {
    char temp[] = "ssid_test";
    memcpy(ssid, temp, strlen(temp));
    return true;
  });
  EXPECT_CALL(cfg, getWiFiPassword(_)).WillRepeatedly([](char *pass) {
    char temp[] = "pass_test";
    memcpy(pass, temp, strlen(temp));
    return true;
  });

  EXPECT_CALL(cfg, isSuplaCommProtocolEnabled()).WillRepeatedly(Return(true));

  int dummy = 0;

  EXPECT_CALL(srpc, srpc_params_init(_));
  EXPECT_CALL(srpc, srpc_init(_)).WillOnce(Return(&dummy));
  EXPECT_CALL(srpc, srpc_set_proto_version(&dummy, 28));

  // proto version at least 23 is enforced by s-d
  EXPECT_TRUE(sd.begin(28));
  EXPECT_EQ(sd.getCurrentStatus(), STATUS_INITIALIZED);

  EXPECT_CALL(net, isReady()).WillRepeatedly(Return(true));
  EXPECT_CALL(net, setup()).Times(1);
  EXPECT_CALL(net, iterate()).Times(AtLeast(1));
  EXPECT_CALL(srpc, srpc_iterate(_)).WillRepeatedly(Return(SUPLA_RESULT_TRUE));

  EXPECT_CALL(*client, connected())
      .WillOnce(Return(false))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*client, connectImp(_, 2015)).WillRepeatedly(Return(1));
  EXPECT_CALL(srpc, srpc_ds_async_registerdevice_in_chunks_g(_, _)).Times(1);
  EXPECT_CALL(srpc, srpc_dcs_async_set_activity_timeout(_, _)).Times(1);

  EXPECT_CALL(srpc, srpc_rd_free(_)).Times(AtLeast(1));

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

  moveTime(10);

  EXPECT_EQ(sd.getCurrentStatus(), STATUS_REGISTERED_AND_READY);
  EXPECT_EQ(client->getRootCACert(), nullptr);
  sd.setAutomaticFirmwareUpdateSupported(true);

  TSD_DeviceCalCfgRequest calcfgRequest = {};

  // authorized and supported, start update, new version available
  EXPECT_CALL(srpc, srpc_getdata(_, _, _))
      .WillOnce([&calcfgRequest](
                    void *, TsrpcReceivedData *rd, unsigned _supla_int_t) {
        memset(rd, 0, sizeof(TsrpcReceivedData));
        rd->data.sd_device_calcfg_request = &calcfgRequest;
        rd->call_id = SUPLA_SD_CALL_DEVICE_CALCFG_REQUEST;
        auto calcfg = rd->data.sd_device_calcfg_request;
        calcfg->ChannelNumber = -1;
        calcfg->SuperUserAuthorized = 1;
        calcfg->Command = SUPLA_CALCFG_CMD_START_SECURITY_UPDATE;
        return SUPLA_RESULT_TRUE;
      });
  EXPECT_CALL(srpc, srpc_ds_async_device_calcfg_result(_, _))
      .WillOnce([](void *, TDS_DeviceCalCfgResult *result) {
        EXPECT_EQ(result->ChannelNumber, -1);
        EXPECT_EQ(result->Command, SUPLA_CALCFG_CMD_START_SECURITY_UPDATE);
        EXPECT_EQ(result->Result, SUPLA_CALCFG_RESULT_TRUE);
        EXPECT_EQ(result->DataSize, 0);
        return 0;
      });

  EXPECT_CALL(swUpdate, iterate())
      .WillOnce([this]() {
        swUpdate.setNewVersion("1.2.3");
        swUpdate.setAborted();
        EXPECT_TRUE(swUpdate.isSecurityOnlyOnFacade());
      })
      .WillOnce([this]() {
        swUpdate.setNewVersion("1.2.3");
        swUpdate.setFinished();
        EXPECT_TRUE(swUpdate.isSecurityOnlyOnFacade());
      });

  EXPECT_CALL(board, deviceSoftwareReset()).Times(1);
  EXPECT_CALL(*client, stop()).Times(1);
  EXPECT_CALL(srpc, srpc_free(_)).Times(1);
  EXPECT_CALL(cfg, setDeviceMode(Supla::DeviceMode::DEVICE_MODE_NORMAL))
      .Times(1);
  EXPECT_CALL(cfg, setSwUpdateBeta(false)).Times(1);
  EXPECT_CALL(cfg, commit()).Times(AtLeast(1));

  Supla::messageReceived(nullptr, 0, 0, srpcLayer, 28);
  moveTime(5);
}

TEST_F(SwUpdateTests, AutomaticUpdateForcedOff) {
  EXPECT_CALL(srpc, srpc_dcs_async_get_user_localtime(_))
      .WillRepeatedly(Return(SUPLA_RESULT_TRUE));
  EXPECT_CALL(timers, initTimers()).Times(1);
  EXPECT_CALL(cfg, getDeviceName(_)).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getGUID(_)).WillRepeatedly([](char *guid) {
    char GUID[SUPLA_GUID_SIZE] = {1};
    memcpy(guid, GUID, SUPLA_GUID_SIZE);
    return true;
  });
  EXPECT_CALL(cfg, getAuthKey(_)).WillRepeatedly([](char *auth) {
    char AUTHKEY[SUPLA_AUTHKEY_SIZE] = {2};
    memcpy(auth, AUTHKEY, SUPLA_AUTHKEY_SIZE);
    return true;
  });
  EXPECT_CALL(cfg, getSwUpdateServer(_)).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, isSwUpdateBeta()).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getDeviceMode())
      .WillRepeatedly(Return(Supla::DEVICE_MODE_NOT_SET));
  EXPECT_CALL(cfg, getSuplaServer(_)).WillRepeatedly([](char *server) {
    char temp[] = "mega.supla.org";
    memcpy(server, temp, strlen(temp));
    return true;
  });
  EXPECT_CALL(cfg, getEmail(_)).WillRepeatedly([](char *mail) {
    char temp[] = "ceo@supla.org";
    memcpy(mail, temp, strlen(temp));
    return true;
  });
  // ssl disabled in config is realized by setting port to 2015
  EXPECT_CALL(cfg, getSuplaServerPort()).WillRepeatedly(Return(2015));
  EXPECT_CALL(cfg, saveIfNeeded()).Times(AtLeast(1));

  EXPECT_CALL(cfg, getInt8(_, _))
      .WillRepeatedly([](const char *key, int8_t *buf) {
        if (strcmp(key, "swUpdNoCert") == 0) {
          *buf = 0;
          return true;
        }
        EXPECT_TRUE(false);
        return false;
      });

  EXPECT_CALL(cfg, getUInt8(_, _))
      .WillRepeatedly([](const char *key, uint8_t *buf) {
        if (strcmp(key, "security_level") == 0) {
          *buf = 0;
          return true;
        }
        if (strcmp(key, "ota_mode") == 0) {
          *buf = 0;
          return true;
        }
        return false;
      });
  EXPECT_CALL(cfg, getWiFiSSID(_)).WillRepeatedly([](char *ssid) {
    char temp[] = "ssid_test";
    memcpy(ssid, temp, strlen(temp));
    return true;
  });
  EXPECT_CALL(cfg, getWiFiPassword(_)).WillRepeatedly([](char *pass) {
    char temp[] = "pass_test";
    memcpy(pass, temp, strlen(temp));
    return true;
  });

  EXPECT_CALL(cfg, isSuplaCommProtocolEnabled()).WillRepeatedly(Return(true));

  int dummy = 0;

  EXPECT_CALL(srpc, srpc_params_init(_));
  EXPECT_CALL(srpc, srpc_init(_)).WillOnce(Return(&dummy));
  EXPECT_CALL(srpc, srpc_set_proto_version(&dummy, 28));

  // proto version at least 23 is enforced by s-d
  EXPECT_TRUE(sd.begin(28));
  EXPECT_EQ(sd.getCurrentStatus(), STATUS_INITIALIZED);

  EXPECT_CALL(net, isReady()).WillRepeatedly(Return(true));
  EXPECT_CALL(net, setup()).Times(1);
  EXPECT_CALL(net, iterate()).Times(AtLeast(1));
  EXPECT_CALL(srpc, srpc_iterate(_)).WillRepeatedly(Return(SUPLA_RESULT_TRUE));

  EXPECT_CALL(*client, connected())
      .WillOnce(Return(false))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*client, connectImp(_, 2015)).WillRepeatedly(Return(1));
  EXPECT_CALL(srpc, srpc_ds_async_registerdevice_in_chunks_g(_, _)).Times(1);
  EXPECT_CALL(srpc, srpc_dcs_async_set_activity_timeout(_, _)).Times(1);

  EXPECT_CALL(srpc, srpc_rd_free(_)).Times(AtLeast(1));

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

  moveTime(10);

  EXPECT_EQ(sd.getCurrentStatus(), STATUS_REGISTERED_AND_READY);
  EXPECT_EQ(client->getRootCACert(), nullptr);
  sd.setAutomaticFirmwareUpdateSupported(true);

  TSD_DeviceCalCfgRequest calcfgRequest = {};

  // authorized and supported, start update, new version available
  EXPECT_CALL(srpc, srpc_getdata(_, _, _))
      .WillOnce([&calcfgRequest](
                    void *, TsrpcReceivedData *rd, unsigned _supla_int_t) {
        memset(rd, 0, sizeof(TsrpcReceivedData));
        rd->data.sd_device_calcfg_request = &calcfgRequest;
        rd->call_id = SUPLA_SD_CALL_DEVICE_CALCFG_REQUEST;
        auto calcfg = rd->data.sd_device_calcfg_request;
        calcfg->ChannelNumber = -1;
        calcfg->SuperUserAuthorized = 1;
        calcfg->Command = SUPLA_CALCFG_CMD_START_SECURITY_UPDATE;
        return SUPLA_RESULT_TRUE;
      });
  EXPECT_CALL(srpc, srpc_ds_async_device_calcfg_result(_, _))
      .WillOnce([](void *, TDS_DeviceCalCfgResult *result) {
        EXPECT_EQ(result->ChannelNumber, -1);
        EXPECT_EQ(result->Command, SUPLA_CALCFG_CMD_START_SECURITY_UPDATE);
        EXPECT_EQ(result->Result, SUPLA_CALCFG_RESULT_FALSE);
        EXPECT_EQ(result->DataSize, 0);
        return 0;
      });

  EXPECT_CALL(swUpdate, iterate()).Times(0);
  EXPECT_CALL(board, deviceSoftwareReset()).Times(0);
  EXPECT_CALL(*client, stop()).Times(0);
  EXPECT_CALL(srpc, srpc_free(_)).Times(0);
  EXPECT_CALL(cfg, setDeviceMode(Supla::DeviceMode::DEVICE_MODE_NORMAL))
      .Times(0);
  EXPECT_CALL(cfg, setSwUpdateBeta(false)).Times(0);
  EXPECT_CALL(cfg, commit()).Times(AtLeast(0));

  Supla::messageReceived(nullptr, 0, 0, srpcLayer, 28);
  moveTime(5);

  EXPECT_CALL(srpc, srpc_getdata(_, _, _))
      .WillOnce([&calcfgRequest](
                    void *, TsrpcReceivedData *rd, unsigned _supla_int_t) {
        memset(rd, 0, sizeof(TsrpcReceivedData));
        rd->data.sd_device_calcfg_request = &calcfgRequest;
        rd->call_id = SUPLA_SD_CALL_DEVICE_CALCFG_REQUEST;
        auto calcfg = rd->data.sd_device_calcfg_request;
        calcfg->ChannelNumber = -1;
        calcfg->SuperUserAuthorized = 1;
        calcfg->Command = SUPLA_CALCFG_CMD_START_FIRMWARE_UPDATE;
        return SUPLA_RESULT_TRUE;
      });
  EXPECT_CALL(srpc, srpc_ds_async_device_calcfg_result(_, _))
      .WillOnce([](void *, TDS_DeviceCalCfgResult *result) {
        EXPECT_EQ(result->ChannelNumber, -1);
        EXPECT_EQ(result->Command, SUPLA_CALCFG_CMD_START_FIRMWARE_UPDATE);
        EXPECT_EQ(result->Result, SUPLA_CALCFG_RESULT_FALSE);
        EXPECT_EQ(result->DataSize, 0);
        return 0;
      });

  Supla::messageReceived(nullptr, 0, 0, srpcLayer, 28);
  moveTime(5);
}

TEST_F(SwUpdateTests, AutomaticUpdateTriggeredInternallySecurityOnly) {
  EXPECT_CALL(srpc, srpc_dcs_async_get_user_localtime(_))
      .WillRepeatedly(Return(SUPLA_RESULT_TRUE));

  EXPECT_CALL(timers, initTimers()).Times(1);
  EXPECT_CALL(cfg, getDeviceName(_)).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getGUID(_)).WillRepeatedly([](char *guid) {
    char GUID[SUPLA_GUID_SIZE] = {1};
    memcpy(guid, GUID, SUPLA_GUID_SIZE);
    return true;
  });
  EXPECT_CALL(cfg, getAuthKey(_)).WillRepeatedly([](char *auth) {
    char AUTHKEY[SUPLA_AUTHKEY_SIZE] = {2};
    memcpy(auth, AUTHKEY, SUPLA_AUTHKEY_SIZE);
    return true;
  });
  EXPECT_CALL(cfg, getSwUpdateServer(_)).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, isSwUpdateBeta()).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getDeviceMode())
      .WillRepeatedly(Return(Supla::DEVICE_MODE_NOT_SET));
  EXPECT_CALL(cfg, getSuplaServer(_)).WillRepeatedly([](char *server) {
    char temp[] = "mega.supla.org";
    memcpy(server, temp, strlen(temp));
    return true;
  });
  EXPECT_CALL(cfg, getEmail(_)).WillRepeatedly([](char *mail) {
    char temp[] = "ceo@supla.org";
    memcpy(mail, temp, strlen(temp));
    return true;
  });
  // ssl disabled in config is realized by setting port to 2015
  EXPECT_CALL(cfg, getSuplaServerPort()).WillRepeatedly(Return(2015));
  EXPECT_CALL(cfg, saveIfNeeded()).Times(AtLeast(1));

  EXPECT_CALL(cfg, getInt8(_, _))
      .WillRepeatedly([](const char *key, int8_t *buf) {
        if (strcmp(key, "swUpdNoCert") == 0) {
          *buf = 0;
          return true;
        }
        EXPECT_TRUE(false);
        return false;
      });

  EXPECT_CALL(cfg, getUInt8(_, _))
      .WillRepeatedly([](const char *key, uint8_t *buf) {
        if (strcmp(key, "security_level") == 0) {
          *buf = 0;
          return true;
        }
        if (strcmp(key, "ota_mode") == 0) {
          *buf = SUPLA_FIRMWARE_UPDATE_POLICY_SECURITY_ONLY;  // SECURITY ONLY
          return true;
        }
        return false;
      });
  EXPECT_CALL(cfg, getWiFiSSID(_)).WillRepeatedly([](char *ssid) {
    char temp[] = "ssid_test";
    memcpy(ssid, temp, strlen(temp));
    return true;
  });
  EXPECT_CALL(cfg, getWiFiPassword(_)).WillRepeatedly([](char *pass) {
    char temp[] = "pass_test";
    memcpy(pass, temp, strlen(temp));
    return true;
  });

  EXPECT_CALL(cfg, isSuplaCommProtocolEnabled()).WillRepeatedly(Return(true));

  int dummy = 0;

  EXPECT_CALL(srpc, srpc_params_init(_));
  EXPECT_CALL(srpc, srpc_init(_)).WillOnce(Return(&dummy));
  EXPECT_CALL(srpc, srpc_set_proto_version(&dummy, 28));

  // proto version at least 23 is enforced by s-d
  EXPECT_TRUE(sd.begin(28));
  EXPECT_EQ(sd.getCurrentStatus(), STATUS_INITIALIZED);

  EXPECT_CALL(net, isReady()).WillRepeatedly(Return(true));
  EXPECT_CALL(net, setup()).Times(1);
  EXPECT_CALL(net, iterate()).Times(AtLeast(1));
  EXPECT_CALL(srpc, srpc_iterate(_)).WillRepeatedly(Return(SUPLA_RESULT_TRUE));

  EXPECT_CALL(*client, connected())
      .WillOnce(Return(false))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*client, connectImp(_, 2015)).WillRepeatedly(Return(1));
  EXPECT_CALL(srpc, srpc_ds_async_registerdevice_in_chunks_g(_, _)).Times(1);
  EXPECT_CALL(srpc, srpc_dcs_async_set_activity_timeout(_, _)).Times(1);
  EXPECT_CALL(srpc, srpc_dcs_async_ping_server(_)).Times(AtLeast(0));

  EXPECT_CALL(srpc, srpc_rd_free(_)).Times(AtLeast(0));

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

  moveTime(10);

  EXPECT_EQ(sd.getCurrentStatus(), STATUS_REGISTERED_AND_READY);
  EXPECT_EQ(client->getRootCACert(), nullptr);
  sd.setAutomaticFirmwareUpdateSupported(true);

  EXPECT_CALL(board, deviceSoftwareReset()).Times(1);
  EXPECT_CALL(*client, stop()).Times(1);
  EXPECT_CALL(srpc, srpc_free(_)).Times(1);
  EXPECT_CALL(cfg, setDeviceMode(Supla::DeviceMode::DEVICE_MODE_NORMAL))
      .Times(1);
  EXPECT_CALL(cfg, setSwUpdateBeta(false)).Times(1);
  EXPECT_CALL(cfg, commit()).Times(AtLeast(1));

  EXPECT_CALL(swUpdate, iterate())
      .WillOnce([this]() {
        swUpdate.setNewVersion("1.2.3");
        swUpdate.setAborted();
        EXPECT_TRUE(swUpdate.isSecurityOnlyOnFacade());
      })
      .WillOnce([this]() {
        swUpdate.setNewVersion("1.2.3");
        swUpdate.setFinished();
        EXPECT_TRUE(swUpdate.isSecurityOnlyOnFacade());
      });
  moveTime(5);
  time.advance(SUPLA_AUTOMATIC_OTA_CHECK_INTERVAL);
  srpcLayer
      ->updateLastResponseTime();  // cheat to not trigger connection timeout
  moveTime(5);
}

TEST_F(SwUpdateTests, AutomaticUpdateTriggeredInternallyAllUpdates) {
  EXPECT_CALL(srpc, srpc_dcs_async_get_user_localtime(_))
      .WillRepeatedly(Return(SUPLA_RESULT_TRUE));

  EXPECT_CALL(timers, initTimers()).Times(1);
  EXPECT_CALL(cfg, getDeviceName(_)).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getGUID(_)).WillRepeatedly([](char *guid) {
    char GUID[SUPLA_GUID_SIZE] = {1};
    memcpy(guid, GUID, SUPLA_GUID_SIZE);
    return true;
  });
  EXPECT_CALL(cfg, getAuthKey(_)).WillRepeatedly([](char *auth) {
    char AUTHKEY[SUPLA_AUTHKEY_SIZE] = {2};
    memcpy(auth, AUTHKEY, SUPLA_AUTHKEY_SIZE);
    return true;
  });
  EXPECT_CALL(cfg, getSwUpdateServer(_)).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, isSwUpdateBeta()).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getDeviceMode())
      .WillRepeatedly(Return(Supla::DEVICE_MODE_NOT_SET));
  EXPECT_CALL(cfg, getSuplaServer(_)).WillRepeatedly([](char *server) {
    char temp[] = "mega.supla.org";
    memcpy(server, temp, strlen(temp));
    return true;
  });
  EXPECT_CALL(cfg, getEmail(_)).WillRepeatedly([](char *mail) {
    char temp[] = "ceo@supla.org";
    memcpy(mail, temp, strlen(temp));
    return true;
  });
  // ssl disabled in config is realized by setting port to 2015
  EXPECT_CALL(cfg, getSuplaServerPort()).WillRepeatedly(Return(2015));
  EXPECT_CALL(cfg, saveIfNeeded()).Times(AtLeast(1));

  EXPECT_CALL(cfg, getInt8(_, _))
      .WillRepeatedly([](const char *key, int8_t *buf) {
        if (strcmp(key, "swUpdNoCert") == 0) {
          *buf = 0;
          return true;
        }
        EXPECT_TRUE(false);
        return false;
      });

  EXPECT_CALL(cfg, getUInt8(_, _))
      .WillRepeatedly([](const char *key, uint8_t *buf) {
        if (strcmp(key, "security_level") == 0) {
          *buf = 0;
          return true;
        }
        if (strcmp(key, "ota_mode") == 0) {
          *buf = SUPLA_FIRMWARE_UPDATE_POLICY_ALL_ENABLED;  // ALL UPDATES
          return true;
        }
        return false;
      });
  EXPECT_CALL(cfg, getWiFiSSID(_)).WillRepeatedly([](char *ssid) {
    char temp[] = "ssid_test";
    memcpy(ssid, temp, strlen(temp));
    return true;
  });
  EXPECT_CALL(cfg, getWiFiPassword(_)).WillRepeatedly([](char *pass) {
    char temp[] = "pass_test";
    memcpy(pass, temp, strlen(temp));
    return true;
  });

  EXPECT_CALL(cfg, isSuplaCommProtocolEnabled()).WillRepeatedly(Return(true));

  int dummy = 0;

  EXPECT_CALL(srpc, srpc_params_init(_));
  EXPECT_CALL(srpc, srpc_init(_)).WillOnce(Return(&dummy));
  EXPECT_CALL(srpc, srpc_set_proto_version(&dummy, 28));

  // proto version at least 23 is enforced by s-d
  EXPECT_TRUE(sd.begin(28));
  EXPECT_EQ(sd.getCurrentStatus(), STATUS_INITIALIZED);

  EXPECT_CALL(net, isReady()).WillRepeatedly(Return(true));
  EXPECT_CALL(net, setup()).Times(1);
  EXPECT_CALL(net, iterate()).Times(AtLeast(1));
  EXPECT_CALL(srpc, srpc_iterate(_)).WillRepeatedly(Return(SUPLA_RESULT_TRUE));

  EXPECT_CALL(*client, connected())
      .WillOnce(Return(false))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*client, connectImp(_, 2015)).WillRepeatedly(Return(1));
  EXPECT_CALL(srpc, srpc_ds_async_registerdevice_in_chunks_g(_, _)).Times(1);
  EXPECT_CALL(srpc, srpc_dcs_async_set_activity_timeout(_, _)).Times(1);
  EXPECT_CALL(srpc, srpc_dcs_async_ping_server(_)).Times(AtLeast(0));

  EXPECT_CALL(srpc, srpc_rd_free(_)).Times(AtLeast(0));

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

  moveTime(10);

  EXPECT_EQ(sd.getCurrentStatus(), STATUS_REGISTERED_AND_READY);
  EXPECT_EQ(client->getRootCACert(), nullptr);
  sd.setAutomaticFirmwareUpdateSupported(true);

  EXPECT_CALL(board, deviceSoftwareReset()).Times(1);
  EXPECT_CALL(*client, stop()).Times(1);
  EXPECT_CALL(srpc, srpc_free(_)).Times(1);
  EXPECT_CALL(cfg, setDeviceMode(Supla::DeviceMode::DEVICE_MODE_NORMAL))
      .Times(1);
  EXPECT_CALL(cfg, setSwUpdateBeta(false)).Times(1);
  EXPECT_CALL(cfg, commit()).Times(AtLeast(1));

  EXPECT_CALL(swUpdate, iterate())
      .WillOnce([this]() {
        swUpdate.setNewVersion("1.2.3");
        swUpdate.setAborted();
        EXPECT_FALSE(swUpdate.isSecurityOnlyOnFacade());
      })
      .WillOnce([this]() {
        swUpdate.setNewVersion("1.2.3");
        swUpdate.setFinished();
        EXPECT_FALSE(swUpdate.isSecurityOnlyOnFacade());
      });
  moveTime(5);
  time.advance(SUPLA_AUTOMATIC_OTA_CHECK_INTERVAL);
  srpcLayer
      ->updateLastResponseTime();  // cheat to not trigger connection timeout
  moveTime(5);
}

TEST_F(SwUpdateTests, AutomaticUpdateDisabledLongTime) {
  EXPECT_CALL(srpc, srpc_dcs_async_get_user_localtime(_))
      .WillRepeatedly(Return(SUPLA_RESULT_TRUE));

  EXPECT_CALL(timers, initTimers()).Times(1);
  EXPECT_CALL(cfg, getDeviceName(_)).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getGUID(_)).WillRepeatedly([](char *guid) {
    char GUID[SUPLA_GUID_SIZE] = {1};
    memcpy(guid, GUID, SUPLA_GUID_SIZE);
    return true;
  });
  EXPECT_CALL(cfg, getAuthKey(_)).WillRepeatedly([](char *auth) {
    char AUTHKEY[SUPLA_AUTHKEY_SIZE] = {2};
    memcpy(auth, AUTHKEY, SUPLA_AUTHKEY_SIZE);
    return true;
  });
  EXPECT_CALL(cfg, getSwUpdateServer(_)).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, isSwUpdateBeta()).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getDeviceMode())
      .WillRepeatedly(Return(Supla::DEVICE_MODE_NOT_SET));
  EXPECT_CALL(cfg, getSuplaServer(_)).WillRepeatedly([](char *server) {
    char temp[] = "mega.supla.org";
    memcpy(server, temp, strlen(temp));
    return true;
  });
  EXPECT_CALL(cfg, getEmail(_)).WillRepeatedly([](char *mail) {
    char temp[] = "ceo@supla.org";
    memcpy(mail, temp, strlen(temp));
    return true;
  });
  // ssl disabled in config is realized by setting port to 2015
  EXPECT_CALL(cfg, getSuplaServerPort()).WillRepeatedly(Return(2015));
  EXPECT_CALL(cfg, saveIfNeeded()).Times(AtLeast(1));

  EXPECT_CALL(cfg, getInt8(_, _))
      .WillRepeatedly([](const char *key, int8_t *buf) {
        if (strcmp(key, "swUpdNoCert") == 0) {
          *buf = 0;
          return true;
        }
        EXPECT_TRUE(false);
        return false;
      });

  EXPECT_CALL(cfg, getUInt8(_, _))
      .WillRepeatedly([](const char *key, uint8_t *buf) {
        if (strcmp(key, "security_level") == 0) {
          *buf = 0;
          return true;
        }
        if (strcmp(key, "ota_mode") == 0) {
          *buf = SUPLA_FIRMWARE_UPDATE_POLICY_DISABLED;
          return true;
        }
        return false;
      });
  EXPECT_CALL(cfg, getWiFiSSID(_)).WillRepeatedly([](char *ssid) {
    char temp[] = "ssid_test";
    memcpy(ssid, temp, strlen(temp));
    return true;
  });
  EXPECT_CALL(cfg, getWiFiPassword(_)).WillRepeatedly([](char *pass) {
    char temp[] = "pass_test";
    memcpy(pass, temp, strlen(temp));
    return true;
  });

  EXPECT_CALL(cfg, isSuplaCommProtocolEnabled()).WillRepeatedly(Return(true));

  int dummy = 0;

  EXPECT_CALL(srpc, srpc_params_init(_));
  EXPECT_CALL(srpc, srpc_init(_)).WillOnce(Return(&dummy));
  EXPECT_CALL(srpc, srpc_set_proto_version(&dummy, 28));

  // proto version at least 23 is enforced by s-d
  EXPECT_TRUE(sd.begin(28));
  EXPECT_EQ(sd.getCurrentStatus(), STATUS_INITIALIZED);

  EXPECT_CALL(net, isReady()).WillRepeatedly(Return(true));
  EXPECT_CALL(net, setup()).Times(1);
  EXPECT_CALL(net, iterate()).Times(AtLeast(1));
  EXPECT_CALL(srpc, srpc_iterate(_)).WillRepeatedly(Return(SUPLA_RESULT_TRUE));

  EXPECT_CALL(*client, connected())
      .WillOnce(Return(false))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*client, connectImp(_, 2015)).WillRepeatedly(Return(1));
  EXPECT_CALL(srpc, srpc_ds_async_registerdevice_in_chunks_g(_, _)).Times(1);
  EXPECT_CALL(srpc, srpc_dcs_async_set_activity_timeout(_, _)).Times(1);
  EXPECT_CALL(srpc, srpc_dcs_async_ping_server(_)).Times(AtLeast(0));

  EXPECT_CALL(srpc, srpc_rd_free(_)).Times(AtLeast(0));

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

  moveTime(10);

  EXPECT_EQ(sd.getCurrentStatus(), STATUS_REGISTERED_AND_READY);
  EXPECT_EQ(client->getRootCACert(), nullptr);
  sd.setAutomaticFirmwareUpdateSupported(true);

  EXPECT_CALL(board, deviceSoftwareReset()).Times(0);
  EXPECT_CALL(*client, stop()).Times(0);
  EXPECT_CALL(srpc, srpc_free(_)).Times(0);
  EXPECT_CALL(cfg, setDeviceMode(Supla::DeviceMode::DEVICE_MODE_NORMAL))
      .Times(0);
  EXPECT_CALL(cfg, setSwUpdateBeta(false)).Times(0);
  EXPECT_CALL(cfg, commit()).Times(AtLeast(0));

  EXPECT_CALL(swUpdate, iterate()).Times(0);
  moveTime(5);
  time.advance(SUPLA_AUTOMATIC_OTA_CHECK_INTERVAL);
  srpcLayer
      ->updateLastResponseTime();  // cheat to not trigger connection timeout
  moveTime(5);
}

TEST_F(SwUpdateTests,
       AutomaticUpdateTriggeredInternallyAllUpdatesButNotAvailable) {
  EXPECT_CALL(srpc, srpc_dcs_async_get_user_localtime(_))
      .WillRepeatedly(Return(SUPLA_RESULT_TRUE));

  EXPECT_CALL(timers, initTimers()).Times(1);
  EXPECT_CALL(cfg, getDeviceName(_)).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getGUID(_)).WillRepeatedly([](char *guid) {
    char GUID[SUPLA_GUID_SIZE] = {1};
    memcpy(guid, GUID, SUPLA_GUID_SIZE);
    return true;
  });
  EXPECT_CALL(cfg, getAuthKey(_)).WillRepeatedly([](char *auth) {
    char AUTHKEY[SUPLA_AUTHKEY_SIZE] = {2};
    memcpy(auth, AUTHKEY, SUPLA_AUTHKEY_SIZE);
    return true;
  });
  EXPECT_CALL(cfg, getSwUpdateServer(_)).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, isSwUpdateBeta()).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getDeviceMode())
      .WillRepeatedly(Return(Supla::DEVICE_MODE_NOT_SET));
  EXPECT_CALL(cfg, getSuplaServer(_)).WillRepeatedly([](char *server) {
    char temp[] = "mega.supla.org";
    memcpy(server, temp, strlen(temp));
    return true;
  });
  EXPECT_CALL(cfg, getEmail(_)).WillRepeatedly([](char *mail) {
    char temp[] = "ceo@supla.org";
    memcpy(mail, temp, strlen(temp));
    return true;
  });
  // ssl disabled in config is realized by setting port to 2015
  EXPECT_CALL(cfg, getSuplaServerPort()).WillRepeatedly(Return(2015));
  EXPECT_CALL(cfg, saveIfNeeded()).Times(AtLeast(1));

  EXPECT_CALL(cfg, getInt8(_, _))
      .WillRepeatedly([](const char *key, int8_t *buf) {
        if (strcmp(key, "swUpdNoCert") == 0) {
          *buf = 0;
          return true;
        }
        EXPECT_TRUE(false);
        return false;
      });

  EXPECT_CALL(cfg, getUInt8(_, _))
      .WillRepeatedly([](const char *key, uint8_t *buf) {
        if (strcmp(key, "security_level") == 0) {
          *buf = 0;
          return true;
        }
        if (strcmp(key, "ota_mode") == 0) {
          *buf = SUPLA_FIRMWARE_UPDATE_POLICY_ALL_ENABLED;  // ALL UPDATES
          return true;
        }
        return false;
      });
  EXPECT_CALL(cfg, getWiFiSSID(_)).WillRepeatedly([](char *ssid) {
    char temp[] = "ssid_test";
    memcpy(ssid, temp, strlen(temp));
    return true;
  });
  EXPECT_CALL(cfg, getWiFiPassword(_)).WillRepeatedly([](char *pass) {
    char temp[] = "pass_test";
    memcpy(pass, temp, strlen(temp));
    return true;
  });

  EXPECT_CALL(cfg, isSuplaCommProtocolEnabled()).WillRepeatedly(Return(true));

  int dummy = 0;

  EXPECT_CALL(srpc, srpc_params_init(_));
  EXPECT_CALL(srpc, srpc_init(_)).WillOnce(Return(&dummy));
  EXPECT_CALL(srpc, srpc_set_proto_version(&dummy, 28));

  // proto version at least 23 is enforced by s-d
  EXPECT_TRUE(sd.begin(28));
  EXPECT_EQ(sd.getCurrentStatus(), STATUS_INITIALIZED);

  EXPECT_CALL(net, isReady()).WillRepeatedly(Return(true));
  EXPECT_CALL(net, setup()).Times(1);
  EXPECT_CALL(net, iterate()).Times(AtLeast(1));
  EXPECT_CALL(srpc, srpc_iterate(_)).WillRepeatedly(Return(SUPLA_RESULT_TRUE));

  EXPECT_CALL(*client, connected())
      .WillOnce(Return(false))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*client, connectImp(_, 2015)).WillRepeatedly(Return(1));
  EXPECT_CALL(srpc, srpc_ds_async_registerdevice_in_chunks_g(_, _)).Times(1);
  EXPECT_CALL(srpc, srpc_dcs_async_set_activity_timeout(_, _)).Times(1);
  EXPECT_CALL(srpc, srpc_dcs_async_ping_server(_)).Times(AtLeast(0));

  EXPECT_CALL(srpc, srpc_rd_free(_)).Times(AtLeast(0));

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

  moveTime(10);

  EXPECT_EQ(sd.getCurrentStatus(), STATUS_REGISTERED_AND_READY);
  EXPECT_EQ(client->getRootCACert(), nullptr);
  sd.setAutomaticFirmwareUpdateSupported(true);

  EXPECT_CALL(board, deviceSoftwareReset()).Times(0);
  EXPECT_CALL(*client, stop()).Times(0);
  EXPECT_CALL(srpc, srpc_free(_)).Times(0);
  EXPECT_CALL(cfg, setDeviceMode(Supla::DeviceMode::DEVICE_MODE_NORMAL))
      .Times(0);
  EXPECT_CALL(cfg, setSwUpdateBeta(false)).Times(0);
  EXPECT_CALL(cfg, commit()).Times(AtLeast(0));

  EXPECT_CALL(swUpdate, iterate()).WillOnce([this]() {
    swUpdate.setAborted();
    EXPECT_FALSE(swUpdate.isSecurityOnlyOnFacade());
  });
  moveTime(5);
  time.advance(SUPLA_AUTOMATIC_OTA_CHECK_INTERVAL);
  srpcLayer
      ->updateLastResponseTime();  // cheat to not trigger connection timeout
  moveTime(5);
}

TEST_F(SwUpdateTests, AutomaticUpdateTriggeredInternallyMissingCfgValue) {
  EXPECT_CALL(srpc, srpc_dcs_async_get_user_localtime(_))
      .WillRepeatedly(Return(SUPLA_RESULT_TRUE));

  EXPECT_CALL(timers, initTimers()).Times(1);
  EXPECT_CALL(cfg, getDeviceName(_)).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getGUID(_)).WillRepeatedly([](char *guid) {
    char GUID[SUPLA_GUID_SIZE] = {1};
    memcpy(guid, GUID, SUPLA_GUID_SIZE);
    return true;
  });
  EXPECT_CALL(cfg, getAuthKey(_)).WillRepeatedly([](char *auth) {
    char AUTHKEY[SUPLA_AUTHKEY_SIZE] = {2};
    memcpy(auth, AUTHKEY, SUPLA_AUTHKEY_SIZE);
    return true;
  });
  EXPECT_CALL(cfg, getSwUpdateServer(_)).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, isSwUpdateBeta()).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getDeviceMode())
      .WillRepeatedly(Return(Supla::DEVICE_MODE_NOT_SET));
  EXPECT_CALL(cfg, getSuplaServer(_)).WillRepeatedly([](char *server) {
    char temp[] = "mega.supla.org";
    memcpy(server, temp, strlen(temp));
    return true;
  });
  EXPECT_CALL(cfg, getEmail(_)).WillRepeatedly([](char *mail) {
    char temp[] = "ceo@supla.org";
    memcpy(mail, temp, strlen(temp));
    return true;
  });
  // ssl disabled in config is realized by setting port to 2015
  EXPECT_CALL(cfg, getSuplaServerPort()).WillRepeatedly(Return(2015));
  EXPECT_CALL(cfg, saveIfNeeded()).Times(AtLeast(1));

  EXPECT_CALL(cfg, getInt8(_, _))
      .WillRepeatedly([](const char *key, int8_t *buf) {
        if (strcmp(key, "swUpdNoCert") == 0) {
          *buf = 0;
          return true;
        }
        EXPECT_TRUE(false);
        return false;
      });

  EXPECT_CALL(cfg, getUInt8(_, _))
      .WillRepeatedly([](const char *key, uint8_t *buf) {
        if (strcmp(key, "security_level") == 0) {
          *buf = 0;
          return true;
        }
        // no "ota_mode" in cfg -> security only should be used as default
        //      if (strcmp(key, "ota_mode") == 0) {
        //        *buf = SUPLA_FIRMWARE_UPDATE_POLICY_ALL_ENABLED;  // SECURITY
        //        ONLY return true;
        //      }
        return false;
      });
  EXPECT_CALL(cfg, getWiFiSSID(_)).WillRepeatedly([](char *ssid) {
    char temp[] = "ssid_test";
    memcpy(ssid, temp, strlen(temp));
    return true;
  });
  EXPECT_CALL(cfg, getWiFiPassword(_)).WillRepeatedly([](char *pass) {
    char temp[] = "pass_test";
    memcpy(pass, temp, strlen(temp));
    return true;
  });

  EXPECT_CALL(cfg, isSuplaCommProtocolEnabled()).WillRepeatedly(Return(true));

  int dummy = 0;

  EXPECT_CALL(srpc, srpc_params_init(_));
  EXPECT_CALL(srpc, srpc_init(_)).WillOnce(Return(&dummy));
  EXPECT_CALL(srpc, srpc_set_proto_version(&dummy, 28));

  // proto version at least 23 is enforced by s-d
  EXPECT_TRUE(sd.begin(28));
  EXPECT_EQ(sd.getCurrentStatus(), STATUS_INITIALIZED);

  EXPECT_CALL(net, isReady()).WillRepeatedly(Return(true));
  EXPECT_CALL(net, setup()).Times(1);
  EXPECT_CALL(net, iterate()).Times(AtLeast(1));
  EXPECT_CALL(srpc, srpc_iterate(_)).WillRepeatedly(Return(SUPLA_RESULT_TRUE));

  EXPECT_CALL(*client, connected())
      .WillOnce(Return(false))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*client, connectImp(_, 2015)).WillRepeatedly(Return(1));
  EXPECT_CALL(srpc, srpc_ds_async_registerdevice_in_chunks_g(_, _)).Times(1);
  EXPECT_CALL(srpc, srpc_dcs_async_set_activity_timeout(_, _)).Times(1);
  EXPECT_CALL(srpc, srpc_dcs_async_ping_server(_)).Times(AtLeast(0));

  EXPECT_CALL(srpc, srpc_rd_free(_)).Times(AtLeast(0));

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

  moveTime(10);

  EXPECT_EQ(sd.getCurrentStatus(), STATUS_REGISTERED_AND_READY);
  EXPECT_EQ(client->getRootCACert(), nullptr);
  sd.setAutomaticFirmwareUpdateSupported(true);

  EXPECT_CALL(board, deviceSoftwareReset()).Times(1);
  EXPECT_CALL(*client, stop()).Times(1);
  EXPECT_CALL(srpc, srpc_free(_)).Times(1);
  EXPECT_CALL(cfg, setDeviceMode(Supla::DeviceMode::DEVICE_MODE_NORMAL))
      .Times(1);
  EXPECT_CALL(cfg, setSwUpdateBeta(false)).Times(1);
  EXPECT_CALL(cfg, commit()).Times(AtLeast(1));

  EXPECT_CALL(swUpdate, iterate())
      .WillOnce([this]() {
        swUpdate.setNewVersion("1.2.3");
        swUpdate.setAborted();
        EXPECT_TRUE(swUpdate.isSecurityOnlyOnFacade());
      })
      .WillOnce([this]() {
        swUpdate.setNewVersion("1.2.3");
        swUpdate.setFinished();
        EXPECT_TRUE(swUpdate.isSecurityOnlyOnFacade());
      });
  moveTime(5);
  time.advance(SUPLA_AUTOMATIC_OTA_CHECK_INTERVAL);
  srpcLayer
      ->updateLastResponseTime();  // cheat to not trigger connection timeout
  moveTime(5);
}

TEST_F(SwUpdateTests, SwUpdateFromCfgDeviceMode) {
  EXPECT_CALL(srpc, srpc_dcs_async_get_user_localtime(_))
      .WillRepeatedly(Return(SUPLA_RESULT_TRUE));
  EXPECT_CALL(timers, initTimers()).Times(1);
  EXPECT_CALL(cfg, getDeviceName(_)).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getGUID(_)).WillRepeatedly([](char *guid) {
    char GUID[SUPLA_GUID_SIZE] = {1};
    memcpy(guid, GUID, SUPLA_GUID_SIZE);
    return true;
  });
  EXPECT_CALL(cfg, getAuthKey(_)).WillRepeatedly([](char *auth) {
    char AUTHKEY[SUPLA_AUTHKEY_SIZE] = {2};
    memcpy(auth, AUTHKEY, SUPLA_AUTHKEY_SIZE);
    return true;
  });
  EXPECT_CALL(cfg, getSwUpdateServer(_)).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, isSwUpdateBeta()).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getDeviceMode())
      .WillRepeatedly(Return(Supla::DEVICE_MODE_SW_UPDATE));
  EXPECT_CALL(cfg, getSuplaServer(_)).WillRepeatedly([](char *server) {
    char temp[] = "mega.supla.org";
    memcpy(server, temp, strlen(temp));
    return true;
  });
  EXPECT_CALL(cfg, getEmail(_)).WillRepeatedly([](char *mail) {
    char temp[] = "ceo@supla.org";
    memcpy(mail, temp, strlen(temp));
    return true;
  });
  // ssl disabled in config is realized by setting port to 2015
  EXPECT_CALL(cfg, getSuplaServerPort()).WillRepeatedly(Return(2015));
  EXPECT_CALL(cfg, saveIfNeeded()).Times(AtLeast(1));

  EXPECT_CALL(cfg, getInt8(_, _))
      .WillRepeatedly([](const char *key, int8_t *buf) {
        if (strcmp(key, "swUpdNoCert") == 0) {
          *buf = 0;
          return true;
        }
        EXPECT_TRUE(false);
        return false;
      });

  EXPECT_CALL(cfg, getUInt8(_, _))
      .WillRepeatedly([](const char *key, uint8_t *buf) {
        if (strcmp(key, "security_level") == 0) {
          *buf = 0;
          return true;
        }
        if (strcmp(key, "ota_mode") == 0) {
          *buf = SUPLA_FIRMWARE_UPDATE_POLICY_FORCED_OFF;
          return true;
        }
        return false;
      });
  EXPECT_CALL(cfg, getWiFiSSID(_)).WillRepeatedly([](char *ssid) {
    char temp[] = "ssid_test";
    memcpy(ssid, temp, strlen(temp));
    return true;
  });
  EXPECT_CALL(cfg, getWiFiPassword(_)).WillRepeatedly([](char *pass) {
    char temp[] = "pass_test";
    memcpy(pass, temp, strlen(temp));
    return true;
  });

  EXPECT_CALL(cfg, isSuplaCommProtocolEnabled()).WillRepeatedly(Return(true));

  //  EXPECT_CALL(srpc, srpc_params_init(_));
  //  EXPECT_CALL(srpc, srpc_init(_)).WillOnce(Return(&dummy));
  //  EXPECT_CALL(srpc, srpc_set_proto_version(&dummy, 28));

  // proto version at least 23 is enforced by s-d
  sd.setAutomaticFirmwareUpdateSupported(true);
  EXPECT_TRUE(sd.begin(28));
  EXPECT_EQ(sd.getCurrentStatus(), STATUS_INITIALIZED);

  EXPECT_CALL(net, isReady()).WillRepeatedly(Return(true));
  EXPECT_CALL(net, setup()).Times(1);
  EXPECT_CALL(net, iterate()).Times(AtLeast(1));
  EXPECT_CALL(srpc, srpc_iterate(_)).WillRepeatedly(Return(SUPLA_RESULT_TRUE));

  EXPECT_CALL(*client, connected()).Times(0);
  EXPECT_CALL(*client, connectImp(_, 2015)).Times(0);
  //  EXPECT_CALL(srpc, srpc_ds_async_registerdevice_in_chunks_g(_,
  //  _)).Times(1); EXPECT_CALL(srpc, srpc_dcs_async_set_activity_timeout(_,
  //  _)).Times(1);

  //  EXPECT_CALL(srpc, srpc_rd_free(_)).Times(AtLeast(0));

  EXPECT_EQ(sd.getCurrentStatus(), STATUS_INITIALIZED);
  EXPECT_CALL(swUpdate, iterate()).WillOnce([this]() {
    swUpdate.setNewVersion("2.1.4");
    swUpdate.setFinished();
    EXPECT_FALSE(swUpdate.isSecurityOnlyOnFacade());
  });

  EXPECT_CALL(board, deviceSoftwareReset()).Times(1);
  EXPECT_CALL(*client, stop()).Times(0);
  EXPECT_CALL(srpc, srpc_free(_)).Times(0);
  EXPECT_CALL(cfg, setDeviceMode(Supla::DeviceMode::DEVICE_MODE_NORMAL))
      .Times(1);
  EXPECT_CALL(cfg, setSwUpdateBeta(false)).Times(1);
  EXPECT_CALL(cfg, commit()).Times(AtLeast(1));

  moveTime(3);
  delete client;
}

TEST_F(SwUpdateTests, AutomaticUpdateForcedOffTriggerLocally) {
  EXPECT_CALL(srpc, srpc_dcs_async_get_user_localtime(_))
      .WillRepeatedly(Return(SUPLA_RESULT_TRUE));

  EXPECT_CALL(timers, initTimers()).Times(1);
  EXPECT_CALL(cfg, getDeviceName(_)).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getGUID(_)).WillRepeatedly([](char *guid) {
    char GUID[SUPLA_GUID_SIZE] = {1};
    memcpy(guid, GUID, SUPLA_GUID_SIZE);
    return true;
  });
  EXPECT_CALL(cfg, getAuthKey(_)).WillRepeatedly([](char *auth) {
    char AUTHKEY[SUPLA_AUTHKEY_SIZE] = {2};
    memcpy(auth, AUTHKEY, SUPLA_AUTHKEY_SIZE);
    return true;
  });
  EXPECT_CALL(cfg, getSwUpdateServer(_)).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, isSwUpdateBeta()).WillRepeatedly(Return(false));
  EXPECT_CALL(cfg, getDeviceMode())
      .WillRepeatedly(Return(Supla::DEVICE_MODE_NOT_SET));
  EXPECT_CALL(cfg, getSuplaServer(_)).WillRepeatedly([](char *server) {
    char temp[] = "mega.supla.org";
    memcpy(server, temp, strlen(temp));
    return true;
  });
  EXPECT_CALL(cfg, getEmail(_)).WillRepeatedly([](char *mail) {
    char temp[] = "ceo@supla.org";
    memcpy(mail, temp, strlen(temp));
    return true;
  });
  // ssl disabled in config is realized by setting port to 2015
  EXPECT_CALL(cfg, getSuplaServerPort()).WillRepeatedly(Return(2015));
  EXPECT_CALL(cfg, saveIfNeeded()).Times(AtLeast(1));

  EXPECT_CALL(cfg, getInt8(_, _))
      .WillRepeatedly([](const char *key, int8_t *buf) {
        if (strcmp(key, "swUpdNoCert") == 0) {
          *buf = 0;
          return true;
        }
        EXPECT_TRUE(false);
        return false;
      });

  EXPECT_CALL(cfg, getUInt8(_, _))
      .WillRepeatedly([](const char *key, uint8_t *buf) {
        if (strcmp(key, "security_level") == 0) {
          *buf = 0;
          return true;
        }
        if (strcmp(key, "ota_mode") == 0) {
          *buf = SUPLA_FIRMWARE_UPDATE_POLICY_FORCED_OFF;
          return true;
        }
        return false;
      });
  EXPECT_CALL(cfg, getWiFiSSID(_)).WillRepeatedly([](char *ssid) {
    char temp[] = "ssid_test";
    memcpy(ssid, temp, strlen(temp));
    return true;
  });
  EXPECT_CALL(cfg, getWiFiPassword(_)).WillRepeatedly([](char *pass) {
    char temp[] = "pass_test";
    memcpy(pass, temp, strlen(temp));
    return true;
  });

  EXPECT_CALL(cfg, isSuplaCommProtocolEnabled()).WillRepeatedly(Return(true));

  int dummy = 0;

  EXPECT_CALL(srpc, srpc_params_init(_));
  EXPECT_CALL(srpc, srpc_init(_)).WillOnce(Return(&dummy));
  EXPECT_CALL(srpc, srpc_set_proto_version(&dummy, 28));

  // proto version at least 23 is enforced by s-d
  EXPECT_TRUE(sd.begin(28));
  EXPECT_EQ(sd.getCurrentStatus(), STATUS_INITIALIZED);

  EXPECT_CALL(net, isReady()).WillRepeatedly(Return(true));
  EXPECT_CALL(net, setup()).Times(1);
  EXPECT_CALL(net, iterate()).Times(AtLeast(1));
  EXPECT_CALL(srpc, srpc_iterate(_)).WillRepeatedly(Return(SUPLA_RESULT_TRUE));

  EXPECT_CALL(*client, connected())
      .WillOnce(Return(false))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*client, connectImp(_, 2015)).WillRepeatedly(Return(1));
  EXPECT_CALL(srpc, srpc_ds_async_registerdevice_in_chunks_g(_, _)).Times(1);
  EXPECT_CALL(srpc, srpc_dcs_async_set_activity_timeout(_, _)).Times(1);

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

  moveTime(10);

  EXPECT_EQ(sd.getCurrentStatus(), STATUS_REGISTERED_AND_READY);
  EXPECT_EQ(client->getRootCACert(), nullptr);
  sd.setAutomaticFirmwareUpdateSupported(true);

  moveTime(5);

  EXPECT_CALL(swUpdate, iterate()).WillOnce([this]() {
    swUpdate.setNewVersion("2.1.4");
    swUpdate.setFinished();
    EXPECT_FALSE(swUpdate.isSecurityOnlyOnFacade());
  });

  EXPECT_CALL(board, deviceSoftwareReset()).Times(1);
  EXPECT_CALL(*client, stop()).Times(1);
  EXPECT_CALL(srpc, srpc_free(_)).Times(1);
  EXPECT_CALL(cfg, setDeviceMode(Supla::DeviceMode::DEVICE_MODE_NORMAL))
      .Times(1);
  EXPECT_CALL(cfg, setSwUpdateBeta(false)).Times(1);
  EXPECT_CALL(cfg, commit()).Times(AtLeast(1));

  sd.handleAction(0, Supla::CHECK_SW_UPDATE);
  moveTime(3);
}

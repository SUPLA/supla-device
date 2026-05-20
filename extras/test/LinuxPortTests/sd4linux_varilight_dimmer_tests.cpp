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
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <network_client_mock.h>
#include <simple_time.h>
#include <srpc_mock.h>
#include <supla/channels/channel.h>
#include <supla/clock/clock.h>
#include <supla/control/varilight_dimmer.h>
#include <supla/element.h>
#include <supla/protocol/supla_srpc.h>

#include <cstring>

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::NiceMock;
using ::testing::Return;

namespace {

class VarilightSrpcLayerTestStub : public Supla::Protocol::SuplaSrpc {
 public:
  explicit VarilightSrpcLayerTestStub(SuplaDeviceClass *sdc)
      : Supla::Protocol::SuplaSrpc(sdc) {
  }

  using Supla::Protocol::SuplaSrpc::deinitializeSrpc;
  using Supla::Protocol::SuplaSrpc::initializeSrpc;

  void setRegisteredAndReady() {
    registered = 1;
  }
};

class Sd4linuxVarilightDimmerTests : public ::testing::Test {
 protected:
  void SetUp() override {
    Supla::Channel::resetToDefaults();
  }

  void TearDown() override {
    Supla::Channel::resetToDefaults();
  }

  static TSD_DeviceCalCfgRequest makeRequest(int32_t command,
                                             int dataSize = 0) {
    TSD_DeviceCalCfgRequest request = {};
    request.ChannelNumber = 0;
    request.SenderID = 1234;
    request.SuperUserAuthorized = 1;
    request.Command = command;
    request.DataSize = dataSize;
    return request;
  }

  static void setRequestByte(TSD_DeviceCalCfgRequest *request, uint8_t value) {
    request->DataSize = 1;
    request->Data[0] = static_cast<char>(value);
  }

  static void setRequestUint16(TSD_DeviceCalCfgRequest *request,
                               uint16_t value) {
    request->DataSize = 2;
    request->Data[0] = static_cast<char>(value & 0xFF);
    request->Data[1] = static_cast<char>((value >> 8) & 0xFF);
  }
};

class Sd4linuxVarilightDimmerSrpcTests : public ::testing::Test {
 protected:
  SimpleTime time;
  NiceMock<SrpcMock> srpc;
  SuplaDeviceClass sd;
  NetworkClientMock *client = nullptr;
  VarilightSrpcLayerTestStub *srpcLayer = nullptr;
  Supla::Control::VarilightDimmer *dimmer = nullptr;

  void SetUp() override {
    if (SuplaDevice.getClock()) {
      delete SuplaDevice.getClock();
    }
    Supla::Channel::resetToDefaults();

    client = new NetworkClientMock;
    srpcLayer = new VarilightSrpcLayerTestStub(&sd);
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

    dimmer = new Supla::Control::VarilightDimmer(0);
    dimmer->setEdgeMinimum(11);
    dimmer->setEdgeMaximum(999);
    dimmer->setOperatingMinimum(123);
    dimmer->setOperatingMaximum(987);
    dimmer->setMode(2);
    dimmer->setBoost(1);
    dimmer->setBoostLevel(250);
    dimmer->setLedConfig(2);
    dimmer->setPicInstalledHexVersion("1.2.3");
    dimmer->onRegistered(srpcLayer);
  }

  void TearDown() override {
    delete dimmer;
    dimmer = nullptr;

    if (srpcLayer) {
      srpcLayer->deinitializeSrpc();
      delete srpcLayer;
      srpcLayer = nullptr;
    }
    Supla::Channel::resetToDefaults();
  }
};

}  // namespace

TEST_F(Sd4linuxVarilightDimmerTests, CreatesDimmerChannel) {
  Supla::Control::VarilightDimmer dimmer;

  EXPECT_EQ(dimmer.getChannel()->getChannelType(), SUPLA_CHANNELTYPE_DIMMER);
  EXPECT_EQ(dimmer.getChannel()->getDefaultFunction(),
            SUPLA_CHANNELFNC_DIMMER);
  EXPECT_EQ(dimmer.getLedConfig(),
            Supla::Control::Varilight::DefaultLedConfig);
}

TEST_F(Sd4linuxVarilightDimmerTests, AppliesBrightnessFromServerValue) {
  Supla::Control::VarilightDimmer dimmer;
  TSD_SuplaChannelNewValue value = {};

  value.value[0] = 120;
  EXPECT_EQ(dimmer.handleNewValueFromServer(&value), -1);
  EXPECT_EQ(dimmer.getBrightness(), 100);
  EXPECT_EQ(dimmer.getChannel()->getValueBrightness(), 100);

  value = {};
  value.value[0] = 44;
  EXPECT_EQ(dimmer.handleNewValueFromServer(&value), -1);
  EXPECT_EQ(dimmer.getBrightness(), 44);
  EXPECT_EQ(dimmer.getChannel()->getValueBrightness(), 44);

  value = {};
  value.value[5] = 1;
  EXPECT_EQ(dimmer.handleNewValueFromServer(&value), -1);
  EXPECT_EQ(dimmer.getBrightness(), 0);
  EXPECT_EQ(dimmer.getChannel()->getValueBrightness(), 0);
}

TEST_F(Sd4linuxVarilightDimmerTests, HandlesCalcfgAuthorizationAndUnknown) {
  Supla::Control::VarilightDimmer dimmer;

  EXPECT_EQ(dimmer.handleCalcfgFromServer(nullptr),
            SUPLA_CALCFG_RESULT_UNAUTHORIZED);

  auto request = makeRequest(Supla::Control::Varilight::MsgSetMinimum);
  request.SuperUserAuthorized = 0;
  setRequestUint16(&request, 10);
  EXPECT_EQ(dimmer.handleCalcfgFromServer(&request),
            SUPLA_CALCFG_RESULT_UNAUTHORIZED);

  request = makeRequest(0x123456);
  EXPECT_EQ(dimmer.handleCalcfgFromServer(&request),
            SUPLA_CALCFG_RESULT_NOT_SUPPORTED);
}

TEST_F(Sd4linuxVarilightDimmerTests, AppliesAndCompletesConfigurationSession) {
  Supla::Control::VarilightDimmer dimmer;
  dimmer.setOperatingMinimum(100);
  dimmer.setLedConfig(2);

  auto request = makeRequest(Supla::Control::Varilight::MsgConfigurationMode);
  EXPECT_EQ(dimmer.handleCalcfgFromServer(&request),
            SUPLA_SRPC_CALCFG_RESULT_PENDING);

  request = makeRequest(Supla::Control::Varilight::MsgSetMinimum);
  setRequestUint16(&request, 222);
  EXPECT_EQ(dimmer.handleCalcfgFromServer(&request),
            SUPLA_CALCFG_RESULT_TRUE);

  request = makeRequest(Supla::Control::Varilight::CalcfgSetLedConfig);
  setRequestByte(&request, 0);
  EXPECT_EQ(dimmer.handleCalcfgFromServer(&request),
            SUPLA_CALCFG_RESULT_TRUE);

  auto report = dimmer.getSuplaConfiguration();
  EXPECT_EQ(report.mainConfig.operatingMinimum, 222);
  EXPECT_EQ(report.led, 0);
  EXPECT_EQ(dimmer.getConfiguration().operatingMinimum, 100);
  EXPECT_EQ(dimmer.getLedConfig(), 2);

  request = makeRequest(Supla::Control::Varilight::MsgConfigComplete);
  setRequestByte(&request, 0);
  EXPECT_EQ(dimmer.handleCalcfgFromServer(&request),
            SUPLA_CALCFG_RESULT_TRUE);
  EXPECT_EQ(dimmer.getConfiguration().operatingMinimum, 100);
  EXPECT_EQ(dimmer.getLedConfig(), 2);

  request = makeRequest(Supla::Control::Varilight::MsgConfigurationMode);
  EXPECT_EQ(dimmer.handleCalcfgFromServer(&request),
            SUPLA_SRPC_CALCFG_RESULT_PENDING);

  request = makeRequest(Supla::Control::Varilight::MsgSetMinimum);
  setRequestUint16(&request, 333);
  EXPECT_EQ(dimmer.handleCalcfgFromServer(&request),
            SUPLA_CALCFG_RESULT_TRUE);

  request = makeRequest(Supla::Control::Varilight::CalcfgSetLedConfig);
  setRequestByte(&request, 1);
  EXPECT_EQ(dimmer.handleCalcfgFromServer(&request),
            SUPLA_CALCFG_RESULT_TRUE);

  request = makeRequest(Supla::Control::Varilight::MsgConfigComplete);
  setRequestByte(&request, 1);
  EXPECT_EQ(dimmer.handleCalcfgFromServer(&request),
            SUPLA_CALCFG_RESULT_TRUE);
  EXPECT_EQ(dimmer.getConfiguration().operatingMinimum, 333);
  EXPECT_EQ(dimmer.getLedConfig(), 1);
}

TEST_F(Sd4linuxVarilightDimmerTests, KeepsConfigOnInvalidCalcfgPayloadSize) {
  Supla::Control::VarilightDimmer dimmer;
  dimmer.setOperatingMinimum(100);

  auto request = makeRequest(Supla::Control::Varilight::MsgSetMinimum);
  setRequestByte(&request, 55);

  EXPECT_EQ(dimmer.handleCalcfgFromServer(&request),
            SUPLA_CALCFG_RESULT_DONE);
  EXPECT_EQ(dimmer.getConfiguration().operatingMinimum, 100);
}

TEST_F(Sd4linuxVarilightDimmerSrpcTests,
       ConfigurationModeSendsPendingConfigurationReport) {
  TSD_DeviceCalCfgRequest request = {};
  request.ChannelNumber = 0;
  request.SenderID = 1234;
  request.SuperUserAuthorized = 1;
  request.Command = Supla::Control::Varilight::MsgConfigurationMode;

  EXPECT_CALL(srpc, srpc_getdata(_, _, _))
      .WillOnce(
          [&request](void *, TsrpcReceivedData *rd, unsigned _supla_int_t) {
            memset(rd, 0, sizeof(TsrpcReceivedData));
            rd->call_id = SUPLA_SD_CALL_DEVICE_CALCFG_REQUEST;
            rd->data.sd_device_calcfg_request = &request;
            return SUPLA_RESULT_TRUE;
          });

  Supla::messageReceived(nullptr, 0, 0, srpcLayer, 28);
  ASSERT_NE(srpcLayer->calCfgResultPending.get(
                0, Supla::Control::Varilight::MsgConfigurationMode),
            nullptr);

  EXPECT_CALL(srpc, srpc_ds_async_device_calcfg_result(_, _))
      .WillOnce([](void *, TDS_DeviceCalCfgResult *result) -> _supla_int_t {
        EXPECT_EQ(result->ChannelNumber, 0);
        EXPECT_EQ(result->ReceiverID, 1234);
        EXPECT_EQ(result->Command,
                  Supla::Control::Varilight::MsgConfigurationAck);
        EXPECT_EQ(result->Result, SUPLA_RESULTCODE_TRUE);
        EXPECT_EQ(result->DataSize, 0);
        return 0;
      });
  dimmer->iterateAlways();

  EXPECT_CALL(srpc, srpc_ds_async_device_calcfg_result(_, _))
      .WillOnce([](void *, TDS_DeviceCalCfgResult *result) -> _supla_int_t {
        EXPECT_EQ(result->ChannelNumber, 0);
        EXPECT_EQ(result->ReceiverID, 1234);
        EXPECT_EQ(result->Command,
                  Supla::Control::Varilight::MsgConfigurationReport);
        EXPECT_EQ(result->Result, SUPLA_RESULTCODE_TRUE);
        EXPECT_EQ(result->DataSize,
                  sizeof(Supla::Control::Varilight::SuplaConfiguration));

        if (result->DataSize ==
            sizeof(Supla::Control::Varilight::SuplaConfiguration)) {
          Supla::Control::Varilight::SuplaConfiguration config = {};
          memcpy(&config, result->Data, sizeof(config));
          EXPECT_EQ(config.cfgVersion,
                    Supla::Control::Varilight::ConfigVersion);
          EXPECT_EQ(config.mainConfig.edgeMinimum, 11);
          EXPECT_EQ(config.mainConfig.edgeMaximum, 999);
          EXPECT_EQ(config.mainConfig.operatingMinimum, 123);
          EXPECT_EQ(config.mainConfig.operatingMaximum, 987);
          EXPECT_EQ(config.mainConfig.mode, 2);
          EXPECT_EQ(config.mainConfig.boost, 1);
          EXPECT_EQ(config.mainConfig.boostLevel, 250);
          EXPECT_EQ(config.led, 2);
          EXPECT_STREQ(config.picInstalledHexVer, "1.2.3");
        }
        return 0;
      });

  dimmer->iterateAlways();
}

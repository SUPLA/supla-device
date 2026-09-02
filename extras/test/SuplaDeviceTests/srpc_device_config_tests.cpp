// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <SuplaDevice.h>
#include <config_mock.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <network_client_mock.h>
#include <simple_time.h>
#include <srpc_mock.h>
#include <supla/device/register_device.h>
#include <supla/device/remote_device_config.h>
#include <supla/protocol/supla_srpc.h>
#include <supla/storage/config_tags.h>

using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StrEq;

namespace {

class TestSrpc : public Supla::Protocol::SuplaSrpc {
 public:
  explicit TestSrpc(SuplaDeviceClass *sdc)
      : Supla::Protocol::SuplaSrpc(sdc) {
  }

  void initializeSrpcForTest() {
    initializeSrpc();
  }

  void setRegisteredForTest(bool value) {
    registered = value ? 1 : 0;
  }

  void setDeviceConfigReceivedForTest(bool value) {
    setDeviceConfigReceivedAfterRegistration = value;
  }

  uint32_t waitForIterateForTest() const {
    return waitForIterate;
  }
};

class SuplaSrpcDeviceConfigTests : public ::testing::Test {
 protected:
  SimpleTime time;

  void SetUp() override {
    registeredFields =
        Supla::Device::RemoteDeviceConfig::GetRegisteredConfigFieldsForTests();
    properties = Supla::Device::RemoteDeviceConfig::
        GetInputActivationPropertiesForTests();
    Supla::Device::RemoteDeviceConfig::SetRegisteredConfigFieldsForTests(0);
    Supla::Device::RemoteDeviceConfig::ClearResendAttemptsCounter();
    Supla::Device::RemoteDeviceConfig::SetInputActivationPropertiesForTests(
        {});
    Supla::RegisterDevice::resetToDefaults();
    Supla::RegisterDevice::setServerName("supla.example");
    Supla::RegisterDevice::setEmail("user@example.com");
    Supla::RegisterDevice::addFlags(
        SUPLA_DEVICE_FLAG_DEVICE_CONFIG_SUPPORTED);
  }

  void TearDown() override {
    Supla::RegisterDevice::resetToDefaults();
    Supla::Device::RemoteDeviceConfig::SetRegisteredConfigFieldsForTests(
        registeredFields);
    Supla::Device::RemoteDeviceConfig::SetInputActivationPropertiesForTests(
        properties);
  }

  uint64_t registeredFields = 0;
  Supla::Device::InputActivationProperties properties = {};
};

}  // namespace

TEST_F(SuplaSrpcDeviceConfigTests,
       ResponseSerializationFailureFinishesFirstTransaction) {
  NiceMock<SrpcMock> srpcMock;
  NiceMock<ConfigMock> storage;
  SuplaDeviceClass device;
  Supla::Device::RemoteDeviceConfig::RegisterConfigField(
      SUPLA_DEVICE_CONFIG_FIELD_INPUT_ACTIVATION);

  EXPECT_CALL(srpcMock, setDeviceConfigResult(_))
      .WillOnce(Return(static_cast<_supla_int_t>(0)));

  Supla::Protocol::SuplaSrpc protocol(&device);
  TSDS_SetDeviceConfig request = {};
  request.EndOfDataFlag = 1;

  protocol.handleDeviceConfig(&request);

  EXPECT_FALSE(protocol.isUpdatePending());
}

TEST_F(SuplaSrpcDeviceConfigTests,
       LocalSerializationFailureKeepsFlagAndRetriesAfterCleanup) {
  NiceMock<SrpcMock> srpcMock;
  NiceMock<ConfigMock> storage;
  auto *client = new NiceMock<NetworkClientMock>;
  SuplaDeviceClass device;
  TestSrpc protocol(&device);

  Supla::Device::RemoteDeviceConfig::RegisterConfigField(
      SUPLA_DEVICE_CONFIG_FIELD_INPUT_ACTIVATION);
  EXPECT_CALL(storage,
              getUInt8(StrEq(Supla::ConfigTag::DeviceConfigChangeCfgTag), _))
      .WillOnce(Invoke([](const char *, uint8_t *value) {
        *value = 1;
        return true;
      }));
  EXPECT_CALL(storage,
              setUInt8(StrEq(Supla::ConfigTag::DeviceConfigChangeCfgTag), 0))
      .WillOnce(Return(true));
  EXPECT_CALL(storage, saveWithDelay(1000)).Times(1);
  EXPECT_CALL(*client, connected()).WillRepeatedly(Return(1));
  EXPECT_CALL(srpcMock, srpc_iterate(_))
      .Times(2)
      .WillRepeatedly(Return(SUPLA_RESULT_TRUE));

  protocol.setNetworkClient(client);
  protocol.initializeSrpcForTest();
  protocol.setRegisteredForTest(true);
  protocol.setDeviceConfigReceivedForTest(true);

  EXPECT_TRUE(protocol.iterate(1000));
  EXPECT_EQ(protocol.waitForIterateForTest(), 1000U);
  EXPECT_TRUE(storage.isDeviceConfigChangeReadyToSend());

  Supla::Device::RemoteDeviceConfig::SetInputActivationPropertiesForTests({
      .availableModes = SUPLA_DEVCFG_INPUT_ACTIVATION_GND |
                        SUPLA_DEVCFG_INPUT_ACTIVATION_VCC,
      .defaultMode = SUPLA_DEVCFG_INPUT_ACTIVATION_GND,
  });
  EXPECT_CALL(storage,
              getBlob(StrEq(Supla::ConfigTag::InputActivationCfgTag), _, _))
      .WillOnce(Return(false));
  EXPECT_CALL(srpcMock, setDeviceConfigRequest(_))
      .WillOnce(Return(static_cast<_supla_int_t>(0)));

  EXPECT_FALSE(protocol.iterate(1500));
  EXPECT_TRUE(protocol.iterate(2001));
  EXPECT_TRUE(storage.isDeviceConfigChangeReadyToSend());

  TSDS_SetDeviceConfigResult result = {};
  result.Result = SUPLA_CONFIG_RESULT_TRUE;
  protocol.handleSetDeviceConfigResult(&result);

  EXPECT_FALSE(storage.isDeviceConfigChangeFlagSet());
}

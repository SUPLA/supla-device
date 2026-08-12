// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <config_mock.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <supla-common/proto.h>
#include <supla/device/remote_device_config.h>
#include <supla/device/thermal_protection_config.h>
#include <supla/element.h>
#include <supla/storage/config_tags.h>

#include <cstring>

using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StrEq;

namespace {

class ConfigChangeObserver : public Supla::Element {
 public:
  void onDeviceConfigChange(uint64_t fieldBit) override {
    if (fieldBit == SUPLA_DEVICE_CONFIG_FIELD_THERMAL_PROTECTION) {
      notificationCount++;
    }
  }

  int notificationCount = 0;
};

class ThermalProtectionDeviceConfigTests : public ::testing::Test {
 protected:
  void SetUp() override {
    registeredFields =
        Supla::Device::RemoteDeviceConfig::GetRegisteredConfigFieldsForTests();
    Supla::Device::RemoteDeviceConfig::SetRegisteredConfigFieldsForTests(
        registeredFields &
        ~SUPLA_DEVICE_CONFIG_FIELD_THERMAL_PROTECTION);
  }

  void TearDown() override {
    Supla::Device::RemoteDeviceConfig::SetRegisteredConfigFieldsForTests(
        registeredFields);
  }

 private:
  uint64_t registeredFields = 0;
};

}  // namespace

TEST_F(ThermalProtectionDeviceConfigTests,
       StoresWritableBlobAndKeepsDevicePropertiesAuthoritative) {
  NiceMock<ConfigMock> storage;
  ConfigChangeObserver observer;
  Supla::Device::ThermalProtectionConfig storedConfig = {
      .threshold = 200,
      .enabled = 0,
  };
  const Supla::Device::ThermalProtectionProperties properties = {
      .minThreshold = 50,
      .maxThreshold = 300,
      .disableAllowed = 1,
  };

  Supla::Device::RemoteDeviceConfig::ClearResendAttemptsCounter();
  Supla::Device::RemoteDeviceConfig::SetThermalProtectionProperties(properties);

  EXPECT_CALL(storage,
              getBlob(StrEq(Supla::ConfigTag::ThermalProtectionCfgTag),
                      _,
                      sizeof(storedConfig)))
      .Times(2)
      .WillRepeatedly(
          Invoke([&storedConfig](const char *, char *blob, size_t blobSize) {
            std::memcpy(blob, &storedConfig, blobSize);
            return true;
          }));
  EXPECT_CALL(storage,
              setBlob(StrEq(Supla::ConfigTag::ThermalProtectionCfgTag),
                      _,
                      sizeof(storedConfig)))
      .WillOnce(Invoke(
          [&storedConfig](const char *, const char *blob, size_t blobSize) {
            std::memcpy(&storedConfig, blob, blobSize);
            return true;
          }));
  EXPECT_CALL(storage, saveWithDelay(1000)).Times(1);
  EXPECT_CALL(storage,
              setUInt8(StrEq(Supla::ConfigTag::DeviceConfigChangeCfgTag), _))
      .Times(0);

  TSDS_SetDeviceConfig incoming = {};
  incoming.EndOfDataFlag = 1;
  incoming.AvailableFields = SUPLA_DEVICE_CONFIG_FIELD_THERMAL_PROTECTION;
  incoming.Fields = SUPLA_DEVICE_CONFIG_FIELD_THERMAL_PROTECTION;
  incoming.ConfigSize = sizeof(TDeviceConfig_ThermalProtection);
  auto *thermal =
      reinterpret_cast<TDeviceConfig_ThermalProtection *>(incoming.Config);
  thermal->Threshold = 250;
  thermal->Enabled = 1;
  // Stale server-side copies of readonly properties.
  thermal->MinThreshold = 0;
  thermal->MaxThreshold = 0;
  thermal->DisableAllowed = 0;

  {
    Supla::Device::RemoteDeviceConfig remoteConfig;
    remoteConfig.processConfig(&incoming);

    EXPECT_EQ(remoteConfig.getResultCode(), SUPLA_CONFIG_RESULT_TRUE);
    EXPECT_EQ(storedConfig.threshold, 200);
    EXPECT_EQ(storedConfig.enabled, 0);
    EXPECT_EQ(observer.notificationCount, 0);
  }

  Supla::Device::RemoteDeviceConfig::RegisterConfigField(
      SUPLA_DEVICE_CONFIG_FIELD_THERMAL_PROTECTION);

  Supla::Device::RemoteDeviceConfig remoteConfig;
  remoteConfig.processConfig(&incoming);

  EXPECT_EQ(remoteConfig.getResultCode(), SUPLA_CONFIG_RESULT_TRUE);
  EXPECT_EQ(storedConfig.threshold, 250);
  EXPECT_EQ(storedConfig.enabled, 1);
  EXPECT_EQ(observer.notificationCount, 1);
  EXPECT_TRUE(remoteConfig.isSetDeviceConfigRequired());

  TSDS_SetDeviceConfig outgoing = {};
  ASSERT_TRUE(remoteConfig.fillSetDeviceConfig(&outgoing));
  ASSERT_EQ(outgoing.Fields, SUPLA_DEVICE_CONFIG_FIELD_THERMAL_PROTECTION);
  ASSERT_EQ(outgoing.ConfigSize, sizeof(TDeviceConfig_ThermalProtection));
  thermal =
      reinterpret_cast<TDeviceConfig_ThermalProtection *>(outgoing.Config);
  EXPECT_EQ(thermal->Threshold, 250);
  EXPECT_EQ(thermal->Enabled, 1);
  EXPECT_EQ(thermal->MinThreshold, properties.minThreshold);
  EXPECT_EQ(thermal->MaxThreshold, properties.maxThreshold);
  EXPECT_EQ(thermal->DisableAllowed, properties.disableAllowed);
}

// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <config_mock.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <supla-common/proto.h>
#include <supla/device/input_activation_config.h>
#include <supla/device/remote_device_config.h>
#include <supla/device/thermal_protection_config.h>
#include <supla/element.h>
#include <supla/storage/config_tags.h>

#include <cstring>

using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::StrEq;

namespace {

constexpr uint8_t kGnd = SUPLA_DEVCFG_INPUT_ACTIVATION_GND;
constexpr uint8_t kVcc = SUPLA_DEVCFG_INPUT_ACTIVATION_VCC;
constexpr uint8_t kBoth = kGnd | kVcc;

class ConfigChangeObserver : public Supla::Element {
 public:
  void onDeviceConfigChange(uint64_t fieldBit) override {
    if (fieldBit == SUPLA_DEVICE_CONFIG_FIELD_INPUT_ACTIVATION) {
      notificationCount++;
    }
  }

  int notificationCount = 0;
};

class InputActivationDeviceConfigTests : public ::testing::Test {
 protected:
  void SetUp() override {
    registeredFields =
        Supla::Device::RemoteDeviceConfig::GetRegisteredConfigFieldsForTests();
    properties = Supla::Device::RemoteDeviceConfig::
        GetInputActivationPropertiesForTests();
    Supla::Device::RemoteDeviceConfig::SetRegisteredConfigFieldsForTests(0);
    Supla::Device::RemoteDeviceConfig::ClearResendAttemptsCounter();
    setProperties(kBoth, kGnd);
  }

  void TearDown() override {
    Supla::Device::RemoteDeviceConfig::SetRegisteredConfigFieldsForTests(
        registeredFields);
    Supla::Device::RemoteDeviceConfig::SetInputActivationPropertiesForTests(
        properties);
  }

  void setProperties(uint8_t availableModes, uint8_t defaultMode) {
    Supla::Device::RemoteDeviceConfig::SetInputActivationProperties({
        .availableModes = availableModes,
        .defaultMode = defaultMode,
    });
  }

  TSDS_SetDeviceConfig incoming(
      uint8_t availableModes,
      uint8_t mode,
      uint64_t fields = SUPLA_DEVICE_CONFIG_FIELD_INPUT_ACTIVATION) {
    TSDS_SetDeviceConfig result = {};
    result.EndOfDataFlag = 1;
    result.AvailableFields = fields;
    result.Fields = fields;
    result.ConfigSize = sizeof(TDeviceConfig_InputActivation);
    auto *config =
        reinterpret_cast<TDeviceConfig_InputActivation *>(result.Config);
    config->AvailableModes = availableModes;
    config->Mode = mode;
    return result;
  }

  uint64_t registeredFields = 0;
  Supla::Device::InputActivationProperties properties = {};
};

void expectInputBlob(NiceMock<ConfigMock> &storage,
                     Supla::Device::InputActivationConfig *storedConfig,
                     bool exists) {
  EXPECT_CALL(storage,
              getBlob(StrEq(Supla::ConfigTag::InputActivationCfgTag),
                      _,
                      sizeof(*storedConfig)))
      .WillRepeatedly(Invoke(
          [storedConfig, exists](const char *, char *blob, size_t blobSize) {
            if (exists) {
              std::memcpy(blob, storedConfig, blobSize);
            }
            return exists;
          }));
}

}  // namespace

TEST_F(InputActivationDeviceConfigTests, UnregisteredFieldIsIgnored) {
  NiceMock<ConfigMock> storage;
  ConfigChangeObserver observer;
  auto config = incoming(kBoth, kVcc);

  EXPECT_CALL(storage,
              setBlob(StrEq(Supla::ConfigTag::InputActivationCfgTag), _, _))
      .Times(0);

  Supla::Device::RemoteDeviceConfig remoteConfig;
  remoteConfig.processConfig(&config);

  EXPECT_EQ(remoteConfig.getResultCode(), SUPLA_CONFIG_RESULT_TRUE);
  EXPECT_FALSE(remoteConfig.isSetDeviceConfigRequired());
  EXPECT_EQ(observer.notificationCount, 0);
}

TEST_F(InputActivationDeviceConfigTests,
       RegisteredFieldWithInvalidPropertiesIsNotSerialized) {
  Supla::Device::RemoteDeviceConfig::SetInputActivationPropertiesForTests({});
  Supla::Device::RemoteDeviceConfig::RegisterConfigField(
      SUPLA_DEVICE_CONFIG_FIELD_INPUT_ACTIVATION);

  TSDS_SetDeviceConfig output = {};
  Supla::Device::RemoteDeviceConfig remoteConfig;

  EXPECT_FALSE(remoteConfig.fillSetDeviceConfig(&output));
}

TEST_F(InputActivationDeviceConfigTests,
       FirstRegistrationServerOnlyFieldProducesEmptySupportedResponse) {
  NiceMock<ConfigMock> storage;
  Supla::Device::RemoteDeviceConfig::SetInputActivationPropertiesForTests({});

  TSDS_SetDeviceConfig incoming = {};
  incoming.EndOfDataFlag = 1;
  incoming.AvailableFields = SUPLA_DEVICE_CONFIG_FIELD_INPUT_ACTIVATION;

  Supla::Device::RemoteDeviceConfig remoteConfig(true);
  remoteConfig.processConfig(&incoming);

  EXPECT_TRUE(remoteConfig.isSetDeviceConfigRequired());

  TSDS_SetDeviceConfig output = {};
  ASSERT_TRUE(remoteConfig.fillSetDeviceConfig(&output));
  EXPECT_EQ(output.Fields, 0);
  EXPECT_EQ(output.AvailableFields, 0);
  EXPECT_EQ(output.ConfigSize, 0);
}

TEST_F(InputActivationDeviceConfigTests,
       FirstRegistrationSerializesOnlySupportedMismatchedFields) {
  NiceMock<ConfigMock> storage;
  Supla::Device::RemoteDeviceConfig::SetInputActivationPropertiesForTests({});
  Supla::Device::RemoteDeviceConfig::SetThermalProtectionProperties({
      .minThreshold = 50,
      .maxThreshold = 300,
      .disableAllowed = 1,
  });
  Supla::Device::RemoteDeviceConfig::RegisterConfigField(
      SUPLA_DEVICE_CONFIG_FIELD_THERMAL_PROTECTION);

  EXPECT_CALL(storage,
              getBlob(StrEq(Supla::ConfigTag::ThermalProtectionCfgTag), _,
                      sizeof(Supla::Device::ThermalProtectionConfig)))
      .WillOnce(Invoke([](const char *, char *, size_t) { return false; }));

  TSDS_SetDeviceConfig incoming = {};
  incoming.EndOfDataFlag = 1;
  incoming.AvailableFields = SUPLA_DEVICE_CONFIG_FIELD_INPUT_ACTIVATION;

  Supla::Device::RemoteDeviceConfig remoteConfig(true);
  remoteConfig.processConfig(&incoming);

  TSDS_SetDeviceConfig output = {};
  ASSERT_TRUE(remoteConfig.fillSetDeviceConfig(&output));
  EXPECT_EQ(output.Fields, SUPLA_DEVICE_CONFIG_FIELD_THERMAL_PROTECTION);
  EXPECT_EQ(output.AvailableFields,
            SUPLA_DEVICE_CONFIG_FIELD_THERMAL_PROTECTION);
  EXPECT_EQ(output.ConfigSize, sizeof(TDeviceConfig_ThermalProtection));
}

TEST_F(InputActivationDeviceConfigTests, RegisteredFieldAcceptsGnd) {
  NiceMock<ConfigMock> storage;
  ConfigChangeObserver observer;
  Supla::Device::InputActivationConfig storedConfig = {.mode = kVcc};
  expectInputBlob(storage, &storedConfig, true);
  Supla::Device::RemoteDeviceConfig::RegisterConfigField(
      SUPLA_DEVICE_CONFIG_FIELD_INPUT_ACTIVATION);
  auto config = incoming(kBoth, kGnd);

  EXPECT_CALL(storage,
              setBlob(StrEq(Supla::ConfigTag::InputActivationCfgTag),
                      _,
                      sizeof(storedConfig)))
      .WillOnce(Invoke(
          [&storedConfig](const char *, const char *blob, size_t blobSize) {
            std::memcpy(&storedConfig, blob, blobSize);
            return true;
          }));
  EXPECT_CALL(storage, saveWithDelay(1000));

  Supla::Device::RemoteDeviceConfig remoteConfig;
  remoteConfig.processConfig(&config);

  EXPECT_EQ(storedConfig.mode, kGnd);
  EXPECT_EQ(observer.notificationCount, 1);
}

TEST_F(InputActivationDeviceConfigTests, RegisteredFieldAcceptsVcc) {
  NiceMock<ConfigMock> storage;
  Supla::Device::InputActivationConfig storedConfig = {.mode = kGnd};
  expectInputBlob(storage, &storedConfig, true);
  Supla::Device::RemoteDeviceConfig::RegisterConfigField(
      SUPLA_DEVICE_CONFIG_FIELD_INPUT_ACTIVATION);
  auto config = incoming(kBoth, kVcc);

  EXPECT_CALL(storage,
              setBlob(StrEq(Supla::ConfigTag::InputActivationCfgTag),
                      _,
                      sizeof(storedConfig)))
      .WillOnce(Invoke(
          [&storedConfig](const char *, const char *blob, size_t blobSize) {
            std::memcpy(&storedConfig, blob, blobSize);
            return true;
          }));
  EXPECT_CALL(storage, saveWithDelay(1000));

  Supla::Device::RemoteDeviceConfig remoteConfig;
  remoteConfig.processConfig(&config);

  EXPECT_EQ(storedConfig.mode, kVcc);
}

TEST_F(InputActivationDeviceConfigTests,
       GndOnlyAndVccOnlyCapabilitiesAreSupported) {
  for (const auto &properties : {
           Supla::Device::InputActivationProperties{.availableModes = kGnd,
                                                    .defaultMode = kGnd},
           Supla::Device::InputActivationProperties{.availableModes = kVcc,
                                                    .defaultMode = kVcc},
       }) {
    Supla::Device::RemoteDeviceConfig::SetInputActivationProperties(properties);
    Supla::Device::RemoteDeviceConfig::RegisterConfigField(
        SUPLA_DEVICE_CONFIG_FIELD_INPUT_ACTIVATION);
    NiceMock<ConfigMock> storage;
    TSDS_SetDeviceConfig output = {};

    EXPECT_CALL(storage,
                getBlob(StrEq(Supla::ConfigTag::InputActivationCfgTag),
                        _,
                        sizeof(Supla::Device::InputActivationConfig)))
        .WillOnce(Invoke([](const char *, char *, size_t) { return false; }));

    Supla::Device::RemoteDeviceConfig remoteConfig;
    ASSERT_TRUE(remoteConfig.fillSetDeviceConfig(&output));
    auto *config =
        reinterpret_cast<TDeviceConfig_InputActivation *>(output.Config);
    EXPECT_EQ(config->AvailableModes, properties.availableModes);
    EXPECT_EQ(config->Mode, properties.defaultMode);
  }
}

TEST_F(InputActivationDeviceConfigTests,
       MissingBlobUsesDefaultAndReservedBytesAreZero) {
  NiceMock<ConfigMock> storage;
  Supla::Device::RemoteDeviceConfig::SetInputActivationProperties({
      .availableModes = kBoth,
      .defaultMode = kVcc,
  });
  Supla::Device::RemoteDeviceConfig::RegisterConfigField(
      SUPLA_DEVICE_CONFIG_FIELD_INPUT_ACTIVATION);
  EXPECT_CALL(storage,
              getBlob(StrEq(Supla::ConfigTag::InputActivationCfgTag), _, _))
      .WillOnce(Invoke([](const char *, char *, size_t) { return false; }));

  TSDS_SetDeviceConfig output = {};
  std::memset(output.Config, 0xA5, sizeof(output.Config));
  Supla::Device::RemoteDeviceConfig remoteConfig;
  ASSERT_TRUE(remoteConfig.fillSetDeviceConfig(&output));
  auto *config =
      reinterpret_cast<TDeviceConfig_InputActivation *>(output.Config);

  EXPECT_EQ(config->AvailableModes, kBoth);
  EXPECT_EQ(config->Mode, kVcc);
  for (auto reserved : config->Reserved) {
    EXPECT_EQ(reserved, 0);
  }
}

TEST_F(InputActivationDeviceConfigTests, InvalidStoredModeFallsBackToDefault) {
  NiceMock<ConfigMock> storage;
  Supla::Device::RemoteDeviceConfig::RegisterConfigField(
      SUPLA_DEVICE_CONFIG_FIELD_INPUT_ACTIVATION);
  Supla::Device::InputActivationConfig storedConfig = {.mode = kBoth};
  expectInputBlob(storage, &storedConfig, true);

  TSDS_SetDeviceConfig output = {};
  Supla::Device::RemoteDeviceConfig remoteConfig;
  ASSERT_TRUE(remoteConfig.fillSetDeviceConfig(&output));
  auto *config =
      reinterpret_cast<TDeviceConfig_InputActivation *>(output.Config);
  EXPECT_EQ(config->Mode, kGnd);
}

TEST_F(InputActivationDeviceConfigTests, OutgoingUsesStoredValidMode) {
  NiceMock<ConfigMock> storage;
  Supla::Device::RemoteDeviceConfig::RegisterConfigField(
      SUPLA_DEVICE_CONFIG_FIELD_INPUT_ACTIVATION);
  Supla::Device::InputActivationConfig storedConfig = {.mode = kVcc};
  expectInputBlob(storage, &storedConfig, true);

  TSDS_SetDeviceConfig output = {};
  Supla::Device::RemoteDeviceConfig remoteConfig;
  ASSERT_TRUE(remoteConfig.fillSetDeviceConfig(&output));
  auto *config =
      reinterpret_cast<TDeviceConfig_InputActivation *>(output.Config);
  EXPECT_EQ(config->AvailableModes, kBoth);
  EXPECT_EQ(config->Mode, kVcc);
}

TEST_F(InputActivationDeviceConfigTests,
       UnchangedIncomingModeIsNotWrittenOrNotified) {
  NiceMock<ConfigMock> storage;
  ConfigChangeObserver observer;
  Supla::Device::InputActivationConfig storedConfig = {.mode = kVcc};
  expectInputBlob(storage, &storedConfig, true);
  Supla::Device::RemoteDeviceConfig::RegisterConfigField(
      SUPLA_DEVICE_CONFIG_FIELD_INPUT_ACTIVATION);
  auto config = incoming(kBoth, kVcc);

  EXPECT_CALL(storage,
              setBlob(StrEq(Supla::ConfigTag::InputActivationCfgTag), _, _))
      .Times(0);
  EXPECT_CALL(storage, saveWithDelay(_)).Times(0);

  Supla::Device::RemoteDeviceConfig remoteConfig;
  remoteConfig.processConfig(&config);

  EXPECT_EQ(observer.notificationCount, 0);
}

TEST_F(InputActivationDeviceConfigTests,
       ChangedIncomingModeNotifiesExactlyOnce) {
  NiceMock<ConfigMock> storage;
  ConfigChangeObserver observer;
  Supla::Device::InputActivationConfig storedConfig = {.mode = kGnd};
  expectInputBlob(storage, &storedConfig, true);
  Supla::Device::RemoteDeviceConfig::RegisterConfigField(
      SUPLA_DEVICE_CONFIG_FIELD_INPUT_ACTIVATION);
  auto config = incoming(kBoth, kVcc);

  EXPECT_CALL(storage,
              setBlob(StrEq(Supla::ConfigTag::InputActivationCfgTag), _, _))
      .WillOnce(Invoke(
          [&storedConfig](const char *, const char *blob, size_t blobSize) {
            std::memcpy(&storedConfig, blob, blobSize);
            return true;
          }));
  EXPECT_CALL(storage, saveWithDelay(1000));

  Supla::Device::RemoteDeviceConfig remoteConfig;
  remoteConfig.processConfig(&config);

  EXPECT_EQ(storedConfig.mode, kVcc);
  EXPECT_EQ(observer.notificationCount, 1);
}

TEST_F(InputActivationDeviceConfigTests,
       IncomingCapabilitiesNeverReplaceLocalPropertiesAndMismatchResends) {
  NiceMock<ConfigMock> storage;
  ConfigChangeObserver observer;
  Supla::Device::InputActivationConfig storedConfig = {.mode = kGnd};
  expectInputBlob(storage, &storedConfig, true);
  Supla::Device::RemoteDeviceConfig::RegisterConfigField(
      SUPLA_DEVICE_CONFIG_FIELD_INPUT_ACTIVATION);
  auto config = incoming(kGnd, kGnd);

  EXPECT_CALL(storage,
              setBlob(StrEq(Supla::ConfigTag::InputActivationCfgTag), _, _))
      .Times(0);
  Supla::Device::RemoteDeviceConfig remoteConfig;
  remoteConfig.processConfig(&config);

  TSDS_SetDeviceConfig output = {};
  ASSERT_TRUE(remoteConfig.fillSetDeviceConfig(&output));
  auto *outgoing =
      reinterpret_cast<TDeviceConfig_InputActivation *>(output.Config);
  EXPECT_EQ(outgoing->AvailableModes, kBoth);
  EXPECT_TRUE(remoteConfig.isSetDeviceConfigRequired());
  EXPECT_EQ(observer.notificationCount, 0);
}

TEST_F(InputActivationDeviceConfigTests,
       UnknownCapabilityBitsAreNotPersistedOrAdvertised) {
  NiceMock<ConfigMock> storage;
  ConfigChangeObserver observer;
  Supla::Device::InputActivationConfig storedConfig = {.mode = kGnd};
  expectInputBlob(storage, &storedConfig, true);
  Supla::Device::RemoteDeviceConfig::RegisterConfigField(
      SUPLA_DEVICE_CONFIG_FIELD_INPUT_ACTIVATION);
  auto config = incoming(kBoth | 0x80, kVcc);

  EXPECT_CALL(storage,
              setBlob(StrEq(Supla::ConfigTag::InputActivationCfgTag), _, _))
      .Times(0);
  Supla::Device::RemoteDeviceConfig remoteConfig;
  remoteConfig.processConfig(&config);

  TSDS_SetDeviceConfig output = {};
  ASSERT_TRUE(remoteConfig.fillSetDeviceConfig(&output));
  auto *outgoing =
      reinterpret_cast<TDeviceConfig_InputActivation *>(output.Config);
  EXPECT_EQ(outgoing->AvailableModes, kBoth);
  EXPECT_TRUE(remoteConfig.isSetDeviceConfigRequired());
  EXPECT_EQ(observer.notificationCount, 0);
}

TEST_F(InputActivationDeviceConfigTests,
       InvalidModesDoNotOverwriteValidStoredMode) {
  for (uint8_t mode : {static_cast<uint8_t>(0),
                       static_cast<uint8_t>(kBoth),
                       static_cast<uint8_t>(0x80),
                       kVcc}) {
    NiceMock<ConfigMock> storage;
    ConfigChangeObserver observer;
    Supla::Device::InputActivationConfig storedConfig = {.mode = kGnd};
    expectInputBlob(storage, &storedConfig, true);
    Supla::Device::RemoteDeviceConfig::RegisterConfigField(
        SUPLA_DEVICE_CONFIG_FIELD_INPUT_ACTIVATION);
    if (mode == kVcc) {
      setProperties(kGnd, kGnd);
    } else {
      setProperties(kBoth, kGnd);
    }
    auto config = incoming(mode == 0x80 ? kBoth : kBoth, mode);

    EXPECT_CALL(storage,
                setBlob(StrEq(Supla::ConfigTag::InputActivationCfgTag), _, _))
        .Times(0);
    Supla::Device::RemoteDeviceConfig remoteConfig;
    remoteConfig.processConfig(&config);

    EXPECT_EQ(storedConfig.mode, kGnd);
    EXPECT_EQ(observer.notificationCount, 0);
  }
}

TEST_F(InputActivationDeviceConfigTests,
       ConfigSizeAndCombinedFieldOffsetAreCorrect) {
  NiceMock<ConfigMock> storage;
  Supla::Device::RemoteDeviceConfig::SetThermalProtectionProperties({
      .minThreshold = 50,
      .maxThreshold = 300,
      .disableAllowed = 1,
  });
  Supla::Device::RemoteDeviceConfig::RegisterConfigField(
      SUPLA_DEVICE_CONFIG_FIELD_THERMAL_PROTECTION);
  Supla::Device::RemoteDeviceConfig::RegisterConfigField(
      SUPLA_DEVICE_CONFIG_FIELD_INPUT_ACTIVATION);
  EXPECT_CALL(storage,
              getBlob(StrEq(Supla::ConfigTag::ThermalProtectionCfgTag),
                      _,
                      sizeof(Supla::Device::ThermalProtectionConfig)))
      .WillOnce(Invoke([](const char *, char *, size_t) { return false; }));
  EXPECT_CALL(storage,
              getBlob(StrEq(Supla::ConfigTag::InputActivationCfgTag), _, _))
      .WillOnce(Invoke([](const char *, char *, size_t) { return false; }));

  TSDS_SetDeviceConfig output = {};
  Supla::Device::RemoteDeviceConfig remoteConfig;
  ASSERT_TRUE(remoteConfig.fillSetDeviceConfig(&output));
  EXPECT_EQ(output.ConfigSize,
            sizeof(TDeviceConfig_ThermalProtection) +
                sizeof(TDeviceConfig_InputActivation));
  auto *input = reinterpret_cast<TDeviceConfig_InputActivation *>(
      output.Config + sizeof(TDeviceConfig_ThermalProtection));
  EXPECT_EQ(input->AvailableModes, kBoth);
  EXPECT_EQ(input->Mode, kGnd);
}

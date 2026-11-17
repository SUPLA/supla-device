// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <SuplaDevice.h>
#include <config_mock.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <supla/device/remote_device_config.h>
#include <supla/modbus/modbus_configurator.h>
#include <supla/storage/config_tags.h>

#include <cstring>

using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StrEq;

namespace {

class TestConfigurator : public Supla::Modbus::Configurator {
 public:
  void loadUnvalidatedConfig(const Supla::Modbus::Config &value) {
    config = value;
  }

  void loadPropertiesForTest(const Supla::Modbus::ConfigProperties &value) {
    properties = value;
  }
};

Supla::Modbus::ConfigProperties allProperties() {
  Supla::Modbus::ConfigProperties properties;
  properties.protocol.master = 1;
  properties.protocol.slave = 1;
  properties.protocol.rtu = 1;
  properties.protocol.ascii = 1;
  properties.protocol.tcp = 1;
  properties.protocol.udp = 1;
  properties.baudrate.b4800 = 1;
  properties.baudrate.b38400 = 1;
  properties.baudrate.b57600 = 1;
  properties.baudrate.b115200 = 1;
  properties.stopBits.one = 1;
  properties.stopBits.oneAndHalf = 1;
  properties.stopBits.two = 1;
  return properties;
}

Supla::Modbus::Config enabledRtuConfig() {
  Supla::Modbus::Config config;
  config.role = Supla::Modbus::Role::Slave;
  config.modbusAddress = 123;
  config.slaveTimeoutMs = 4321;
  config.serial.mode = Supla::Modbus::ModeSerial::Rtu;
  config.serial.baudrate = 57600;
  config.serial.stopBits = Supla::Modbus::SerialStopBits::Two;
  config.network.port = 1502;
  return config;
}

void copyConfigToBlob(const Supla::Modbus::Config &stored,
                      char *blob,
                      size_t blobSize) {
  ASSERT_EQ(blobSize, sizeof(stored));
  std::memcpy(blob, &stored, sizeof(stored));
}

void expectNoConfigWrite(NiceMock<ConfigMock> &storage) {
  EXPECT_CALL(storage,
              setBlob(StrEq(Supla::ConfigTag::ModbusCfgTag), _, _))
      .Times(0);
  EXPECT_CALL(storage, saveWithDelay(_)).Times(0);
  EXPECT_CALL(storage,
              setUInt8(StrEq(Supla::ConfigTag::DeviceConfigChangeCfgTag), _))
      .Times(0);
}

}  // namespace

TEST(ModbusConfiguratorTests, MissingConfigStoresSafeDisabledDefault) {
  NiceMock<ConfigMock> storage;
  TestConfigurator configurator;
  configurator.setProperties(allProperties());

  EXPECT_CALL(storage,
              getBlob(StrEq(Supla::ConfigTag::ModbusCfgTag),
                      _,
                      sizeof(Supla::Modbus::Config)))
      .WillOnce(Return(false));
  EXPECT_CALL(storage,
              setBlob(StrEq(Supla::ConfigTag::ModbusCfgTag),
                      _,
                      sizeof(Supla::Modbus::Config)))
      .WillOnce(Invoke([](const char *, const char *blob, size_t blobSize) {
        EXPECT_EQ(blobSize, sizeof(Supla::Modbus::Config));
        if (blobSize != sizeof(Supla::Modbus::Config)) {
          return false;
        }
        Supla::Modbus::Config stored;
        std::memcpy(&stored, blob, sizeof(stored));
        EXPECT_EQ(stored.role, Supla::Modbus::Role::NotSet);
        EXPECT_EQ(stored.serial.mode, Supla::Modbus::ModeSerial::Disabled);
        EXPECT_EQ(stored.network.mode, Supla::Modbus::ModeNetwork::Disabled);
        return true;
      }));
  EXPECT_CALL(storage, saveWithDelay(5000)).Times(1);
  EXPECT_CALL(storage,
              setUInt8(StrEq(Supla::ConfigTag::DeviceConfigChangeCfgTag), _))
      .Times(0);

  configurator.onLoadConfig(nullptr);

  EXPECT_EQ(configurator.getConfig().role, Supla::Modbus::Role::NotSet);
  EXPECT_EQ(configurator.getConfig().serial.mode,
            Supla::Modbus::ModeSerial::Disabled);
  EXPECT_EQ(configurator.getConfig().network.mode,
            Supla::Modbus::ModeNetwork::Disabled);
  EXPECT_TRUE(configurator.isModbusDisabled());
  EXPECT_FALSE(configurator.isSerialModeEnabled());
  EXPECT_FALSE(configurator.isNetworkModeEnabled());
}

TEST(ModbusConfiguratorTests, ExistingSlaveRtuConfigIsLoadedWithoutRewrite) {
  NiceMock<ConfigMock> storage;
  TestConfigurator configurator;
  configurator.setProperties(allProperties());
  const auto stored = enabledRtuConfig();

  EXPECT_CALL(storage,
              getBlob(StrEq(Supla::ConfigTag::ModbusCfgTag),
                      _,
                      sizeof(Supla::Modbus::Config)))
      .WillOnce(Invoke([&stored](const char *, char *blob, size_t blobSize) {
        copyConfigToBlob(stored, blob, blobSize);
        return true;
      }));
  expectNoConfigWrite(storage);

  configurator.onLoadConfig(nullptr);

  EXPECT_EQ(configurator.getConfig(), stored);
  EXPECT_EQ(configurator.getConfig().slaveTimeoutMs, stored.slaveTimeoutMs);
  EXPECT_FALSE(configurator.isModbusDisabled());
  EXPECT_TRUE(configurator.isSerialModeEnabled());
  EXPECT_FALSE(configurator.isNetworkModeEnabled());
}

TEST(ModbusConfiguratorTests,
     ExistingDisabledConfigSurvivesRestartOrOtaStartup) {
  NiceMock<ConfigMock> storage;
  TestConfigurator configurator;
  configurator.setProperties(allProperties());
  Supla::Modbus::Config stored;
  stored.modbusAddress = 42;
  stored.slaveTimeoutMs = 9876;
  stored.serial.baudrate = 115200;
  stored.serial.stopBits = Supla::Modbus::SerialStopBits::Two;
  stored.network.port = 2502;

  EXPECT_CALL(storage,
              getBlob(StrEq(Supla::ConfigTag::ModbusCfgTag),
                      _,
                      sizeof(Supla::Modbus::Config)))
      .Times(2)
      .WillRepeatedly(
          Invoke([&stored](const char *, char *blob, size_t blobSize) {
            copyConfigToBlob(stored, blob, blobSize);
            return true;
          }));
  expectNoConfigWrite(storage);

  configurator.onLoadConfig(nullptr);
  configurator.onInit();
  configurator.onLoadConfig(nullptr);

  EXPECT_EQ(configurator.getConfig(), stored);
  EXPECT_TRUE(configurator.isModbusDisabled());
  EXPECT_FALSE(configurator.isSerialModeEnabled());
  EXPECT_FALSE(configurator.isNetworkModeEnabled());
}

TEST(ModbusConfiguratorTests, FactoryResetIsFollowedBySafeDefault) {
  NiceMock<ConfigMock> storage;
  EXPECT_CALL(storage, removeAll()).Times(1);
  EXPECT_CALL(storage, commit()).Times(1);

  SuplaDeviceClass device;
  device.resetToFactorySettings();

  TestConfigurator configurator;
  configurator.setProperties(allProperties());
  EXPECT_CALL(storage,
              getBlob(StrEq(Supla::ConfigTag::ModbusCfgTag),
                      _,
                      sizeof(Supla::Modbus::Config)))
      .WillOnce(Return(false));
  EXPECT_CALL(storage,
              setBlob(StrEq(Supla::ConfigTag::ModbusCfgTag),
                      _,
                      sizeof(Supla::Modbus::Config)))
      .WillOnce(Return(true));
  EXPECT_CALL(storage, saveWithDelay(5000)).Times(1);
  EXPECT_CALL(storage,
              setUInt8(StrEq(Supla::ConfigTag::DeviceConfigChangeCfgTag), _))
      .Times(0);

  configurator.onLoadConfig(nullptr);

  EXPECT_TRUE(configurator.isModbusDisabled());
  EXPECT_FALSE(configurator.isSerialModeEnabled());
  EXPECT_FALSE(configurator.isNetworkModeEnabled());
}

TEST(ModbusConfiguratorTests, CloudConfigCanBeAppliedAfterFactoryReset) {
  NiceMock<ConfigMock> storage;
  TestConfigurator configurator;
  configurator.setProperties(allProperties());
  const auto properties = configurator.getProperties();
  auto cloudConfig = enabledRtuConfig();
  cloudConfig.network.mode = Supla::Modbus::ModeNetwork::Disabled;
  cloudConfig.network.port = 502;
  const Supla::Modbus::Config safeDefault;

  EXPECT_CALL(storage,
              getBlob(StrEq(Supla::ConfigTag::ModbusCfgTag),
                      _,
                      sizeof(Supla::Modbus::Config)))
      .WillOnce(Return(false))
      .WillOnce(
          Invoke([&safeDefault](const char *, char *blob, size_t blobSize) {
            copyConfigToBlob(safeDefault, blob, blobSize);
            return true;
          }))
      .WillOnce(
          Invoke([&cloudConfig](const char *, char *blob, size_t blobSize) {
            copyConfigToBlob(cloudConfig, blob, blobSize);
            return true;
          }));
  EXPECT_CALL(storage,
              setBlob(StrEq(Supla::ConfigTag::ModbusCfgTag),
                      _,
                      sizeof(Supla::Modbus::Config)))
      .WillOnce(Return(true))
      .WillOnce(Invoke([&cloudConfig](const char *,
                                     const char *blob,
                                     size_t blobSize) {
        EXPECT_EQ(blobSize, sizeof(cloudConfig));
        if (blobSize != sizeof(cloudConfig)) {
          return false;
        }
        EXPECT_EQ(std::memcmp(blob, &cloudConfig, sizeof(cloudConfig)), 0);
        return true;
      }));
  EXPECT_CALL(storage, saveWithDelay(5000)).Times(1);
  EXPECT_CALL(storage, saveWithDelay(1000)).Times(1);
  EXPECT_CALL(storage,
              setUInt8(StrEq(Supla::ConfigTag::DeviceConfigChangeCfgTag), _))
      .Times(0);

  configurator.onLoadConfig(nullptr);
  configurator.onInit();

  TSDS_SetDeviceConfig packet = {};
  packet.EndOfDataFlag = 1;
  packet.AvailableFields = SUPLA_DEVICE_CONFIG_FIELD_MODBUS;
  packet.Fields = SUPLA_DEVICE_CONFIG_FIELD_MODBUS;
  packet.ConfigSize = sizeof(TDeviceConfig_Modbus);
  auto *modbus =
      reinterpret_cast<TDeviceConfig_Modbus *>(packet.Config);
  modbus->Role = static_cast<uint8_t>(cloudConfig.role);
  modbus->ModbusAddress = cloudConfig.modbusAddress;
  modbus->SlaveTimeoutMs = cloudConfig.slaveTimeoutMs;
  modbus->SerialConfig.Mode = static_cast<uint8_t>(cloudConfig.serial.mode);
  modbus->SerialConfig.Baudrate = cloudConfig.serial.baudrate;
  modbus->SerialConfig.StopBits =
      static_cast<uint8_t>(cloudConfig.serial.stopBits);
  modbus->NetworkConfig.Mode = static_cast<uint8_t>(cloudConfig.network.mode);
  modbus->NetworkConfig.Port = cloudConfig.network.port;
  modbus->Properties.Protocol.Master = properties.protocol.master;
  modbus->Properties.Protocol.Slave = properties.protocol.slave;
  modbus->Properties.Protocol.Rtu = properties.protocol.rtu;
  modbus->Properties.Protocol.Ascii = properties.protocol.ascii;
  modbus->Properties.Protocol.Tcp = properties.protocol.tcp;
  modbus->Properties.Protocol.Udp = properties.protocol.udp;
  modbus->Properties.Baudrate.B4800 = properties.baudrate.b4800;
  modbus->Properties.Baudrate.B9600 = properties.baudrate.b9600;
  modbus->Properties.Baudrate.B19200 = properties.baudrate.b19200;
  modbus->Properties.Baudrate.B38400 = properties.baudrate.b38400;
  modbus->Properties.Baudrate.B57600 = properties.baudrate.b57600;
  modbus->Properties.Baudrate.B115200 = properties.baudrate.b115200;
  modbus->Properties.StopBits.One = properties.stopBits.one;
  modbus->Properties.StopBits.OneAndHalf = properties.stopBits.oneAndHalf;
  modbus->Properties.StopBits.Two = properties.stopBits.two;

  Supla::Device::RemoteDeviceConfig remoteConfig;
  remoteConfig.processConfig(&packet);

  EXPECT_EQ(remoteConfig.getResultCode(), SUPLA_CONFIG_RESULT_TRUE);
  EXPECT_EQ(configurator.getConfig(), cloudConfig);
  EXPECT_FALSE(configurator.isModbusDisabled());
  EXPECT_TRUE(configurator.isSerialModeEnabled());
}

TEST(ModbusConfiguratorTests, UnsupportedStopBitsFallBackToSupportedValue) {
  auto properties = allProperties();
  properties.stopBits.oneAndHalf = 0;
  properties.stopBits.two = 0;
  auto config = enabledRtuConfig();

  EXPECT_TRUE(config.validateAndFix(properties));
  EXPECT_EQ(config.serial.stopBits, Supla::Modbus::SerialStopBits::One);
}

TEST(ModbusConfiguratorTests, InvalidStopBitsFallBackToSupportedValue) {
  auto config = enabledRtuConfig();
  config.serial.stopBits = static_cast<Supla::Modbus::SerialStopBits>(0xFF);

  EXPECT_TRUE(config.validateAndFix(allProperties()));
  EXPECT_TRUE(config.serial.stopBits == Supla::Modbus::SerialStopBits::One ||
              config.serial.stopBits ==
                  Supla::Modbus::SerialStopBits::OneAndHalf ||
              config.serial.stopBits == Supla::Modbus::SerialStopBits::Two);
}

TEST(ModbusConfiguratorTests, InvalidStoredRoleDoesNotEnableModbus) {
  NiceMock<ConfigMock> storage;
  TestConfigurator configurator;
  configurator.setProperties(allProperties());
  auto stored = enabledRtuConfig();
  stored.role = static_cast<Supla::Modbus::Role>(0xFF);
  ON_CALL(storage, getBlob(StrEq(Supla::ConfigTag::ModbusCfgTag), _, _))
      .WillByDefault(Invoke([&stored](const char *, char *blob, size_t size) {
        copyConfigToBlob(stored, blob, size);
        return true;
      }));

  configurator.onLoadConfig(nullptr);

  EXPECT_EQ(configurator.getConfig().role, Supla::Modbus::Role::NotSet);
  EXPECT_TRUE(configurator.isModbusDisabled());
  EXPECT_FALSE(configurator.isSerialModeEnabled());
  EXPECT_FALSE(configurator.isNetworkModeEnabled());
}

TEST(ModbusConfiguratorTests, InvalidStoredSerialModeDoesNotEnableSerial) {
  auto config = enabledRtuConfig();
  config.serial.mode = static_cast<Supla::Modbus::ModeSerial>(0xFF);

  EXPECT_TRUE(config.validateAndFix(allProperties()));
  EXPECT_EQ(config.serial.mode, Supla::Modbus::ModeSerial::Disabled);
}

TEST(ModbusConfiguratorTests, InvalidStoredNetworkModeDoesNotEnableNetwork) {
  auto config = enabledRtuConfig();
  config.network.mode = static_cast<Supla::Modbus::ModeNetwork>(0xFF);

  EXPECT_TRUE(config.validateAndFix(allProperties()));
  EXPECT_EQ(config.network.mode, Supla::Modbus::ModeNetwork::Disabled);
}

TEST(ModbusConfiguratorTests,
     CloudNetworkModeIsDisabledAndPortRemainsInactive) {
  NiceMock<ConfigMock> storage;
  TestConfigurator configurator;
  Supla::Modbus::ConfigProperties properties;
  properties.protocol.slave = 1;
  properties.protocol.tcp = 1;
  properties.stopBits.one = 1;
  configurator.setProperties(properties);
  Supla::Modbus::Config stored;
  stored.role = Supla::Modbus::Role::Slave;
  ON_CALL(storage, getBlob(StrEq(Supla::ConfigTag::ModbusCfgTag), _, _))
      .WillByDefault(Invoke([&stored](const char *, char *blob, size_t size) {
        copyConfigToBlob(stored, blob, size);
        return true;
      }));
  configurator.onLoadConfig(nullptr);

  EXPECT_CALL(storage, setBlob(StrEq(Supla::ConfigTag::ModbusCfgTag), _, _))
      .WillOnce(Invoke([&stored](const char *, const char *blob, size_t size) {
        EXPECT_EQ(size, sizeof(stored));
        if (size != sizeof(stored)) {
          return false;
        }
        std::memcpy(&stored, blob, sizeof(stored));
        return true;
      }));

  TSDS_SetDeviceConfig packet = {};
  packet.EndOfDataFlag = 1;
  packet.AvailableFields = SUPLA_DEVICE_CONFIG_FIELD_MODBUS;
  packet.Fields = SUPLA_DEVICE_CONFIG_FIELD_MODBUS;
  packet.ConfigSize = sizeof(TDeviceConfig_Modbus);
  auto *modbus = reinterpret_cast<TDeviceConfig_Modbus *>(packet.Config);
  modbus->Role = MODBUS_ROLE_SLAVE;
  modbus->ModbusAddress = 1;
  modbus->SerialConfig.Mode = MODBUS_SERIAL_MODE_DISABLED;
  modbus->SerialConfig.Baudrate = 19200;
  modbus->SerialConfig.StopBits = MODBUS_SERIAL_STOP_BITS_ONE;
  modbus->NetworkConfig.Mode = MODBUS_NETWORK_MODE_TCP;
  modbus->NetworkConfig.Port = 1502;
  modbus->Properties.Protocol.Slave = 1;
  modbus->Properties.StopBits.One = 1;

  Supla::Device::RemoteDeviceConfig remoteConfig;
  Supla::Device::RemoteDeviceConfig::ClearResendAttemptsCounter();
  remoteConfig.processConfig(&packet);

  EXPECT_EQ(remoteConfig.getResultCode(), SUPLA_CONFIG_RESULT_TRUE);
  EXPECT_EQ(stored.network.mode, Supla::Modbus::ModeNetwork::Disabled);
  EXPECT_EQ(stored.network.port, 1502);
  EXPECT_EQ(configurator.getConfig().network.mode,
            Supla::Modbus::ModeNetwork::Disabled);
  EXPECT_EQ(configurator.getConfig().network.port, 1502);
  EXPECT_FALSE(configurator.isNetworkModeEnabled());
  EXPECT_TRUE(configurator.isModbusDisabled());
  EXPECT_TRUE(remoteConfig.isSetDeviceConfigRequired());
}

TEST(ModbusConfiguratorTests, NetworkProtocolsAreNotAdvertised) {
  TestConfigurator configurator;
  configurator.setProperties(allProperties());

  EXPECT_FALSE(configurator.getProperties().protocol.tcp);
  EXPECT_FALSE(configurator.getProperties().protocol.udp);
  EXPECT_TRUE(configurator.getProperties().protocol.rtu);
  EXPECT_TRUE(configurator.getProperties().protocol.ascii);
}

TEST(ModbusConfiguratorTests, NetworkModeValidationUsesProtocolProperties) {
  for (auto mode : {Supla::Modbus::ModeNetwork::Tcp,
                    Supla::Modbus::ModeNetwork::Udp}) {
    auto config = enabledRtuConfig();
    config.network.mode = mode;

    EXPECT_FALSE(config.validateAndFix(allProperties()));
    EXPECT_EQ(config.network.mode, mode);

    auto properties = allProperties();
    // The other network protocol must not authorize the requested one.
    properties.protocol.tcp = mode != Supla::Modbus::ModeNetwork::Tcp;
    properties.protocol.udp = mode != Supla::Modbus::ModeNetwork::Udp;
    EXPECT_TRUE(config.validateAndFix(properties));
    EXPECT_EQ(config.network.mode, Supla::Modbus::ModeNetwork::Disabled);
    EXPECT_EQ(config.serial.mode, Supla::Modbus::ModeSerial::Rtu);
    EXPECT_FALSE(config.validateAndFix(properties));
  }
}

TEST(ModbusConfiguratorTests, NetworkRuntimeUsesProtocolProperties) {
  TestConfigurator configurator;
  for (auto mode : {Supla::Modbus::ModeNetwork::Tcp,
                    Supla::Modbus::ModeNetwork::Udp}) {
    auto config = enabledRtuConfig();
    config.serial.mode = Supla::Modbus::ModeSerial::Disabled;
    config.network.mode = mode;
    configurator.loadUnvalidatedConfig(config);

    // Bypass the public setter only to verify the prepared network behavior.
    configurator.loadPropertiesForTest(allProperties());
    EXPECT_TRUE(configurator.isNetworkModeEnabled());
    EXPECT_FALSE(configurator.isModbusDisabled());

    configurator.setProperties(allProperties());
    EXPECT_FALSE(configurator.isNetworkModeEnabled());
    EXPECT_TRUE(configurator.isModbusDisabled());
  }
}

TEST(ModbusConfiguratorTests, NetworkRuntimeFailsafeDoesNotRequireValidation) {
  TestConfigurator configurator;
  for (auto mode : {Supla::Modbus::ModeNetwork::Tcp,
                    Supla::Modbus::ModeNetwork::Udp}) {
    auto config = enabledRtuConfig();
    config.serial.mode = Supla::Modbus::ModeSerial::Disabled;
    config.network.mode = mode;
    configurator.loadUnvalidatedConfig(config);

    EXPECT_FALSE(configurator.isNetworkModeEnabled());
    EXPECT_TRUE(configurator.isModbusDisabled());
  }
}

TEST(ModbusConfiguratorTests, StopBitsFallbackUsesAnAvailableVariant) {
  for (auto supported : {Supla::Modbus::SerialStopBits::OneAndHalf,
                         Supla::Modbus::SerialStopBits::Two}) {
    auto properties = allProperties();
    properties.stopBits.one = 0;
    properties.stopBits.oneAndHalf =
        supported == Supla::Modbus::SerialStopBits::OneAndHalf;
    properties.stopBits.two = supported == Supla::Modbus::SerialStopBits::Two;
    auto config = enabledRtuConfig();
    config.serial.stopBits = Supla::Modbus::SerialStopBits::One;

    EXPECT_TRUE(config.validateAndFix(properties));
    EXPECT_EQ(config.serial.stopBits, supported);
    EXPECT_FALSE(config.validateAndFix(properties));
  }
}

TEST(ModbusConfiguratorTests,
     NoSupportedStopBitsDisablesSerialAndIsIdempotent) {
  auto properties = allProperties();
  properties.stopBits.one = 0;
  properties.stopBits.oneAndHalf = 0;
  properties.stopBits.two = 0;
  auto config = enabledRtuConfig();

  EXPECT_TRUE(config.validateAndFix(properties));
  EXPECT_EQ(config.serial.mode, Supla::Modbus::ModeSerial::Disabled);
  EXPECT_FALSE(config.validateAndFix(properties));
}

TEST(ModbusConfiguratorTests, StoredNetworkModeIsDisabledAndCorrectionIsSaved) {
  for (auto mode : {Supla::Modbus::ModeNetwork::Tcp,
                    Supla::Modbus::ModeNetwork::Udp}) {
    NiceMock<ConfigMock> storage;
    TestConfigurator configurator;
    configurator.setProperties(allProperties());
    auto stored = enabledRtuConfig();
    stored.network.mode = mode;
    ON_CALL(storage, getBlob(StrEq(Supla::ConfigTag::ModbusCfgTag), _, _))
        .WillByDefault(Invoke([&stored](const char *, char *blob, size_t size) {
          copyConfigToBlob(stored, blob, size);
          return true;
        }));
    EXPECT_CALL(storage, setBlob(StrEq(Supla::ConfigTag::ModbusCfgTag), _, _))
        .WillOnce(Invoke([&stored](const char *,
                                  const char *blob,
                                  size_t size) {
          EXPECT_EQ(size, sizeof(stored));
          if (size != sizeof(stored)) {
            return false;
          }
          std::memcpy(&stored, blob, sizeof(stored));
          return true;
        }));
    EXPECT_CALL(storage, saveWithDelay(5000)).Times(1);

    configurator.onLoadConfig(nullptr);
    configurator.onLoadConfig(nullptr);

    EXPECT_EQ(stored.network.mode, Supla::Modbus::ModeNetwork::Disabled);
    EXPECT_EQ(stored.network.port, 1502);
    EXPECT_EQ(stored.serial.mode, Supla::Modbus::ModeSerial::Rtu);
    EXPECT_FALSE(configurator.isNetworkModeEnabled());
    EXPECT_TRUE(configurator.isSerialModeEnabled());
  }
}

TEST(ModbusConfiguratorTests, InvalidCloudPortDoesNotReplaceStoredPort) {
  for (unsigned int port : {0U, 65536U, 0xFFFFFFFFU}) {
    SCOPED_TRACE(port);
    NiceMock<ConfigMock> storage;
    TestConfigurator configurator;
    Supla::Modbus::ConfigProperties properties;
    properties.protocol.slave = 1;
    properties.stopBits.one = 1;
    configurator.setProperties(properties);
    Supla::Modbus::Config stored;
    stored.role = Supla::Modbus::Role::Slave;
    stored.network.port = 2502;
    ON_CALL(storage, getBlob(StrEq(Supla::ConfigTag::ModbusCfgTag), _, _))
        .WillByDefault(Invoke([&stored](const char *, char *blob, size_t size) {
          copyConfigToBlob(stored, blob, size);
          return true;
        }));
    expectNoConfigWrite(storage);
    configurator.onLoadConfig(nullptr);

    TSDS_SetDeviceConfig packet = {};
    packet.EndOfDataFlag = 1;
    packet.AvailableFields = SUPLA_DEVICE_CONFIG_FIELD_MODBUS;
    packet.Fields = SUPLA_DEVICE_CONFIG_FIELD_MODBUS;
    packet.ConfigSize = sizeof(TDeviceConfig_Modbus);
    auto *modbus = reinterpret_cast<TDeviceConfig_Modbus *>(packet.Config);
    modbus->Role = MODBUS_ROLE_SLAVE;
    modbus->ModbusAddress = 1;
    modbus->SerialConfig.Baudrate = 19200;
    modbus->NetworkConfig.Port = port;
    modbus->Properties.Protocol.Slave = 1;
    modbus->Properties.StopBits.One = 1;

    Supla::Device::RemoteDeviceConfig remoteConfig;
    Supla::Device::RemoteDeviceConfig::ClearResendAttemptsCounter();
    remoteConfig.processConfig(&packet);

    EXPECT_TRUE(remoteConfig.isSetDeviceConfigRequired());
    EXPECT_EQ(configurator.getConfig().network.port, 2502);
    EXPECT_FALSE(configurator.isNetworkModeEnabled());
  }
}

// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <array>

#include <SuplaDevice.h>
#include <supla/clock/clock.h>
#include <supla/device/factory_test.h>
#include <supla/device/register_device.h>
#include <supla/device/security_logger.h>
#include <supla/network/web_server.h>
#include <supla/storage/storage.h>

#include <config_mock.h>
#include <simple_time.h>

class SecureConfigMock : public ConfigMock {
 public:
  bool isEncryptionEnabled() override {
    return true;
  }
};

class EnabledSecurityLogger : public Supla::Device::SecurityLogger {
 public:
  bool isEnabled() const override {
    return true;
  }
};

class MockWebServer : public Supla::WebServer {
 public:
  MockWebServer() : Supla::WebServer(nullptr) {}

  void start() override {}
  void stop() override {}

  MOCK_METHOD(bool, verifyEmbeddedHttpsCertificates, (), (override));
};

class FactoryTestDevice : public SuplaDeviceClass {
 public:
  void setDeviceModeForTest(Supla::DeviceMode mode) {
    deviceMode = mode;
  }

  void setStorageInitResultForTest(bool value) {
    storageInitResult = value;
  }
};

class TestingFactoryTest : public Supla::Device::FactoryTest {
 public:
  TestingFactoryTest(SuplaDeviceClass *sdc, uint32_t timeoutS)
      : FactoryTest(sdc, timeoutS) {}

  int16_t getManufacturerId() override {
    return 123;
  }

  bool failed() const {
    return testFailed;
  }

  int reason() const {
    return failReason;
  }
};

class FactoryTestSecurityOptionsTests : public ::testing::Test {
 protected:
  void SetUp() override {
    Supla::RegisterDevice::resetToDefaults();
    Supla::Storage::SetConfigInstance(&config);
    ON_CALL(config, init()).WillByDefault(::testing::Return(false));
    ON_CALL(webServer, verifyEmbeddedHttpsCertificates())
        .WillByDefault(::testing::Return(true));
    rsaKey.fill(1);
    sd.setDeviceModeForTest(Supla::DEVICE_MODE_TEST);
    sd.setStorageInitResultForTest(true);
    sd.setRsaPublicKeyPtr(rsaKey.data());
    sd.setAutomaticFirmwareUpdateSupported(true);
    sd.setSecurityLogger(&securityLogger);
    sd.setSuplaCACert("CERT");
    sd.setSupla3rdPartyCACert("CERT2");
    Supla::RegisterDevice::setManufacturerId(123);
    Supla::RegisterDevice::setProductId(321);
    Supla::RegisterDevice::setName("Device");
    sd.setInitialMode(Supla::InitialMode::StartOffline);
    Supla::Device::FactoryTest::setInsecureOptions(0);
  }

  void TearDown() override {
    Supla::Device::FactoryTest::setInsecureOptions(0);
    Supla::Storage::SetConfigInstance(nullptr);
    Supla::RegisterDevice::resetToDefaults();
  }

  FactoryTestDevice sd;
  ::testing::NiceMock<SecureConfigMock> config;
  EnabledSecurityLogger securityLogger;
  ::testing::NiceMock<MockWebServer> webServer;
  Supla::Clock clock;
  SimpleTime time;
  std::array<uint8_t, 512> rsaKey = {};
};

TEST_F(FactoryTestSecurityOptionsTests, DefaultProfileRejectsStartInCfgMode) {
  sd.setInitialMode(Supla::InitialMode::StartInCfgMode);

  TestingFactoryTest tester(&sd, 0);
  tester.onInit();

  EXPECT_TRUE(tester.failed());
  EXPECT_EQ(tester.reason(), 21);
  EXPECT_EQ(Supla::Device::FactoryTest::getInsecureOptions(), 0u);
}

TEST_F(FactoryTestSecurityOptionsTests, InsecureFlagAllowsStartInCfgMode) {
  sd.setInitialMode(Supla::InitialMode::StartInCfgMode);
  Supla::Device::FactoryTest::setInsecureOptions(
      Supla::Device::FactoryTest::AllowStartInCfgMode);

  TestingFactoryTest tester(&sd, 0);
  tester.onInit();

  EXPECT_FALSE(tester.failed());
  EXPECT_EQ(tester.reason(), 0);
  EXPECT_EQ(Supla::Device::FactoryTest::getInsecureOptions(),
            Supla::Device::FactoryTest::AllowStartInCfgMode);
}

TEST_F(FactoryTestSecurityOptionsTests,
       StartInCfgModeFlagDoesNotBypassOtherSecurityChecks) {
  ON_CALL(webServer, verifyEmbeddedHttpsCertificates())
      .WillByDefault(::testing::Return(false));
  sd.setInitialMode(Supla::InitialMode::StartInCfgMode);
  Supla::Device::FactoryTest::setInsecureOptions(
      Supla::Device::FactoryTest::AllowStartInCfgMode);

  TestingFactoryTest tester(&sd, 0);
  tester.onInit();

  EXPECT_TRUE(tester.failed());
  EXPECT_EQ(tester.reason(), 18);
}

TEST_F(FactoryTestSecurityOptionsTests, InsecureOptionsAreAdditiveFlags) {
  const auto options = Supla::Device::FactoryTest::AllowStartInCfgMode |
                       Supla::Device::FactoryTest::AllowSecurityLogDisabled;

  Supla::Device::FactoryTest::setInsecureOptions(options);

  EXPECT_EQ(Supla::Device::FactoryTest::getInsecureOptions(), options);
}

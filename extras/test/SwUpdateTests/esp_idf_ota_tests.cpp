// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <esp_idf_ota.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <supla/device/last_state_logger.h>
#include <supla/device/register_device.h>

#include <cstring>
#include <string>
#include <vector>

#include "esp_idf_ota_mock.h"

namespace {

using ::testing::IsNull;
using ::testing::StrEq;

class LastStateLoggerMock : public Supla::Device::LastStateLogger {
 public:
  MOCK_METHOD(void, log, (const char *state, int uptimeSec), (override));
};

class ObserverMock : public Supla::Device::SwUpdateObserver {
 public:
  MOCK_METHOD(void, onSwUpdateStarted, (), (override));
  MOCK_METHOD(void,
              onSwUpdateProgress,
              (uint32_t downloadedBytes, uint32_t totalBytes),
              (override));
  MOCK_METHOD(void,
              onSwUpdateFinished,
              (bool success, const char *reason),
              (override));
  MOCK_METHOD(void,
              onSwUpdateResult,
              (Supla::Device::SwUpdateResult result, const char *reason),
              (override));
};

class TestEspIdfOta : public Supla::EspIdfOta {
 public:
  explicit TestEspIdfOta(Supla::SwUpdateMode mode,
                         SuplaDeviceClass *sdc = nullptr)
      : EspIdfOta(sdc, "https://example.test/check", mode) {
  }
};

class EspIdfOtaTests : public ::testing::Test {
 protected:
  void SetUp() override {
    EspIdfOtaMock::reset();
    Supla::RegisterDevice::resetToDefaults();
    Supla::RegisterDevice::setManufacturerId(1);
    Supla::RegisterDevice::setProductId(2);
    Supla::RegisterDevice::setName("OTA test device");
    Supla::RegisterDevice::setSoftVer("1.0.0");
    char guid[SUPLA_GUID_SIZE] = {};
    guid[0] = 1;
    Supla::RegisterDevice::setGUID(guid);
  }

  void run(TestEspIdfOta &update, ObserverMock &observer) {
    update.setObserver(&observer);
    EXPECT_CALL(observer, onSwUpdateStarted());
    update.start();
    update.iterate();
  }
};

TEST_F(EspIdfOtaTests, NoUpdateIsSuccessfulForOnlyCheck) {
  EspIdfOtaMock::setHttpResponse(R"({"status":"ok","latestUpdate":null})");
  SuplaDeviceClass sd;
  auto logger = new LastStateLoggerMock;
  sd.setLastStateLogger(logger);
  sd.enableLastStateLog();
  TestEspIdfOta update(Supla::SwUpdateMode::OnlyCheck, &sd);
  ObserverMock observer;

  EXPECT_CALL(*logger, log).Times(0);
  EXPECT_CALL(observer,
              onSwUpdateResult(Supla::Device::SwUpdateResult::UP_TO_DATE,
                               IsNull()));
  run(update, observer);

  EXPECT_TRUE(update.isAborted());
  EXPECT_FALSE(update.isRetryAllowed());
  EXPECT_EQ(update.getNewVersion(), nullptr);
  EXPECT_TRUE(EspIdfOtaMock::wasCertificateConfigured());
}

TEST_F(EspIdfOtaTests, NoUpdateIsSuccessfulForPeriodicCheck) {
  EspIdfOtaMock::setHttpResponse(R"({"status":"ok"})", {1, 2, 3, 5, 8});
  TestEspIdfOta update(Supla::SwUpdateMode::PeriodicCheckAndUpdate);
  ObserverMock observer;

  EXPECT_CALL(observer,
              onSwUpdateResult(Supla::Device::SwUpdateResult::UP_TO_DATE,
                               IsNull()));
  run(update, observer);

  EXPECT_TRUE(update.isAborted());
  EXPECT_FALSE(update.isRetryAllowed());
}

TEST_F(EspIdfOtaTests, NoUpdateIsSuccessfulForCheckAndUpdate) {
  EspIdfOtaMock::setHttpResponse(R"({"status":"ok"})");
  TestEspIdfOta update(Supla::SwUpdateMode::CheckAndUpdate);
  ObserverMock observer;

  EXPECT_CALL(observer,
              onSwUpdateResult(Supla::Device::SwUpdateResult::UP_TO_DATE,
                               IsNull()));
  run(update, observer);

  EXPECT_TRUE(update.isAborted());
  EXPECT_FALSE(update.isRetryAllowed());
}

TEST_F(EspIdfOtaTests, ParsesFragmentedAvailableUpdateForOnlyCheck) {
  EspIdfOtaMock::setHttpResponse(
      R"({"status":"ok","latestUpdate":{"version":"2.0.0","updateUrl":"https://updates.supla.org/fw.bin","changelogUrl":"https://updates.supla.org/changes"}})",
      {2, 1, 7, 3, 11, 5});
  TestEspIdfOta update(Supla::SwUpdateMode::OnlyCheck);
  ObserverMock observer;

  EXPECT_CALL(observer, onSwUpdateFinished(true, IsNull()));
  run(update, observer);

  EXPECT_TRUE(update.isAborted());
  EXPECT_STREQ(update.getNewVersion(), "2.0.0");
  EXPECT_STREQ(update.getUrl(), "https://iot.updates.supla.org/fw.bin");
  EXPECT_STREQ(update.getChangelogUrl(),
               "https://iot.updates.supla.org/changes");
}

TEST_F(EspIdfOtaTests, DownloadsVerifiesAndActivatesUpdate) {
  EspIdfOtaMock::setHttpResponse(
      R"({"status":"ok","latestUpdate":{"version":"2.0.0","updateUrl":"https://updates.supla.org/fw.bin"}})",
      {7, 11, 13});

  std::string firmware(600, '\0');
  const uint8_t rsaFooter[] = {0xBA,
                               0xBE,
                               0x2B,
                               0xED,
                               0x00,
                               0x01,
                               0x02,
                               0x00,
                               0x10,
                               0x00,
                               0x00,
                               0x00,
                               0x00,
                               0x00,
                               0x00,
                               0x00};
  memcpy(firmware.data() + firmware.size() - sizeof(rsaFooter),
         rsaFooter,
         sizeof(rsaFooter));
  EspIdfOtaMock::addHttpResponse(firmware, {200, 400});
  EspIdfOtaMock::setRsaVerificationResult(true);

  SuplaDeviceClass sd;
  uint8_t rsaPublicKey[512] = {};
  sd.setRsaPublicKeyPtr(rsaPublicKey);
  TestEspIdfOta update(Supla::SwUpdateMode::CheckAndUpdate, &sd);
  ObserverMock observer;

  EXPECT_CALL(observer, onSwUpdateProgress(0, firmware.size()));
  EXPECT_CALL(observer, onSwUpdateProgress(firmware.size(), firmware.size()));
  EXPECT_CALL(observer, onSwUpdateFinished(true, IsNull()));
  run(update, observer);

  EXPECT_TRUE(update.isFinished());
  EXPECT_FALSE(update.isAborted());
  EXPECT_EQ(EspIdfOtaMock::getHttpClientInitCount(), 2);
  EXPECT_EQ(EspIdfOtaMock::getRequestUrl(),
            "https://iot.updates.supla.org/fw.bin");
  EXPECT_EQ(EspIdfOtaMock::getOtaWrittenBytes(), firmware.size());
  EXPECT_TRUE(EspIdfOtaMock::wasOtaBeginCalled());
  EXPECT_TRUE(EspIdfOtaMock::wasOtaEndCalled());
  EXPECT_TRUE(EspIdfOtaMock::wasRsaVerificationCalled());
  EXPECT_TRUE(EspIdfOtaMock::wasBootPartitionSet());
}

TEST_F(EspIdfOtaTests, AddsBetaAndSecurityOnlyToRequest) {
  EspIdfOtaMock::setHttpResponse(R"({"status":"ok"})");
  TestEspIdfOta update(Supla::SwUpdateMode::OnlyCheck);
  ObserverMock observer;
  update.useBeta();
  update.setSecurityOnly();

  EXPECT_CALL(observer,
              onSwUpdateResult(Supla::Device::SwUpdateResult::UP_TO_DATE,
                               IsNull()));
  run(update, observer);

  EXPECT_THAT(EspIdfOtaMock::getRequestBody(),
              testing::HasSubstr("&beta=true"));
  EXPECT_THAT(EspIdfOtaMock::getRequestBody(),
              testing::HasSubstr("&securityOnly=true"));
}

TEST_F(EspIdfOtaTests, AcceptsCompleteResponseAtBufferCapacity) {
  std::string response = R"({"status":"ok"})";
  response.resize(4096, ' ');
  EspIdfOtaMock::setHttpResponse(response, {1000, 1000, 1000, 1096});
  TestEspIdfOta update(Supla::SwUpdateMode::OnlyCheck);
  ObserverMock observer;

  EXPECT_CALL(observer,
              onSwUpdateResult(Supla::Device::SwUpdateResult::UP_TO_DATE,
                               IsNull()));
  run(update, observer);

  EXPECT_TRUE(update.isAborted());
}

TEST_F(EspIdfOtaTests, RejectsResponseLargerThanBuffer) {
  std::string response = R"({"status":"ok"})";
  response.resize(4097, ' ');
  EspIdfOtaMock::setHttpResponse(response, {2048, 2048, 1});
  TestEspIdfOta update(Supla::SwUpdateMode::OnlyCheck);
  ObserverMock observer;

  EXPECT_CALL(observer,
              onSwUpdateFinished(
                  false, StrEq("SW update: check update response too large")));
  run(update, observer);

  EXPECT_TRUE(update.isAborted());
}

TEST_F(EspIdfOtaTests, ReportsMalformedJsonAsFailure) {
  EspIdfOtaMock::setHttpResponse("not json", {2, 3, 3});
  TestEspIdfOta update(Supla::SwUpdateMode::OnlyCheck);
  ObserverMock observer;

  EXPECT_CALL(
      observer,
      onSwUpdateFinished(
          false, StrEq("SW update: failed to parse check update response")));
  run(update, observer);

  EXPECT_TRUE(update.isAborted());
}

}  // namespace

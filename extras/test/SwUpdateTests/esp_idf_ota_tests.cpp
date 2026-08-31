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

constexpr char AVAILABLE_UPDATE_RESPONSE[] =
"{\"status\":\"ok\",\"latestUpdate\":{\"version\":\"2.0.0\","
"\"updateUrl\":\"https://updates.supla.org/fw.bin\"}}";

std::string makeFirmware(size_t size = 600) {
  std::string firmware(size, '\0');
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
  return firmware;
}

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
  EspIdfOtaMock::setHttpResponse(AVAILABLE_UPDATE_RESPONSE, {7, 11, 13});

  std::string firmware = makeFirmware();
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

TEST_F(EspIdfOtaTests, HeaderReadFailureStopsBeforeParsingAndAllowsRetry) {
  EspIdfOtaMock::setHttpResponse(AVAILABLE_UPDATE_RESPONSE);
  EspIdfOtaMock::setHttpHeadersError(0);
  TestEspIdfOta update(Supla::SwUpdateMode::CheckAndUpdate);
  ObserverMock observer;

  EXPECT_CALL(observer,
              onSwUpdateFinished(
                  false,
                  StrEq("SW update: failed to read response headers: "
                        "Error 1")));
  run(update, observer);

  EXPECT_TRUE(update.isAborted());
  EXPECT_TRUE(update.isRetryAllowed());
  EXPECT_EQ(EspIdfOtaMock::getHttpReadCallCount(0), 0u);
  EXPECT_FALSE(EspIdfOtaMock::wasOtaBeginCalled());
}

TEST_F(EspIdfOtaTests, OnlyCheckTransportFailureDoesNotBecomeInstallRetry) {
  EspIdfOtaMock::setHttpResponse(AVAILABLE_UPDATE_RESPONSE);
  EspIdfOtaMock::setHttpHeadersError(0);
  TestEspIdfOta update(Supla::SwUpdateMode::OnlyCheck);
  ObserverMock observer;

  EXPECT_CALL(observer, onSwUpdateFinished(false, testing::_));
  run(update, observer);

  EXPECT_TRUE(update.isAborted());
  EXPECT_FALSE(update.isRetryAllowed());
  EXPECT_FALSE(EspIdfOtaMock::wasOtaBeginCalled());
}

TEST_F(EspIdfOtaTests, UpdateCheckRejectsNonSuccessStatusBeforeBody) {
  EspIdfOtaMock::setHttpResponse(AVAILABLE_UPDATE_RESPONSE);
  EspIdfOtaMock::setHttpStatusCode(0, 503);
  TestEspIdfOta update(Supla::SwUpdateMode::CheckAndUpdate);
  ObserverMock observer;

  EXPECT_CALL(observer,
              onSwUpdateFinished(
                  false,
                  StrEq("SW update: update check failed with status code "
                        "503")));
  run(update, observer);

  EXPECT_TRUE(update.isRetryAllowed());
  EXPECT_EQ(EspIdfOtaMock::getHttpReadCallCount(0), 0u);
  EXPECT_FALSE(EspIdfOtaMock::wasOtaBeginCalled());
}

TEST_F(EspIdfOtaTests, DataReadErrorAbortsPartialSessionAndRetryRestarts) {
  std::string firmware = makeFirmware();
  EspIdfOtaMock::setHttpResponse(AVAILABLE_UPDATE_RESPONSE);
  EspIdfOtaMock::addHttpResponse(firmware);
  EspIdfOtaMock::setHttpReadResults(1, {200, -1});
  EspIdfOtaMock::addHttpResponse(AVAILABLE_UPDATE_RESPONSE);
  EspIdfOtaMock::addHttpResponse(firmware, {300, 300});
  EspIdfOtaMock::setRsaVerificationResult(true);

  SuplaDeviceClass sd;
  uint8_t rsaPublicKey[512] = {};
  sd.setRsaPublicKeyPtr(rsaPublicKey);
  ObserverMock observer;

  {
    TestEspIdfOta update(Supla::SwUpdateMode::CheckAndUpdate, &sd);
    EXPECT_CALL(observer, onSwUpdateProgress(0, firmware.size()));
    EXPECT_CALL(observer,
                onSwUpdateFinished(false,
                                   StrEq("SW update: data read error")));
    run(update, observer);

    EXPECT_TRUE(update.isAborted());
    EXPECT_TRUE(update.isRetryAllowed());
    EXPECT_EQ(EspIdfOtaMock::getOtaWrittenBytes(), 200u);
    EXPECT_EQ(EspIdfOtaMock::getOtaBeginCount(), 1u);
    EXPECT_EQ(EspIdfOtaMock::getOtaAbortCount(), 1u);
    EXPECT_EQ(EspIdfOtaMock::getOtaEndCount(), 0u);
  }

  TestEspIdfOta retry(Supla::SwUpdateMode::CheckAndUpdate, &sd);
  EXPECT_CALL(observer, onSwUpdateProgress(0, firmware.size()));
  EXPECT_CALL(observer, onSwUpdateProgress(firmware.size(), firmware.size()));
  EXPECT_CALL(observer, onSwUpdateFinished(true, IsNull()));
  run(retry, observer);

  EXPECT_FALSE(retry.isAborted());
  EXPECT_EQ(EspIdfOtaMock::getHttpClientInitCount(), 4u);
  EXPECT_EQ(EspIdfOtaMock::getOtaWrittenBytes(), firmware.size());
  EXPECT_EQ(EspIdfOtaMock::getOtaBeginCount(), 2u);
  EXPECT_EQ(EspIdfOtaMock::getOtaAbortCount(), 1u);
  EXPECT_EQ(EspIdfOtaMock::getOtaEndCount(), 1u);
  EXPECT_TRUE(EspIdfOtaMock::wasBootPartitionSet());
}

TEST_F(EspIdfOtaTests, UnexpectedZeroReadFailsWithoutSpinning) {
  std::string firmware = makeFirmware();
  EspIdfOtaMock::setHttpResponse(AVAILABLE_UPDATE_RESPONSE);
  EspIdfOtaMock::addHttpResponse(firmware);
  EspIdfOtaMock::setHttpReadResults(1, {200, 0, -1});
  TestEspIdfOta update(Supla::SwUpdateMode::CheckAndUpdate);
  ObserverMock observer;

  EXPECT_CALL(observer, onSwUpdateProgress(0, firmware.size()));
  EXPECT_CALL(observer,
              onSwUpdateFinished(false, StrEq("SW update: data read error")));
  run(update, observer);

  EXPECT_TRUE(update.isRetryAllowed());
  EXPECT_EQ(EspIdfOtaMock::getHttpReadCallCount(1), 2u);
  EXPECT_EQ(EspIdfOtaMock::getOtaWrittenBytes(), 200u);
  EXPECT_TRUE(EspIdfOtaMock::wasOtaAbortCalled());
  EXPECT_FALSE(EspIdfOtaMock::wasOtaEndCalled());
}

TEST_F(EspIdfOtaTests, EarlyServerCloseDoesNotFinalizePartialImage) {
  std::string partialFirmware = makeFirmware(300);
  EspIdfOtaMock::setHttpResponse(AVAILABLE_UPDATE_RESPONSE);
  EspIdfOtaMock::addHttpResponse(partialFirmware);
  EspIdfOtaMock::setHttpContentLength(1, 600);
  EspIdfOtaMock::setResponseComplete(1, false);
  TestEspIdfOta update(Supla::SwUpdateMode::CheckAndUpdate);
  ObserverMock observer;

  EXPECT_CALL(observer, onSwUpdateProgress(0, 600));
  EXPECT_CALL(observer,
              onSwUpdateFinished(false, StrEq("SW update: data read error")));
  run(update, observer);

  EXPECT_TRUE(update.isRetryAllowed());
  EXPECT_EQ(EspIdfOtaMock::getOtaWrittenBytes(), partialFirmware.size());
  EXPECT_TRUE(EspIdfOtaMock::wasOtaAbortCalled());
  EXPECT_FALSE(EspIdfOtaMock::wasBootPartitionSet());
}

TEST_F(EspIdfOtaTests, CertificateFailureIsTerminal) {
  std::string firmware = makeFirmware();
  EspIdfOtaMock::setHttpResponse(AVAILABLE_UPDATE_RESPONSE);
  EspIdfOtaMock::addHttpResponse(firmware);
  EspIdfOtaMock::setHttpOpenError(1, 0, 1);
  TestEspIdfOta update(Supla::SwUpdateMode::CheckAndUpdate);
  ObserverMock observer;

  EXPECT_CALL(
      observer,
      onSwUpdateFinished(
          false,
          StrEq("SW update: failed to open HTTPS connection: certificate "
                "verification failed, flags=0x1")));
  run(update, observer);

  EXPECT_FALSE(update.isRetryAllowed());
  EXPECT_FALSE(EspIdfOtaMock::wasOtaBeginCalled());
  EXPECT_FALSE(EspIdfOtaMock::wasBootPartitionSet());
}

TEST_F(EspIdfOtaTests, PermanentHttpErrorIsNotRetried) {
  EspIdfOtaMock::setHttpResponse(AVAILABLE_UPDATE_RESPONSE);
  EspIdfOtaMock::addHttpResponse("not found");
  EspIdfOtaMock::setHttpStatusCode(1, 404);
  TestEspIdfOta update(Supla::SwUpdateMode::CheckAndUpdate);
  ObserverMock observer;

  EXPECT_CALL(observer,
              onSwUpdateFinished(
                  false,
                  StrEq("SW update: HTTPS GET failed with status code 404")));
  run(update, observer);

  EXPECT_FALSE(update.isRetryAllowed());
  EXPECT_FALSE(EspIdfOtaMock::wasOtaBeginCalled());
}

TEST_F(EspIdfOtaTests, TransientHttpErrorAllowsRetry) {
  EspIdfOtaMock::setHttpResponse(AVAILABLE_UPDATE_RESPONSE);
  EspIdfOtaMock::addHttpResponse("service unavailable");
  EspIdfOtaMock::setHttpStatusCode(1, 503);
  TestEspIdfOta update(Supla::SwUpdateMode::CheckAndUpdate);
  ObserverMock observer;

  EXPECT_CALL(observer, onSwUpdateFinished(false, testing::_));
  run(update, observer);

  EXPECT_TRUE(update.isRetryAllowed());
  EXPECT_FALSE(EspIdfOtaMock::wasOtaBeginCalled());
}

TEST_F(EspIdfOtaTests, InvalidRsaSignatureIsTerminal) {
  std::string firmware = makeFirmware();
  EspIdfOtaMock::setHttpResponse(AVAILABLE_UPDATE_RESPONSE);
  EspIdfOtaMock::addHttpResponse(firmware);

  SuplaDeviceClass sd;
  uint8_t rsaPublicKey[512] = {};
  sd.setRsaPublicKeyPtr(rsaPublicKey);
  TestEspIdfOta update(Supla::SwUpdateMode::CheckAndUpdate, &sd);
  ObserverMock observer;

  EXPECT_CALL(observer, onSwUpdateProgress(0, firmware.size()));
  EXPECT_CALL(observer,
              onSwUpdateFinished(
                  false,
                  StrEq("SW update: RSA signature verification failed")));
  run(update, observer);

  EXPECT_FALSE(update.isRetryAllowed());
  EXPECT_TRUE(EspIdfOtaMock::wasOtaEndCalled());
  EXPECT_FALSE(EspIdfOtaMock::wasOtaAbortCalled());
  EXPECT_FALSE(EspIdfOtaMock::wasBootPartitionSet());
}

TEST_F(EspIdfOtaTests, ImageValidationFailureIsTerminal) {
  std::string firmware = makeFirmware();
  EspIdfOtaMock::setHttpResponse(AVAILABLE_UPDATE_RESPONSE);
  EspIdfOtaMock::addHttpResponse(firmware);
  EspIdfOtaMock::setOtaEndResult(ESP_ERR_OTA_VALIDATE_FAILED);
  TestEspIdfOta update(Supla::SwUpdateMode::CheckAndUpdate);
  ObserverMock observer;

  EXPECT_CALL(observer, onSwUpdateProgress(0, firmware.size()));
  EXPECT_CALL(
      observer,
      onSwUpdateFinished(
          false,
          StrEq("SW update: image validation failed - image is corrupted")));
  run(update, observer);

  EXPECT_FALSE(update.isRetryAllowed());
  EXPECT_TRUE(EspIdfOtaMock::wasOtaEndCalled());
  EXPECT_FALSE(EspIdfOtaMock::wasOtaAbortCalled());
  EXPECT_FALSE(EspIdfOtaMock::wasRsaVerificationCalled());
  EXPECT_FALSE(EspIdfOtaMock::wasBootPartitionSet());
}

TEST_F(EspIdfOtaTests, MissingContentLengthStillUsesCompleteTransferState) {
  std::string firmware = makeFirmware();
  EspIdfOtaMock::setHttpResponse(AVAILABLE_UPDATE_RESPONSE);
  EspIdfOtaMock::addHttpResponse(firmware, {200, 400});
  EspIdfOtaMock::setHttpContentLength(1, 0);
  EspIdfOtaMock::setRsaVerificationResult(true);

  SuplaDeviceClass sd;
  uint8_t rsaPublicKey[512] = {};
  sd.setRsaPublicKeyPtr(rsaPublicKey);
  TestEspIdfOta update(Supla::SwUpdateMode::CheckAndUpdate, &sd);
  ObserverMock observer;

  EXPECT_CALL(observer, onSwUpdateProgress(0, 0));
  EXPECT_CALL(observer, onSwUpdateProgress(firmware.size(), 0));
  EXPECT_CALL(observer, onSwUpdateFinished(true, IsNull()));
  run(update, observer);

  EXPECT_EQ(EspIdfOtaMock::getOtaWrittenBytes(), firmware.size());
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

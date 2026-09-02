// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <supla/device/sw_update.h>

namespace {
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

class LegacyObserverMock : public Supla::Device::SwUpdateObserver {
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
};

class TestSwUpdate : public Supla::Device::SwUpdate {
 public:
  TestSwUpdate()
      : Supla::Device::SwUpdate(
            nullptr, nullptr, Supla::SwUpdateMode::CheckAndUpdate) {
  }

  void iterate() override {
  }

  void progress(uint32_t downloadedBytes, uint32_t totalBytes) {
    notifyProgress(downloadedBytes, totalBytes);
  }

  void finish(bool success, const char *reason = nullptr) {
    notifyFinished(success, reason);
  }

  void finish(Supla::Device::SwUpdateResult result,
              const char *reason = nullptr) {
    notifyFinished(result, reason);
  }
};
}  // namespace

TEST(SwUpdateObserverTests, ReportsLifecycleToRegisteredObserver) {
  TestSwUpdate update;
  ObserverMock observer;
  update.setObserver(&observer);

  EXPECT_CALL(observer, onSwUpdateStarted());
  update.start();

  EXPECT_CALL(observer, onSwUpdateProgress(65536, 262144));
  update.progress(65536, 262144);

  EXPECT_CALL(observer, onSwUpdateFinished(true, nullptr));
  update.finish(true);
}

TEST(SwUpdateObserverTests, ReportsFailureReason) {
  TestSwUpdate update;
  ObserverMock observer;
  update.setObserver(&observer);

  EXPECT_CALL(observer,
              onSwUpdateFinished(false, testing::StrEq("TLS error")));
  update.finish(false, "TLS error");
}

TEST(SwUpdateObserverTests, ReportsTypedUpdateResult) {
  TestSwUpdate update;
  ObserverMock observer;
  update.setObserver(&observer);

  EXPECT_CALL(observer,
              onSwUpdateResult(Supla::Device::SwUpdateResult::UP_TO_DATE,
                                testing::StrEq("no update")));
  update.finish(Supla::Device::SwUpdateResult::UP_TO_DATE, "no update");
}

TEST(SwUpdateObserverTests, PreservesLegacyObserverCallbackForTypedResult) {
  TestSwUpdate update;
  LegacyObserverMock observer;
  update.setObserver(&observer);

  EXPECT_CALL(observer,
              onSwUpdateFinished(true, testing::StrEq("no update")));
  update.finish(Supla::Device::SwUpdateResult::UP_TO_DATE, "no update");
}

// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <SuplaDevice.h>
#include <config_mock.h>
#include <element_with_storage.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <storage_mock.h>

namespace {
class ShutdownElement : public ElementWithStorage {
 public:
  void onSoftReset() override {
    stateValue = 123456;
    ++resets;
  }
  int resets = 0;
};

class ShutdownConfig : public ConfigMock {
 public:
  ShutdownConfig() { initResult = true; }
};

class FailingCommitConfig : public ShutdownConfig {
 public:
  bool commitWithResult() override { return false; }
};
}  // namespace

TEST(ShutdownTests, SavesStateAfterCallbacksAndCommitsLegacyConfig) {
  SuplaDeviceClass device;
  testing::NiceMock<ShutdownConfig> config;
  testing::NiceMock<StorageMockSimulator> storage;
  ShutdownElement element;
  EXPECT_CALL(config, init()).WillOnce(testing::Return(true));
  ASSERT_TRUE(Supla::Storage::Init());
  Supla::Storage::IsStateStorageValid();
  config.saveWithDelay(5000);
  EXPECT_CALL(config, commit()).WillOnce([&]() {
    EXPECT_EQ(element.resets, 1);
    element.stateValue = 0;
    Supla::Storage::LoadStateStorage();
    EXPECT_EQ(element.stateValue, 123456);
    return true;
  });
  EXPECT_TRUE(device.prepareForShutdown());
  EXPECT_EQ(element.resets, 1);
}

TEST(ShutdownTests, ReportsConfigWriteFailure) {
  SuplaDeviceClass device;
  testing::NiceMock<FailingCommitConfig> config;
  EXPECT_CALL(config, init()).WillOnce(testing::Return(true));
  EXPECT_FALSE(device.prepareForShutdown());
}

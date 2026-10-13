// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>
#include <nvs_config.h>

esp_err_t fakeNvsCommitResult = ESP_OK;
nvs_handle_t fakeNvsCommitHandle = 0;
int fakeNvsCommitCalls = 0;
esp_err_t fakeNvsEraseResult = ESP_OK;
nvs_handle_t fakeNvsEraseHandle = 0;
int fakeNvsEraseCalls = 0;

esp_err_t fakeNvsPartitionEraseResult = ESP_OK;
const char *fakeNvsErasedPartition = nullptr;
int fakeNvsPartitionEraseCalls = 0;

namespace {
class TestNvsConfig : public Supla::NvsConfig {
 public:
  TestNvsConfig() {
    nvsHandle = 42;
    nvsPartitionName = "suplanvs";
  }
  bool init() override {
    ++initCalls;
    if (switchToDefaultOnInit) nvsPartitionName = "nvs";
    return initSucceeds;
  }
  void useDefaultPartition() { nvsPartitionName = "nvs"; }
  bool switchToDefaultOnInit = false;
  bool initSucceeds = true;
  int initCalls = 0;
};
}  // namespace

TEST(NvsConfigCommitTests, ReportsConfirmedCommitSuccess) {
  fakeNvsCommitResult = ESP_OK;
  fakeNvsCommitCalls = 0;
  TestNvsConfig config;
  EXPECT_TRUE(config.commit());
  EXPECT_EQ(fakeNvsCommitHandle, 42u);
  EXPECT_EQ(fakeNvsCommitCalls, 1);
}
TEST(NvsConfigCommitTests, PropagatesBackendCommitFailure) {
  fakeNvsCommitResult = ESP_ERR_NOT_FOUND;
  fakeNvsCommitCalls = 0;
  TestNvsConfig config;
  EXPECT_FALSE(config.commit());
  EXPECT_EQ(fakeNvsCommitHandle, 42u);
  EXPECT_EQ(fakeNvsCommitCalls, 1);
}
class NvsConfigResetTests : public testing::Test {
 protected:
  void SetUp() override {
    fakeNvsPartitionEraseResult = ESP_OK;
    fakeNvsPartitionEraseCalls = 0;
    fakeNvsErasedPartition = nullptr;
    fakeNvsEraseCalls = fakeNvsCommitCalls = 0;
  }
};

TEST_F(NvsConfigResetTests, ErasesWholePartitionThenReinitializes) {
  TestNvsConfig config;
  EXPECT_TRUE(config.removeAllChecked());
  EXPECT_STREQ(fakeNvsErasedPartition, "suplanvs");
  EXPECT_EQ(fakeNvsPartitionEraseCalls, 1);
  EXPECT_EQ(config.initCalls, 1);
  EXPECT_EQ(fakeNvsEraseCalls, 0);
  EXPECT_EQ(fakeNvsCommitCalls, 0);
}
TEST_F(NvsConfigResetTests, ErasesSelectedDefaultPartition) {
  TestNvsConfig config;
  config.useDefaultPartition();
  EXPECT_TRUE(config.removeAllChecked());
  EXPECT_STREQ(fakeNvsErasedPartition, "nvs");
  EXPECT_EQ(config.initCalls, 1);
}
TEST_F(NvsConfigResetTests, EraseFailureCannotReportSuccess) {
  fakeNvsPartitionEraseResult = ESP_ERR_NOT_FOUND;
  TestNvsConfig config;
  EXPECT_FALSE(config.removeAllChecked());
  EXPECT_EQ(fakeNvsPartitionEraseCalls, 1);
  EXPECT_EQ(config.initCalls, 0);
}
TEST_F(NvsConfigResetTests, ReinitializationFailureCannotReportSuccess) {
  TestNvsConfig config;
  config.initSucceeds = false;
  EXPECT_FALSE(config.removeAllChecked());
  EXPECT_EQ(fakeNvsPartitionEraseCalls, 1);
  EXPECT_EQ(config.initCalls, 1);
}
TEST_F(NvsConfigResetTests, RealInitFailureCannotReportSuccess) {
  Supla::NvsConfig config;
  ASSERT_FALSE(config.init());
  EXPECT_FALSE(config.removeAllChecked());
  EXPECT_STREQ(fakeNvsErasedPartition, "nvs");
  EXPECT_EQ(fakeNvsPartitionEraseCalls, 1);
}

TEST_F(NvsConfigResetTests, FailedInitCannotRedirectEraseRetry) {
  TestNvsConfig config;
  config.switchToDefaultOnInit = true;
  config.initSucceeds = false;
  EXPECT_FALSE(config.removeAllChecked());
  EXPECT_STREQ(fakeNvsErasedPartition, "suplanvs");
  config.switchToDefaultOnInit = false;
  config.initSucceeds = true;
  EXPECT_TRUE(config.removeAllChecked());
  EXPECT_STREQ(fakeNvsErasedPartition, "suplanvs");
  EXPECT_EQ(fakeNvsPartitionEraseCalls, 2);
}
TEST_F(NvsConfigResetTests, SuccessfulFallbackCannotConfirmOrRedirectReset) {
  TestNvsConfig config;
  config.switchToDefaultOnInit = true;
  EXPECT_FALSE(config.removeAllChecked());
  EXPECT_STREQ(fakeNvsErasedPartition, "suplanvs");
  EXPECT_FALSE(config.removeAllChecked());
  EXPECT_STREQ(fakeNvsErasedPartition, "suplanvs");
  EXPECT_EQ(fakeNvsPartitionEraseCalls, 2);
}

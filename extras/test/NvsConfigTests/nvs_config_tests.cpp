// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>
#include <config_key_collector.h>
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

class NvsConfigIterationTests : public testing::Test {
 protected:
  void SetUp() override {
    fakeNvsEntries.clear();
    fakeNvsFindError = fakeNvsInfoError = fakeNvsNextError = ESP_OK;
    fakeNvsIteratorActive = false;
    fakeNvsIteratorReleases = 0;
  }
  void TearDown() override {
    EXPECT_FALSE(fakeNvsIteratorActive);
  }
};

TEST_F(NvsConfigIterationTests,
       FiltersNamespaceSubstringAndReportsNativeTypes) {
  fakeNvsEntries = {{"other", "hvac_ignore", NVS_TYPE_BLOB},
      {"supla", "1_hvac_cfg2", NVS_TYPE_BLOB},
      {"supla", "sl-text", NVS_TYPE_STR},
      {"supla", "i8", NVS_TYPE_I8}, {"supla", "u8", NVS_TYPE_U8},
      {"supla", "i32", NVS_TYPE_I32}, {"supla", "u32", NVS_TYPE_U32},
      {"supla", "native-u64", NVS_TYPE_U64}};
  TestNvsConfig config;
  ConfigKeyCollector all;
  using Type = Supla::ConfigValueType;
  using Result = Supla::ConfigIterationResult;
  EXPECT_EQ(config.forEachKey("", all.collect, &all), Result::COMPLETED);
  EXPECT_EQ(all.visits, 7);
  EXPECT_EQ(fakeNvsEnumeratedPartition, "suplanvs");
  EXPECT_EQ(fakeNvsEnumeratedNamespace, "supla");
  EXPECT_EQ(all.keys.at("1_hvac_cfg2"), Type::BLOB);
  EXPECT_EQ(all.keys.at("sl-text"), Type::STRING);
  EXPECT_EQ(all.keys.at("i8"), Type::INT8);
  EXPECT_EQ(all.keys.at("u8"), Type::UINT8);
  EXPECT_EQ(all.keys.at("i32"), Type::INT32);
  EXPECT_EQ(all.keys.at("u32"), Type::UINT32);
  EXPECT_EQ(all.keys.at("native-u64"), Type::UNKNOWN);
  ConfigKeyCollector match;
  EXPECT_EQ(config.forEachKey("hvac_", match.collect, &match),
              Result::COMPLETED);
  EXPECT_EQ(match.visits, 1);
  ConfigKeyCollector absent;
  EXPECT_EQ(config.forEachKey("HVAC", absent.collect, &absent),
              Result::COMPLETED);
  EXPECT_EQ(absent.visits, 0);
  config.useDefaultPartition();
  EXPECT_EQ(config.forEachKey("no-match", absent.collect, &absent),
            Result::COMPLETED);
  EXPECT_EQ(fakeNvsEnumeratedPartition, "nvs");
}

TEST_F(NvsConfigIterationTests, EmptyStopAndBackendErrorsAreDistinct) {
  TestNvsConfig config;
  ConfigKeyCollector found;
  using Result = Supla::ConfigIterationResult;
  EXPECT_EQ(config.forEachKey("", found.collect, &found), Result::COMPLETED);
  EXPECT_EQ(found.visits, 0);
  EXPECT_EQ(config.forEachKey(nullptr, found.collect, &found), Result::ERROR);
  EXPECT_EQ(config.forEachKey("", nullptr, &found), Result::ERROR);
  fakeNvsEntries = {{"supla", "first", NVS_TYPE_BLOB},
                    {"supla", "second", NVS_TYPE_BLOB}};
  found.stopAfter = 1;
  EXPECT_EQ(config.forEachKey("", found.collect, &found), Result::STOPPED);
  EXPECT_EQ(found.visits, 1);
  EXPECT_FALSE(fakeNvsIteratorActive);
  found = {};
  fakeNvsFindError = ESP_FAIL;
  EXPECT_EQ(config.forEachKey("", found.collect, &found), Result::ERROR);
  EXPECT_EQ(found.visits, 0);
  fakeNvsFindError = ESP_OK;
  fakeNvsInfoError = ESP_FAIL;
  EXPECT_EQ(config.forEachKey("", found.collect, &found), Result::ERROR);
  EXPECT_EQ(found.visits, 0);
  fakeNvsInfoError = ESP_OK;
  fakeNvsNextError = ESP_FAIL;
  EXPECT_EQ(config.forEachKey("", found.collect, &found), Result::ERROR);
  EXPECT_EQ(found.visits, 1);  // partial visits do not imply completion
  EXPECT_FALSE(fakeNvsIteratorActive);
}

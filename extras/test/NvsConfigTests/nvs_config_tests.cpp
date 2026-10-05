// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>
#include <nvs_config.h>

esp_err_t fakeNvsCommitResult = ESP_OK;
nvs_handle_t fakeNvsCommitHandle = 0;
int fakeNvsCommitCalls = 0;

namespace {
class TestNvsConfig : public Supla::NvsConfig {
 public:
  TestNvsConfig() { nvsHandle = 42; }
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

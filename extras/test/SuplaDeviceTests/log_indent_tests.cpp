// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>
#include <supla/element_with_channel_actions.h>
#include <supla/log_wrapper.h>

#include <thread>
#include <string>

extern "C" const char *supla_test_get_last_log();

namespace {
class LogIndentTests : public ::testing::Test {
 protected:
  void SetUp() override {
    previousLogLevel = supla_log_get_level();
    supla_log_set_level(LOG_VERBOSE);
  }
  void TearDown() override { supla_log_set_level(previousLogLevel); }
  int previousLogLevel = LOG_VERBOSE;
};
}  // namespace

TEST_F(LogIndentTests, NestedScopesIndentMessagesAndRestoreOnReturn) {
  const auto nested = []() {
    Supla::LogIndent indent;
    SUPLA_LOG_INFO("nested %d", 42);
    return;
  };
  EXPECT_EQ(Supla::LogIndent::spaces(), 0);
  {
    Supla::LogIndent indent;
    nested();
    EXPECT_STREQ(supla_test_get_last_log(), "    nested 42");
    SUPLA_LOG_WARNING("outer");
    EXPECT_STREQ(supla_test_get_last_log(), "  outer");
  }
  SUPLA_LOG_INFO("outside");
  EXPECT_STREQ(supla_test_get_last_log(), "outside");
}

TEST_F(LogIndentTests, SaturationRestoresEveryScope) {
  const auto nested = [](const auto &self, int remaining) -> void {
    if (!remaining) {
      EXPECT_EQ(Supla::LogIndent::spaces(), 16);
      return;
    }
    const int before = Supla::LogIndent::spaces();
    {
      Supla::LogIndent indent;
      self(self, remaining - 1);
    }
    EXPECT_EQ(Supla::LogIndent::spaces(), before);
  };
  nested(nested, 12);
  EXPECT_EQ(Supla::LogIndent::spaces(), 0);
}

TEST_F(LogIndentTests, ThreadsHaveIndependentDepth) {
  Supla::LogIndent indent;
  std::thread worker([]() {
    EXPECT_EQ(Supla::LogIndent::spaces(), 0);
    Supla::LogIndent indent;
    EXPECT_EQ(Supla::LogIndent::spaces(), 2);
  });
  worker.join();
  EXPECT_EQ(Supla::LogIndent::spaces(), 2);
}

TEST_F(LogIndentTests, ScopeAlsoIndentsLogsFromLibraryCalls) {
  Supla::ConfigTypesBitmap bitmap;
  Supla::LogIndent indent;
  bitmap.setAll(0, 7);
  EXPECT_STREQ(supla_test_get_last_log(),
               "  Channel[7] ConfigTypesBitmap set all to 0x0");
}

TEST_F(LogIndentTests, LongMessageRetainsIndentAndArgumentsAfterResize) {
  Supla::LogIndent indent;
  const std::string payload(1500, 'x');
  SUPLA_LOG_INFO("%s/%d", payload.c_str(), 42);
  EXPECT_EQ(std::string(supla_test_get_last_log()), "  " + payload + "/42");
}

TEST_F(LogIndentTests, RuntimeFilteringDoesNotEvaluateArguments) {
  SUPLA_LOG_INFO("previous");
  supla_log_set_level(LOG_ERR);
  Supla::LogIndent indent;
  int evaluations = 0;
  SUPLA_LOG_DEBUG("disabled %d", ++evaluations);
  EXPECT_EQ(evaluations, 0);
  EXPECT_STREQ(supla_test_get_last_log(), "previous");
}

TEST_F(LogIndentTests, NullFormatPreservesPreviousMessage) {
  SUPLA_LOG_INFO("previous");
  supla_device_logf(LOG_INFO, nullptr);
  EXPECT_STREQ(supla_test_get_last_log(), "previous");
}

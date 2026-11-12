// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>
#include <supla/log_indent.h>

#include <thread>

namespace {

// Exercise the pre-scheduler branch during actual C++ global initialization.
const bool globalConstructorPassed = []() {
  Supla::LogIndent outer;
  if (Supla::LogIndent::spaces() != 2) {
    return false;
  }
  {
    Supla::LogIndent inner;
    if (Supla::LogIndent::spaces() != 4) {
      return false;
    }
  }
  return Supla::LogIndent::spaces() == 2;
}();

class LogIndentEsp32Tests : public ::testing::Test {
 protected:
  void SetUp() override {
    logIndentTestSchedulerState = taskSCHEDULER_NOT_STARTED;
  }
  void TearDown() override {
    logIndentTestSchedulerState = taskSCHEDULER_NOT_STARTED;
  }
};

}  // namespace

TEST_F(LogIndentEsp32Tests, GlobalConstructorRestoresStartupDepth) {
  EXPECT_TRUE(globalConstructorPassed);
  EXPECT_EQ(Supla::LogIndent::spaces(), 0);
}

TEST_F(LogIndentEsp32Tests, StartupStorageIsSeparateFromTaskStorage) {
  {
    Supla::LogIndent startupScope;
    EXPECT_EQ(Supla::LogIndent::spaces(), 2);

    logIndentTestSchedulerState = taskSCHEDULER_RUNNING;
    EXPECT_EQ(Supla::LogIndent::spaces(), 0);
    {
      Supla::LogIndent taskScope;
      EXPECT_EQ(Supla::LogIndent::spaces(), 2);
      logIndentTestSchedulerState = taskSCHEDULER_SUSPENDED;
      EXPECT_EQ(Supla::LogIndent::spaces(), 2);
      logIndentTestSchedulerState = taskSCHEDULER_RUNNING;
    }
    EXPECT_EQ(Supla::LogIndent::spaces(), 0);

    logIndentTestSchedulerState = taskSCHEDULER_NOT_STARTED;
    EXPECT_EQ(Supla::LogIndent::spaces(), 2);
  }
  EXPECT_EQ(Supla::LogIndent::spaces(), 0);
}

TEST_F(LogIndentEsp32Tests, RunningTasksHaveIndependentDepth) {
  logIndentTestSchedulerState = taskSCHEDULER_RUNNING;
  {
    Supla::LogIndent mainScope;
    std::thread worker([]() {
      EXPECT_EQ(Supla::LogIndent::spaces(), 0);
      Supla::LogIndent workerScope;
      EXPECT_EQ(Supla::LogIndent::spaces(), 2);
    });
    worker.join();
    EXPECT_EQ(Supla::LogIndent::spaces(), 2);
  }
  EXPECT_EQ(Supla::LogIndent::spaces(), 0);
}

// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <string.h>

#include <gtest/gtest.h>

#include <string>

#include <supla/device/security_logger.h>

extern "C" const char *supla_test_get_last_log();
extern "C" void supla_test_clear_last_log();

namespace {

class CapturingSecurityLogger : public Supla::Device::SecurityLogger {
 public:
  void storeLog(const Supla::SecurityLogEntry &entry) override {
    lastEntry = entry;
    hasEntry = true;
  }

  Supla::SecurityLogEntry lastEntry = {};
  bool hasEntry = false;
};

void expectStoredLog(CapturingSecurityLogger &logger,
                     const std::string &source,
                     size_t expectedLength) {
  ASSERT_TRUE(logger.hasEntry);
  ASSERT_EQ('\0', logger.lastEntry.log[sizeof(logger.lastEntry.log) - 1]);
  ASSERT_GE(source.size(), expectedLength);
  EXPECT_EQ(expectedLength,
            strnlen(logger.lastEntry.log, sizeof(logger.lastEntry.log)));
  EXPECT_EQ(0, memcmp(logger.lastEntry.log, source.data(), expectedLength));
}

}  // namespace

TEST(SecurityLoggerTests, PreservesFiftyCharacterMessage) {
  CapturingSecurityLogger logger;
  const std::string message(50, 'a');

  logger.log(1, message.c_str());

  expectStoredLog(logger, message, 50);
}

TEST(SecurityLoggerTests, TerminatesMessageAtUsableCapacity) {
  CapturingSecurityLogger logger;
  const std::string message(51, 'b');

  logger.log(1, message.c_str());

  expectStoredLog(logger, message, 51);
}

TEST(SecurityLoggerTests, TruncatesFiftyTwoCharacterMessage) {
  CapturingSecurityLogger logger;
  const std::string message(52, 'c');

  logger.log(1, message.c_str());

  expectStoredLog(logger, message, sizeof(logger.lastEntry.log) - 1);
}

TEST(SecurityLoggerTests, TruncatesWeakPasswordMessage) {
  CapturingSecurityLogger logger;
  const std::string message =
      "Password change failed: password is not strong enough";
  ASSERT_EQ(53U, message.size());

  logger.log(1, message.c_str());

  expectStoredLog(logger, message, sizeof(logger.lastEntry.log) - 1);
}

TEST(SecurityLoggerTests, PrintBoundsUnterminatedStoredEntry) {
  Supla::SecurityLogEntry entry = {};
  memset(entry.log, 'x', sizeof(entry.log));
  supla_test_clear_last_log();

  entry.print();

  const std::string expected = "SSLOG: 0.[0][Unknown] " +
                               std::string(sizeof(entry.log), 'x');
  EXPECT_EQ(expected, supla_test_get_last_log());
}

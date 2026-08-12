// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include <cstring>

#include <supla/network/web_server.h>

TEST(WebServerRedactionTests, MasksSecretFieldsWithLength) {
  char redacted[Supla::REDACTED_LOG_VALUE_BUFFER_SIZE] = {};

  EXPECT_STREQ("<redacted len=8>",
               Supla::redactLogValue("cfg_pwd",
                                     "password",
                                     redacted,
                                     sizeof(redacted)));
  EXPECT_STREQ("<redacted len=11>",
               Supla::redactLogValue("wpw",
                                     "password123",
                                     redacted,
                                     sizeof(redacted)));
  EXPECT_STREQ("<redacted len=12>",
               Supla::redactLogValue("mqttpasswd",
                                     "secret-value",
                                     redacted,
                                     sizeof(redacted)));
  EXPECT_STREQ("<redacted len=25>",
               Supla::redactLogValue("eml",
                                     "kon.trojanski42@mail.xyzv",
                                     redacted,
                                     sizeof(redacted)));
}

TEST(WebServerRedactionTests, LeavesNonSensitiveFieldsUntouched) {
  char redacted[Supla::REDACTED_LOG_VALUE_BUFFER_SIZE] = {};

  EXPECT_STREQ("beta-cloud.supla.org",
               Supla::redactLogValue("svr",
                                     "beta-cloud.supla.org",
                                     redacted,
                                     sizeof(redacted)));
  EXPECT_STREQ("truskawka_IoT",
               Supla::redactLogValue("sid",
                                     "truskawka_IoT",
                                     redacted,
                                     sizeof(redacted)));
}

TEST(WebServerRedactionTests, DetectsSensitiveFieldNames) {
  EXPECT_TRUE(Supla::isSensitiveLogField("cfg_pwd"));
  EXPECT_TRUE(Supla::isSensitiveLogField("custom_ca"));
  EXPECT_TRUE(Supla::isSensitiveLogField("mqtt_ca"));
  EXPECT_TRUE(Supla::isSensitiveLogField("apiToken"));
  EXPECT_TRUE(Supla::isSensitiveLogField("wpw"));
  EXPECT_TRUE(Supla::isSensitiveLogField("mqttuser"));
  EXPECT_TRUE(Supla::isSensitiveLogField("mqttpasswd"));

  EXPECT_FALSE(Supla::isSensitiveLogField("svr"));
  EXPECT_FALSE(Supla::isSensitiveLogField("sid"));
}

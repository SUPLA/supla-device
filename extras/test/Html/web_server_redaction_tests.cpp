// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include <supla/crypto.h>
#include <supla/network/web_server.h>

#include <cstring>
#include <string>

namespace {
class CsrfTestWebServer : public Supla::WebServer {
 public:
  CsrfTestWebServer() : WebServer(nullptr) {
    memcpy(csrfToken, "0123456789abcdef0123456789abcdef", sizeof(csrfToken));
  }
  void start() override {}
  void stop() override {}
};
}  // namespace

TEST(CryptoComparisonTests, ComparesEveryByteIncludingEmbeddedZeros) {
  unsigned char left[64] = {};
  unsigned char right[64] = {};
  EXPECT_TRUE(Supla::Crypto::constantTimeEqual(left, right, sizeof(left)));
  for (size_t i = 0; i < sizeof(right); i++) {
    right[i] = 1;
    EXPECT_FALSE(Supla::Crypto::constantTimeEqual(left, right, sizeof(left)));
    right[i] = 0;
  }
  EXPECT_FALSE(Supla::Crypto::constantTimeEqual(nullptr, right, sizeof(right)));
  EXPECT_FALSE(Supla::Crypto::constantTimeEqual(left, nullptr, sizeof(left)));
  EXPECT_TRUE(Supla::Crypto::constantTimeEqual(left, right, 0));
}

TEST(WebServerCsrfTests, RequiresExactCompleteToken) {
  CsrfTestWebServer server;
  const std::string token = server.getCsrfToken();
  ASSERT_EQ(token.size(), 32);
  EXPECT_TRUE(server.isCsrfTokenValid(token.c_str()));
  EXPECT_FALSE(server.isCsrfTokenValid(nullptr));
  EXPECT_FALSE(server.isCsrfTokenValid(""));
  EXPECT_FALSE(server.isCsrfTokenValid(token.substr(0, 31).c_str()));
  EXPECT_FALSE(server.isCsrfTokenValid((token + "0").c_str()));
  for (size_t i = 0; i < token.size(); i++) {
    auto different = token;
    different[i] = token[i] == '0' ? '1' : '0';
    EXPECT_FALSE(server.isCsrfTokenValid(different.c_str()));
  }
  auto differentCase = token;
  differentCase[10] = 'A';
  EXPECT_FALSE(server.isCsrfTokenValid(differentCase.c_str()));
  char unterminated[33];
  memset(unterminated, '0', sizeof(unterminated));
  EXPECT_FALSE(server.isCsrfTokenValid(unterminated));
}

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

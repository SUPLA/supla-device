// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>
#include <supla/crypto.h>
#include <supla/network/async_dns.h>
#include <supla/network/esp_async_client.h>
#include <supla/network/esp32_tcp_abort.h>
#include <mbedtls/ssl.h>
#include <lwip/sockets.h>

#include <algorithm>
#include <cerrno>
#include <string>
#include <vector>

int fakeHandshakeResult = 0;
bool fakeParseFailure = false;
bool fakeReadFailure = false;
bool fakeWriteFailure = false;
namespace {
std::vector<std::string> events;
int nextFd = 42;
bool failConnect = false;
uint32_t now = 0;
using Result = Supla::Client::ConnectResult;
}  // namespace

void recordTlsFree() { events.push_back("tls-free"); }
uint32_t millis() { return now; }
void delay(uint64_t ms) { now += ms; }
bool Supla::Crypto::fillRandom(uint8_t *, size_t) { return true; }
bool Supla::AsyncDns::start(const char *) { return true; }
int Supla::AsyncDns::poll(uint32_t *address) { *address = 1; return 1; }
void Supla::AsyncDns::cancel() {}
void Supla::abortTcpConnection(int fd) {
  events.push_back("abort:" + std::to_string(fd));
}
extern "C" {
int __wrap_socket(int, int, int) { return nextFd; }
int __wrap_fcntl(int, int, ...) { return 0; }
int __wrap_connect(int, const sockaddr *, socklen_t) {
  errno = ECONNREFUSED;
  return failConnect ? -1 : 0;
}
int __wrap_select(int, fd_set *, fd_set *, fd_set *, timeval *) { return 1; }
int __wrap_getsockopt(int, int, int, void *value, socklen_t *) {
  *static_cast<int *>(value) = 0;
  return 0;
}
int __wrap_getsockname(int, sockaddr *, socklen_t *) { return -1; }
ssize_t __wrap_recv(int, void *, size_t, int flags) {
  if (fakeReadFailure && !(flags & MSG_PEEK)) return 0;
  errno = EAGAIN;
  return -1;
}
ssize_t __wrap_send(int, const void *, size_t size, int) {
  return fakeWriteFailure ? 0 : static_cast<ssize_t>(size);
}
int __wrap_close(int fd) {
  events.push_back("close:" + std::to_string(fd));
  return 0;
}
}  // extern "C"

class ArduinoEsp32TcpAbortTests : public ::testing::TestWithParam<bool> {
 protected:
  void SetUp() override {
    events.clear();
    nextFd = 42;
    now = 0;
    failConnect = fakeParseFailure = fakeReadFailure = fakeWriteFailure = false;
    fakeHandshakeResult = 0;
  }
  void connect(Supla::EspAsyncClient *client) {
    client->setSSLEnabled(GetParam());
    Result result = Result::Pending;
    for (int i = 0; i < 8 && result == Result::Pending; ++i)
      result = client->connectStep("server", 2016);
    ASSERT_EQ(result, Result::Connected);
    events.clear();
  }
  int abortCalls() {
    return std::count_if(events.begin(), events.end(),
                         [](const std::string &s) {
      return s.find("abort:") == 0;
    });
  }
  void expectCleanup() {
    if (GetParam()) {
      EXPECT_EQ(events, (std::vector<std::string>{
          "abort:42", "tls-free", "close:42"}));
    } else {
      EXPECT_EQ(events, (std::vector<std::string>{"abort:42", "close:42"}));
    }
  }
};

TEST_P(ArduinoEsp32TcpAbortTests, RepeatedStopAbortsOnceBeforeCleanup) {
  Supla::EspAsyncClient client;
  connect(&client);
  client.stop();
  client.stop();
  expectCleanup();
}
TEST_P(ArduinoEsp32TcpAbortTests, DestructorAbortsBeforeTlsAndSocketCleanup) {
  {
    Supla::EspAsyncClient client;
    connect(&client);
  }
  expectCleanup();
}
TEST_P(ArduinoEsp32TcpAbortTests, PendingTcpIsAborted) {
  Supla::EspAsyncClient client;
  client.setSSLEnabled(GetParam());
  ASSERT_EQ(client.connectStep("server", 2016), Result::Pending);
  ASSERT_EQ(client.connectStep("server", 2016), Result::Pending);
  client.stop();
  EXPECT_EQ(events, (std::vector<std::string>{"abort:42", "close:42"}));
}
TEST_P(ArduinoEsp32TcpAbortTests, RefusedConnectAbortsAndDoesNotReuseFd) {
  Supla::EspAsyncClient client;
  client.setSSLEnabled(GetParam());
  failConnect = true;
  ASSERT_EQ(client.connectStep("server", 2016), Result::Pending);
  EXPECT_EQ(client.connectStep("server", 2016), Result::Failed);
  client.stop();
  EXPECT_EQ(events, (std::vector<std::string>{"abort:42", "close:42"}));
}
TEST_P(ArduinoEsp32TcpAbortTests, FatalWriteAbortsOnce) {
  Supla::EspAsyncClient client;
  connect(&client);
  fakeWriteFailure = true;
  EXPECT_EQ(client.write(static_cast<uint8_t>(1)), 0U);
  client.stop();
  expectCleanup();
}
TEST_P(ArduinoEsp32TcpAbortTests, FatalReadAbortsOnce) {
  Supla::EspAsyncClient client;
  connect(&client);
  fakeReadFailure = true;
  uint8_t byte;
  EXPECT_EQ(client.read(&byte, 1), 0);
  client.stop();
  expectCleanup();
}
TEST_P(ArduinoEsp32TcpAbortTests, SocketZeroIsOwnedButNotAbortedTwice) {
  Supla::EspAsyncClient client;
  nextFd = 0;
  connect(&client);
  client.stop();
  const auto first = events;
  ASSERT_FALSE(first.empty());
  EXPECT_EQ(first.front(), "abort:0");
  EXPECT_EQ(first.back(), "close:0");
  client.stop();
  EXPECT_EQ(events, first);
}
TEST_P(ArduinoEsp32TcpAbortTests, ReconnectAbortsOnlyCurrentOwner) {
  Supla::EspAsyncClient client;
  connect(&client);
  client.stop();
  EXPECT_EQ(abortCalls(), 1);
  events.clear();
  nextFd = 43;
  Result result = Result::Pending;
  for (int i = 0; i < 8 && result == Result::Pending; ++i)
    result = client.connectStep("server", 2016);
  ASSERT_EQ(result, Result::Connected);
  // Reconnect must not abort or close the already released previous fd.
  EXPECT_TRUE(events.empty());
  client.stop();
  EXPECT_EQ(abortCalls(), 1);
  EXPECT_EQ(events.front(), "abort:43");
  EXPECT_EQ(events.back(), "close:43");
}
TEST(ArduinoEsp32AsyncAbort, CancelDnsWithoutSocketDoesNotAbort) {
  events.clear();
  Supla::EspAsyncClient client;
  ASSERT_EQ(client.connectStep("server", 2016), Result::Pending);
  client.stop();
  EXPECT_TRUE(events.empty());
}
TEST(ArduinoEsp32AsyncAbort, PendingTlsTimeoutAbortsBeforeTlsCleanup) {
  events.clear();
  fakeHandshakeResult = MBEDTLS_ERR_SSL_WANT_READ;
  failConnect = false;
  nextFd = 42;
  Supla::EspAsyncClient client;
  client.setSSLEnabled(true);
  client.setTimeoutMs(100);
  for (int i = 0; i < 4; ++i)
    ASSERT_EQ(client.connectStep("server", 2016), Result::Pending);
  now += 101;
  EXPECT_EQ(client.connectStep("server", 2016), Result::Failed);
  EXPECT_EQ(events, (std::vector<std::string>{
      "abort:42", "tls-free", "close:42"}));
  fakeHandshakeResult = 0;
}
TEST(ArduinoEsp32AsyncAbort, InvalidSocketNeverAbortsOrClosesAnotherOwner) {
  events.clear();
  nextFd = -1;
  Supla::EspAsyncClient client;
  ASSERT_EQ(client.connectStep("server", 2016), Result::Pending);
  EXPECT_EQ(client.connectStep("server", 2016), Result::Failed);
  client.stop();
  EXPECT_TRUE(events.empty());
  nextFd = 42;
}
TEST(ArduinoEsp32AsyncAbort, FailedTlsHandshakeAbortsBeforeTlsCleanup) {
  events.clear();
  fakeHandshakeResult = MBEDTLS_ERR_SSL_INTERNAL_ERROR;
  failConnect = false;
  nextFd = 42;
  Supla::EspAsyncClient client;
  client.setSSLEnabled(true);
  Result result = Result::Pending;
  for (int i = 0; i < 8 && result == Result::Pending; ++i)
    result = client.connectStep("server", 2016);
  EXPECT_EQ(result, Result::Failed);
  EXPECT_EQ(events, (std::vector<std::string>{
      "abort:42", "tls-free", "close:42"}));
  fakeHandshakeResult = 0;
}
INSTANTIATE_TEST_SUITE_P(PlainAndTls, ArduinoEsp32TcpAbortTests,
                        ::testing::Bool());

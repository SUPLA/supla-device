// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>
#include <fake_lwip.h>

#include <algorithm>
#include <string>
#include <vector>

#include "../../../src/supla/arduino_esp_platform.cpp"  // NOLINT(build/include)

class ArduinoEsp32TcpAbortTests : public ::testing::TestWithParam<bool> {
 protected:
  void SetUp() override {
    fakeLwipReset();
    fakeAbortObserver = [](int fd) {
      NetworkClient::events.push_back("abort:" + std::to_string(fd));
    };
    NetworkClient::events.clear();
    NetworkClientSecure::closedSockets.clear();
    NetworkClient::nextFd = 12;
    NetworkClient::failConnect = NetworkClient::failWrite = false;
    NetworkClient::failRead = NetworkClient::failAvailable = false;
    NetworkClient::directCloseOnConnect = false;
    NetworkClient::lastCreated = nullptr;
  }
  void TearDown() override { fakeLwipReset(); }

  void connect(Supla::ArduinoEspClient *client) {
    client->setSSLEnabled(GetParam());
    ASSERT_EQ(client->connect("server", GetParam() ? 2016 : 2015), 1);
    NetworkClient::events.clear();
  }
  void expectAbortThenStop() {
    ASSERT_GE(NetworkClient::events.size(), 2u);
    EXPECT_EQ(NetworkClient::events[0], "abort:12");
    EXPECT_EQ(NetworkClient::events[1], stopEvent());
  }
  int abortCalls() {
    return std::count_if(NetworkClient::events.begin(),
                         NetworkClient::events.end(), [](const std::string &s) {
                           return s.find("abort:") == 0;
                         });
  }
  const char *stopEvent() { return GetParam() ? "secure-stop" : "plain-stop"; }
};

TEST_P(ArduinoEsp32TcpAbortTests, RepeatedStopAbortsOnceBeforeBaseCleanup) {
  Supla::ArduinoEspClient client;
  connect(&client);
  client.stop();
  client.stop();
  expectAbortThenStop();
  EXPECT_EQ(abortCalls(), 1);
}

TEST_P(ArduinoEsp32TcpAbortTests,
       DestructorAbortsBeforeTlsAndSocketDestruction) {
  {
    Supla::ArduinoEspClient client;
    connect(&client);
  }
  expectAbortThenStop();
  EXPECT_EQ(abortCalls(), 1);
  EXPECT_EQ(NetworkClient::events.back(), "socket-destroy");
  if (GetParam()) {
    const auto destroyed = std::find(NetworkClient::events.begin(),
                                     NetworkClient::events.end(),
                                     "tls-destroy");
    ASSERT_NE(destroyed, NetworkClient::events.end());
    EXPECT_GT(destroyed - NetworkClient::events.begin(), 1);
  }
}

TEST_P(ArduinoEsp32TcpAbortTests, AdapterDestructorAlsoStopsWithoutOuterOwner) {
  NetworkClient *client = GetParam() ?
      static_cast<NetworkClient *>(new Supla::AbortableNetworkClientSecure) :
      new Supla::AbortableNetworkClient;
  ASSERT_EQ(client->connect("server", 2016), 1);
  delete client;
  expectAbortThenStop();
  EXPECT_EQ(abortCalls(), 1);
}

TEST_P(ArduinoEsp32TcpAbortTests, InternalWriteErrorUsesVirtualStop) {
  Supla::ArduinoEspClient client;
  connect(&client);
  NetworkClient::failWrite = true;
  EXPECT_EQ(client.write(static_cast<uint8_t>(42)), 0u);
  expectAbortThenStop();
  client.stop();
  EXPECT_EQ(abortCalls(), 1);
}

TEST_P(ArduinoEsp32TcpAbortTests,
       InternalReadAndAvailableErrorsUseVirtualStop) {
  for (bool readFailure : {false, true}) {
    Supla::ArduinoEspClient client;
    connect(&client);
    NetworkClient::failRead = readFailure;
    NetworkClient::failAvailable = !readFailure;
    uint8_t byte = 0;
    if (readFailure) {
      EXPECT_EQ(client.read(&byte, 1), -1);
    } else {
      EXPECT_EQ(client.available(), 0);
    }
    expectAbortThenStop();
    client.stop();
    EXPECT_EQ(abortCalls(), 1);
    NetworkClient::failRead = NetworkClient::failAvailable = false;
  }
}

TEST_P(ArduinoEsp32TcpAbortTests, FailedConnectWithLiveFdIsAborted) {
  Supla::ArduinoEspClient client;
  client.setSSLEnabled(GetParam());
  NetworkClient::failConnect = true;
  EXPECT_EQ(client.connect("server", 2016), 0);
  expectAbortThenStop();
}

TEST_P(ArduinoEsp32TcpAbortTests, AlreadyClosedConnectFailureDoesNotAbortFd) {
  Supla::ArduinoEspClient client;
  client.setSSLEnabled(GetParam());
  NetworkClient::directCloseOnConnect = true;
  EXPECT_EQ(client.connect("server", 2016), 0);
  client.stop();
  EXPECT_EQ(abortCalls(), 0);
}

TEST_P(ArduinoEsp32TcpAbortTests,
       BaseCleanupStillRunsAfterSynchronizationFailure) {
  Supla::ArduinoEspClient client;
  connect(&client);
  fakeResult = ERR_MEM;
  client.stop();
  EXPECT_EQ(NetworkClient::events,
            (std::vector<std::string>{stopEvent()}));
  EXPECT_EQ(NetworkClient::lastCreated->fd(), -1);
  EXPECT_EQ(apiCallCount, 1);
  EXPECT_EQ(abortCount, 0);
}

TEST_P(ArduinoEsp32TcpAbortTests, ReconnectDoesNotAbortStoppedFd) {
  Supla::ArduinoEspClient client;
  connect(&client);
  client.stop();
  EXPECT_EQ(abortCalls(), 1);
  NetworkClient::events.clear();
  EXPECT_EQ(client.connect("server", 2016), 1);
  EXPECT_EQ(abortCalls(), 0);
  client.stop();
  EXPECT_EQ(abortCalls(), 1);
  EXPECT_NE(std::find(NetworkClient::events.begin(),
                      NetworkClient::events.end(), "abort:12"),
            NetworkClient::events.end());
}

TEST_P(ArduinoEsp32TcpAbortTests, FailedAttemptCanBeFollowedByLiveConnection) {
  Supla::ArduinoEspClient client;
  client.setSSLEnabled(GetParam());
  NetworkClient::failConnect = true;
  EXPECT_EQ(client.connect("server", 2016), 0);
  EXPECT_EQ(abortCalls(), 1);
  NetworkClient::failConnect = false;
  NetworkClient::events.clear();
  EXPECT_EQ(client.connect("server", 2016), 1);
  EXPECT_EQ(abortCalls(), 0);
  client.stop();
  EXPECT_EQ(abortCalls(), 1);
}

TEST_P(ArduinoEsp32TcpAbortTests, RealSocketZeroIsAbortedExactlyOnce) {
  NetworkClient::nextFd = 0;
  Supla::ArduinoEspClient client;
  connect(&client);
  client.stop();
  client.stop();
  EXPECT_EQ(abortCalls(), 1);
  ASSERT_FALSE(NetworkClient::events.empty());
  EXPECT_EQ(NetworkClient::events.front(), "abort:0");
}

TEST_P(ArduinoEsp32TcpAbortTests,
       SecureClientDestructionDoesNotAbortStaleZero) {
  Supla::ArduinoEspClient client;
  connect(&client);
  client.stop();
  NetworkClient::events.clear();
  client.setSSLEnabled(!GetParam());
  EXPECT_EQ(client.connect("server", 2016), 1);
  EXPECT_EQ(abortCalls(), 0);
  client.stop();
  EXPECT_EQ(abortCalls(), 1);
}

TEST_P(ArduinoEsp32TcpAbortTests, UnconnectedAdapterDoesNotAbort) {
  NetworkClient *client = GetParam() ?
      static_cast<NetworkClient *>(new Supla::AbortableNetworkClientSecure) :
      new Supla::AbortableNetworkClient;
  EXPECT_EQ(client->fd(), -1);
  client->stop();
  EXPECT_EQ(client->fd(), -1);
  delete client;
  EXPECT_EQ(abortCalls(), 0);
}

class ArduinoEsp32SecureCleanupTests : public ::testing::Test {
 protected:
  void SetUp() override { fakeLwipReset(); }
  void TearDown() override { fakeLwipReset(); }
};

TEST_F(ArduinoEsp32SecureCleanupTests, CoreStopAndDeleterBothCloseTheStoredFd) {
  NetworkClientSecure::closedSockets.clear();
  {
    NetworkClientSecure client;
    EXPECT_EQ(client.fd(), -1);
    client.stop();
    EXPECT_EQ(client.fd(), 0);
  }
  // Demonstrate the real core's hazard, rather than hiding it in the double.
  EXPECT_EQ(NetworkClientSecure::closedSockets, (std::vector<int>{0}));
}

TEST_F(ArduinoEsp32SecureCleanupTests, AdapterNeverClosesStaleFdOnDestruction) {
  for (int fd : {0, 12}) {
    NetworkClientSecure::closedSockets.clear();
    NetworkClient::events.clear();
    NetworkClient::nextFd = fd;
    NetworkClient::failConnect = false;
    NetworkClient::directCloseOnConnect = false;
    {
      Supla::AbortableNetworkClientSecure client;
      ASSERT_EQ(client.connect("server", 2016), 1);
      client.stop();
      EXPECT_EQ(client.fd(), -1);
      client.stop();
      EXPECT_EQ(client.fd(), -1);
    }
    // Includes the derived destructor and the base context's final deleter.
    EXPECT_EQ(NetworkClientSecure::closedSockets, (std::vector<int>{fd}));
  }
}

TEST_F(ArduinoEsp32SecureCleanupTests, UnconnectedAdapterClosesNoSocket) {
  NetworkClientSecure::closedSockets.clear();
  {
    Supla::AbortableNetworkClientSecure client;
    client.stop();
    client.stop();
  }
  EXPECT_TRUE(NetworkClientSecure::closedSockets.empty());
}

INSTANTIATE_TEST_SUITE_P(PlainAndTls, ArduinoEsp32TcpAbortTests,
                         ::testing::Bool());

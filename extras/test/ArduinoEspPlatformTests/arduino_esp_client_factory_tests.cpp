// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>
#include <memory>

#include "../../../src/supla/arduino_esp_platform.cpp"  // NOLINT(build/include)

bool Supla::Crypto::fillRandom(uint8_t *, size_t) { return false; }
namespace {
uint32_t nowMs = 0;
using Transport = Supla::Esp8266AsyncClient;
using Result = Supla::Client::ConnectResult;

class ArduinoClientFactory : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_EQ(0, Transport::live);
    Transport::created = 0;
    Transport::starts = 0;
    Transport::closes = 0;
    Transport::writes = 0;
    Transport::ready = false;
    nowMs = 0;
    client.reset(Supla::ClientBuilder());
  }
  std::unique_ptr<Supla::Client> client;
};

TEST_F(ArduinoClientFactory, ReturnsOneConcreteTransportWithOriginalTimeout) {
  auto transport = dynamic_cast<Transport *>(client.get());
  ASSERT_NE(nullptr, transport);
  EXPECT_EQ(3000, transport->timeout());
  EXPECT_EQ(1, Transport::created);
  EXPECT_EQ(1, Transport::live);
}

TEST_F(ArduinoClientFactory, PendingConnectDoesNotTransmit) {
  EXPECT_EQ(Result::Pending, client->connectStep("server", 2016));
  EXPECT_FALSE(client->connected());
  EXPECT_FALSE(client->isSecure());
  EXPECT_EQ(0U, client->write("test", 4));
  EXPECT_EQ(0, Transport::writes);
}

TEST_F(ArduinoClientFactory, BothApisUseSameTransportAcrossReconnect) {
  auto original = client.get();
  EXPECT_EQ(Result::Pending, client->connectStep("server", 2016));
  Transport::ready = true;
  EXPECT_EQ(Result::Connected, client->connectStep("server", 2016));
  EXPECT_EQ(4U, client->write("test", 4));
  client->stop();
  EXPECT_EQ(1, Transport::live);
  EXPECT_EQ(1, client->connect("other-server", 2016));
  EXPECT_EQ(original, client.get());
  EXPECT_EQ(1, Transport::created);
  EXPECT_EQ(2, Transport::starts);
}

TEST_F(ArduinoClientFactory, ConfigChangesCancelPendingWithoutReplacingClient) {
  client->setSSLEnabled(true);
  EXPECT_EQ(Result::Pending, client->connectStep("server", 2016));
  const int closes = Transport::closes;
  client->setCACert("new CA");
  EXPECT_GT(Transport::closes, closes);
  EXPECT_EQ(Result::Pending, client->connectStep("server", 2016));
  EXPECT_STREQ("new CA", static_cast<Transport *>(client.get())->ca());
  EXPECT_EQ(1, Transport::created);
  EXPECT_EQ(2, Transport::starts);
}

TEST_F(ArduinoClientFactory, TlsAndPlaintextUseSameObjectAndReportActualState) {
  client->setSSLEnabled(true);
  EXPECT_EQ(Result::Pending, client->connectStep("server", 2016));
  Transport::ready = true;
  EXPECT_EQ(Result::Connected, client->connectStep("server", 2016));
  EXPECT_TRUE(client->isSecure());
  client->setSSLEnabled(false);
  EXPECT_FALSE(client->connected());
  EXPECT_FALSE(client->isSecure());
  EXPECT_EQ(1, client->connect("server", 2015));
  EXPECT_FALSE(client->isSecure());
  EXPECT_EQ(1, Transport::created);
}

TEST_F(ArduinoClientFactory, RepeatedReconnectsDoNotAllocateAnotherClient) {
  for (int attempt = 0; attempt < 100; ++attempt) {
    Transport::ready = false;
    EXPECT_EQ(Result::Pending, client->connectStep("server", 2016));
    Transport::ready = true;
    EXPECT_EQ(Result::Connected, client->connectStep("server", 2016));
    client->stop();
    EXPECT_EQ(1, Transport::live);
    EXPECT_EQ(1, Transport::created);
  }
  EXPECT_EQ(100, Transport::starts);
}

TEST_F(ArduinoClientFactory, EndpointChangeRestartsAttemptInSameObject) {
  EXPECT_EQ(Result::Pending, client->connectStep("first", 2016));
  const int closes = Transport::closes;
  EXPECT_EQ(Result::Pending, client->connectStep("second", 2016));
  EXPECT_GT(Transport::closes, closes);
  EXPECT_EQ(2, Transport::starts);
  EXPECT_EQ(1, Transport::created);
}

TEST_F(ArduinoClientFactory, TimeoutStopAndDestructionKeepSingleOwner) {
  EXPECT_EQ(Result::Pending, client->connectStep("server", 2016));
  nowMs = 3000;
  EXPECT_EQ(Result::Failed, client->connectStep("server", 2016));
  EXPECT_EQ(Supla::ConnectionError::CONNECTION_TIMEOUT,
            client->getConnectionError());
  EXPECT_EQ(1, Transport::live);
  EXPECT_EQ(Result::Pending, client->connectStep("server", 2016));
  const int closes = Transport::closes;
  client.reset();
  EXPECT_GT(Transport::closes, closes);
  EXPECT_EQ(0, Transport::live);
  EXPECT_EQ(1, Transport::created);
}
}  // namespace

uint32_t millis() { return nowMs; }
void delay(uint64_t duration) { nowMs += duration; }

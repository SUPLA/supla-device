/*
 * Copyright (C) AC SOFTWARE SP. Z O.O
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#include <stdint.h>

#include <deque>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <SuplaDevice.h>
#include <supla/device/register_device.h>
#include <supla/network/client.h>
#include <supla/network/network.h>
#include <supla/protocol/supla_srpc.h>

extern "C" const char *supla_test_get_last_log();
extern "C" void supla_test_clear_last_log();

namespace {

struct ClientStats {
  enum Event { Stop, Destroy };

  int connectedCalls = 0;
  int connectCalls = 0;
  int stopCalls = 0;
  int destroyed = 0;
  int connectResult = 0;
  std::vector<Event> events;
};

class TrackingClient : public Supla::Client {
 public:
  explicit TrackingClient(ClientStats &stats) : stats(stats) {
  }

  ~TrackingClient() override {
    stats.events.push_back(ClientStats::Destroy);
    stats.destroyed++;
  }

  int available() override {
    return 0;
  }

  void stop() override {
    stats.events.push_back(ClientStats::Stop);
    stats.stopCalls++;
  }

  uint8_t connected() override {
    stats.connectedCalls++;
    return connectedState ? 1 : 0;
  }

  void setTimeoutMs(uint16_t) override {
  }

  SuplaDeviceClass *configuredSdc() const {
    return sdc;
  }

  bool sslEnabledForTest() const {
    return sslEnabled;
  }

 protected:
  int connectImp(const char *, uint16_t) override {
    stats.connectCalls++;
    connectedState = stats.connectResult == 1;
    return stats.connectResult;
  }

  size_t writeImp(const uint8_t *, size_t size) override {
    return size;
  }

  int readImp(uint8_t *, size_t) override {
    return 0;
  }

 private:
  ClientStats &stats;
  bool connectedState = false;
};

class TestNetwork : public Supla::Network {
 public:
  TestNetwork() : Supla::Network() {
  }

  void setup() override {
  }

  void disable() override {
  }

  bool isReady() override {
    return true;
  }

  bool iterate() override {
    return false;
  }

  Supla::Client *createClient() override {
    createClientCalls++;
    if (clients.empty()) {
      return nullptr;
    }

    auto client = clients.front();
    clients.pop_front();
    return client;
  }

  int createClientCalls = 0;
  std::deque<Supla::Client *> clients;
};

class TestSrpc : public Supla::Protocol::SuplaSrpc {
 public:
  explicit TestSrpc(SuplaDeviceClass *sdc)
      : Supla::Protocol::SuplaSrpc(sdc) {
  }

  uint32_t waitForIterateForTest() const {
    return waitForIterate;
  }
};

class SrpcClientManagementTests : public ::testing::Test {
 protected:
  void SetUp() override {
    Supla::RegisterDevice::resetToDefaults();
    supla_test_clear_last_log();
  }

  void TearDown() override {
    Supla::RegisterDevice::resetToDefaults();
  }
};

}  // namespace

TEST_F(SrpcClientManagementTests, InitClientHandlesNullFromNetworkFactory) {
  TestNetwork network;
  SuplaDeviceClass sd;
  TestSrpc srpc(&sd);

  srpc.initClient();

  EXPECT_EQ(network.createClientCalls, 1);
  EXPECT_EQ(srpc.client, nullptr);
  EXPECT_NE(std::string(supla_test_get_last_log()).find(
                "Failed to create SRPC network client"),
            std::string::npos);
}

TEST_F(SrpcClientManagementTests,
       IterateBacksOffWhenNetworkFactoryReturnsNullForSrpc) {
  TestNetwork network;
  SuplaDeviceClass sd;
  Supla::RegisterDevice::setServerName("supla.example");
  Supla::RegisterDevice::setEmail("user@example.com");
  TestSrpc srpc(&sd);

  EXPECT_FALSE(srpc.iterate(0));

  EXPECT_EQ(network.createClientCalls, 1);
  EXPECT_EQ(srpc.client, nullptr);
  EXPECT_EQ(srpc.waitForIterateForTest(), 1000U);
  EXPECT_FALSE(srpc.isNetworkRestartRequested());
}

TEST_F(SrpcClientManagementTests,
       AutodiscoveryHandlesNullFromNetworkFactory) {
  TestNetwork network;
  SuplaDeviceClass sd;
  Supla::RegisterDevice::setEmail("user@example.com");
  TestSrpc srpc(&sd);

  EXPECT_FALSE(srpc.iterate(0));

  EXPECT_EQ(network.createClientCalls, 1);
  EXPECT_EQ(srpc.client, nullptr);
  EXPECT_EQ(srpc.waitForIterateForTest(), 1000U);
  EXPECT_NE(std::string(supla_test_get_last_log()).find(
                "Failed to create autodiscovery network client"),
            std::string::npos);
}

TEST_F(SrpcClientManagementTests,
       FailedAutodiscoveryConnectStopsAndDestroysEachClient) {
  ClientStats firstStats;
  ClientStats secondStats;
  auto firstClient = new TrackingClient(firstStats);
  auto secondClient = new TrackingClient(secondStats);
  firstStats.connectResult = 0;
  secondStats.connectResult = 0;

  TestNetwork network;
  network.clients.push_back(firstClient);
  network.clients.push_back(secondClient);
  SuplaDeviceClass sd;
  Supla::RegisterDevice::setEmail("user@example.com");
  TestSrpc srpc(&sd);

  EXPECT_FALSE(srpc.iterate(0));
  EXPECT_FALSE(srpc.iterate(1000));

  EXPECT_EQ(network.createClientCalls, 2);
  EXPECT_EQ(firstStats.connectCalls, 1);
  EXPECT_EQ(firstStats.stopCalls, 1);
  EXPECT_EQ(firstStats.destroyed, 1);
  ASSERT_EQ(firstStats.events.size(), 2U);
  EXPECT_EQ(firstStats.events[0], ClientStats::Stop);
  EXPECT_EQ(firstStats.events[1], ClientStats::Destroy);
  EXPECT_EQ(secondStats.connectCalls, 1);
  EXPECT_EQ(secondStats.stopCalls, 1);
  EXPECT_EQ(secondStats.destroyed, 1);
  ASSERT_EQ(secondStats.events.size(), 2U);
  EXPECT_EQ(secondStats.events[0], ClientStats::Stop);
  EXPECT_EQ(secondStats.events[1], ClientStats::Destroy);
  EXPECT_EQ(srpc.client, nullptr);
}

TEST_F(SrpcClientManagementTests,
       SuccessfulClientCreationUsesNetworkFactoryAndConfiguresClient) {
  ClientStats stats;
  auto networkClient = new TrackingClient(stats);

  TestNetwork network;
  network.clients.push_back(networkClient);
  SuplaDeviceClass sd;
  TestSrpc srpc(&sd);
  srpc.setServerPort(2016);

  srpc.initClient();
  srpc.initClient();

  EXPECT_EQ(network.createClientCalls, 1);
  EXPECT_EQ(srpc.client, networkClient);
  EXPECT_EQ(networkClient->configuredSdc(), &sd);
  EXPECT_TRUE(networkClient->sslEnabledForTest());
}

TEST_F(SrpcClientManagementTests, OtherClientEntryPointsIgnoreNullClient) {
  SuplaDeviceClass sd;
  TestSrpc srpc(&sd);
  uint8_t buffer[1] = {};

  EXPECT_EQ(Supla::dataRead(buffer, sizeof(buffer), &srpc), 0);
  EXPECT_EQ(Supla::dataWrite(buffer, sizeof(buffer), &srpc), 0);
  srpc.sendChannelStateResult(1, 0);
}

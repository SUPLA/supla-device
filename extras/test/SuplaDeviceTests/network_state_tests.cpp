// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <SuplaDevice.h>
#include <gtest/gtest.h>
#include <supla/device/last_state_logger.h>
#include <supla/network/netif_lan.h>
#include <supla/network/netif_wifi.h>

#include <string>
#include <vector>

namespace {
class RecordingLogger : public Supla::Device::LastStateLogger {
 public:
  void log(const char *message, int) override { entries.emplace_back(message); }
  std::vector<std::string> entries;
};

template <typename Base>
class TestInterface : public Base {
 public:
  void setup() override { this->setAllowDisable(true); }
  void disable() override {
    this->takeAllowDisable();
    this->setLinkConnected(false);
  }
  using Base::reportStartResult;
  using Base::resetLinkState;
  using Base::setStaticIpApplied;
};

template <typename Base>
class NetworkStateTests : public testing::Test {
 protected:
  SuplaDeviceClass device;
  TestInterface<Base> net;
  RecordingLogger *logger = new RecordingLogger;

  void SetUp() override {
    device.setLastStateLogger(logger);
    net.setSuplaDeviceClass(&device);
    net.setup();
    net.setLinkConnected(true);
    net.setIpv4Addr(123);
  }
};
using InterfaceTypes = testing::Types<Supla::LAN, Supla::Wifi>;
TYPED_TEST_SUITE(NetworkStateTests, InterfaceTypes);

TYPED_TEST(NetworkStateTests, LeaseLossLeavesLinkUpAndLogsOnlyOnce) {
  this->net.setIpv4Addr(0);
  this->net.setIpv4Addr(0);
  EXPECT_FALSE(this->net.isReady());
  EXPECT_EQ(this->net.getIP(), 0U);
  EXPECT_TRUE(this->net.isLinkConnected());
  ASSERT_EQ(this->logger->entries.size(), 1U);
  EXPECT_EQ(this->logger->entries[0],
            std::string(this->net.getIntfName()) + ": IP lost");
}

TYPED_TEST(NetworkStateTests, LinkLossClearsIpWithoutDuplicateLeaseLoss) {
  this->net.setLinkConnected(false);
  this->net.setIpv4Addr(0);
  this->net.setLinkConnected(false);
  EXPECT_FALSE(this->net.isReady());
  EXPECT_EQ(this->net.getIP(), 0U);
  ASSERT_EQ(this->logger->entries.size(), 1U);
  EXPECT_EQ(this->logger->entries[0],
            std::string(this->net.getIntfName()) + ": disconnected");
}

TYPED_TEST(NetworkStateTests, DeliberateShutdownIsSilentAndSetupReenablesLogs) {
  this->net.disable();
  this->net.reportStartResult(false);
  this->net.setIpv4Addr(0);
  EXPECT_TRUE(this->logger->entries.empty());
  this->net.setup();
  this->net.setLinkConnected(true);
  this->net.setIpv4Addr(456);
  this->net.setLinkConnected(false);
  EXPECT_EQ(this->logger->entries.size(), 1U);
}

TYPED_TEST(NetworkStateTests, LateEventsCannotRestoreReadinessAfterShutdown) {
  this->net.disable();
  this->net.setLinkConnected(true);
  this->net.setIpv4Addr(123);
  EXPECT_FALSE(this->net.isReady());
  EXPECT_FALSE(this->net.isLinkConnected());
  EXPECT_EQ(this->net.getIP(), 0U);
  EXPECT_TRUE(this->logger->entries.empty());
}

TYPED_TEST(NetworkStateTests, LateIpAfterLinkLossIsIgnored) {
  this->net.setLinkConnected(false);
  this->net.setIpv4Addr(123);
  EXPECT_FALSE(this->net.isReady());
  EXPECT_EQ(this->net.getIP(), 0U);
}

TYPED_TEST(NetworkStateTests, StaticIpLossIsNotReportedAsDhcpFailure) {
  this->net.setStaticIpApplied(true);
  this->net.setIpv4Addr(0);
  EXPECT_FALSE(this->net.isReady());
  EXPECT_TRUE(this->logger->entries.empty());
  this->net.setIpv4Addr(123);
  this->net.setLinkConnected(false);
  EXPECT_EQ(this->logger->entries.size(), 1U);
}

TYPED_TEST(NetworkStateTests, StartFailureDeduplicatesAndSuccessRearmsIt) {
  this->net.reportStartResult(false);
  this->net.reportStartResult(false);
  ASSERT_EQ(this->logger->entries.size(), 1U);
  this->net.reportStartResult(true);
  this->net.reportStartResult(false);
  ASSERT_EQ(this->logger->entries.size(), 2U);
  EXPECT_EQ(this->logger->entries[1],
            std::string(this->net.getIntfName()) + ": start failed");
}

TYPED_TEST(NetworkStateTests, TeardownResetIsSilentAndClearsReadiness) {
  this->net.resetLinkState();
  EXPECT_FALSE(this->net.isReady());
  EXPECT_FALSE(this->net.isLinkConnected());
  EXPECT_EQ(this->net.getIP(), 0U);
  EXPECT_TRUE(this->logger->entries.empty());
}

TEST(WifiStateTests, DetailedReasonsDeduplicateAndRearmAfterReconnect) {
  SuplaDeviceClass device;
  TestInterface<Supla::Wifi> net;
  auto logger = new RecordingLogger;
  device.setLastStateLogger(logger);
  net.setSuplaDeviceClass(&device);
  net.setup();
  net.setWifiConnected(true);
  net.setIpv4Addr(123);
  net.setWifiConnected(false);
  net.logWifiReason(200);
  net.logWifiReason(200);
  ASSERT_EQ(logger->entries.size(), 1U);
  EXPECT_EQ(logger->entries[0], "Wi-Fi: beacon timeout");
  EXPECT_FALSE(net.isReady());
  net.setWifiConnected(true);
  net.setWifiConnected(false);
  net.logWifiReason(200);
  EXPECT_EQ(logger->entries.size(), 2U);
  net.disable();
  net.logWifiReason(202);
  EXPECT_EQ(logger->entries.size(), 2U);
}

TEST(WifiStateTests, ExistingConnectionIsRecoveredWithoutConnectedEvents) {
  TestInterface<Supla::Wifi> net;
  net.setup();
  EXPECT_FALSE(net.isReady());
  net.syncWifiState(true, 123);
  EXPECT_TRUE(net.isReady());
  EXPECT_TRUE(net.isLinkConnected());
  EXPECT_EQ(net.getIP(), 123U);
  net.disable();
  net.syncWifiState(true, 123);
  EXPECT_FALSE(net.isReady());
  EXPECT_FALSE(net.isLinkConnected());
  EXPECT_EQ(net.getIP(), 0U);
}

TEST(WifiStateTests, IncompleteConnectionSnapshotDoesNotMarkReady) {
  TestInterface<Supla::Wifi> net;
  net.setup();
  net.syncWifiState(false, 123);
  EXPECT_FALSE(net.isReady());
  net.syncWifiState(true, 0);
  EXPECT_FALSE(net.isReady());
  // GOT_IP can recover a CONNECTED event missed during initialization.
  net.syncWifiState(true, 456);
  EXPECT_TRUE(net.isReady());
  EXPECT_EQ(net.getIP(), 456U);
}

TEST(WifiStateTests, DelayedLocalDisconnectAfterSetupIsSilent) {
  SuplaDeviceClass device;
  TestInterface<Supla::Wifi> net;
  auto logger = new RecordingLogger;
  device.setLastStateLogger(logger);
  net.setSuplaDeviceClass(&device);
  net.setup();
  net.setWifiConnected(true);
  net.setIpv4Addr(123);
  net.expectWifiDisconnect();
  net.disable();
  net.setup();
  // A snapshot must not consume a still queued local disconnect event.
  net.syncWifiState(true, 123);
  EXPECT_FALSE(net.handleWifiDisconnect(8));
  EXPECT_TRUE(logger->entries.empty());
  net.setWifiConnected(true);
  net.setIpv4Addr(456);
  EXPECT_TRUE(net.handleWifiDisconnect(200));
  ASSERT_EQ(logger->entries.size(), 1U);
  EXPECT_EQ(logger->entries[0], "Wi-Fi: beacon timeout");
  // The local-disconnect suppression is consumed exactly once.
  EXPECT_TRUE(net.handleWifiDisconnect(8));
  ASSERT_EQ(logger->entries.size(), 2U);
  EXPECT_EQ(logger->entries[1], "Wi-Fi: disconnecting from AP");
}

TEST(WifiStateTests, RejectedConnectedPreservesExpectedLocalDisconnect) {
  SuplaDeviceClass device;
  TestInterface<Supla::Wifi> net;
  auto logger = new RecordingLogger;
  device.setLastStateLogger(logger);
  net.setSuplaDeviceClass(&device);
  net.setup();
  net.setWifiConnected(true);
  net.setIpv4Addr(123);
  net.expectWifiDisconnect();
  net.disable();
  // A queued CONNECTED is rejected while shutdown is in effect.
  net.setWifiConnected(true);
  EXPECT_FALSE(net.isLinkConnected());
  EXPECT_FALSE(net.isReady());
  net.setup();
  EXPECT_FALSE(net.handleWifiDisconnect(8));
  EXPECT_TRUE(logger->entries.empty());
  // The expected disconnect was consumed, so subsequent failures are visible.
  EXPECT_TRUE(net.handleWifiDisconnect(200));
  ASSERT_EQ(logger->entries.size(), 1U);
  EXPECT_EQ(logger->entries[0], "Wi-Fi: beacon timeout");
}

TEST(WifiStateTests, RejectedConnectedPreservesReasonDeduplication) {
  SuplaDeviceClass device;
  TestInterface<Supla::Wifi> net;
  auto logger = new RecordingLogger;
  device.setLastStateLogger(logger);
  net.setSuplaDeviceClass(&device);
  net.setup();
  net.logWifiReason(202);
  net.disable();
  net.setWifiConnected(true);
  net.setup();
  net.logWifiReason(202);
  EXPECT_EQ(logger->entries.size(), 1U);
  // An accepted connection starts a new diagnostic history.
  net.setWifiConnected(true);
  net.logWifiReason(202);
  EXPECT_EQ(logger->entries.size(), 2U);
}

TEST(WifiStateTests, ExpectedLocalDisconnectDoesNotHideOtherFailures) {
  SuplaDeviceClass device;
  TestInterface<Supla::Wifi> net;
  auto logger = new RecordingLogger;
  device.setLastStateLogger(logger);
  net.setSuplaDeviceClass(&device);
  net.setup();
  net.expectWifiDisconnect();
  EXPECT_TRUE(net.handleWifiDisconnect(202));
  ASSERT_EQ(logger->entries.size(), 1U);
  EXPECT_EQ(logger->entries[0], "Wi-Fi: auth fail");
}

TEST(WifiStateTests, ConfigModeKeepsUnexpectedDisconnectDiagnostics) {
  SuplaDeviceClass device;
  TestInterface<Supla::Wifi> net;
  auto logger = new RecordingLogger;
  device.setLastStateLogger(logger);
  net.setSuplaDeviceClass(&device);
  net.setup();
  net.setConfigMode();
  EXPECT_TRUE(net.handleWifiDisconnect(202));
  ASSERT_EQ(logger->entries.size(), 1U);
  EXPECT_EQ(logger->entries[0], "Wi-Fi: auth fail");
  net.disable();
  net.logWifiReason(200);
  EXPECT_EQ(logger->entries.size(), 1U);
  net.setNormalMode();
}
}  // namespace

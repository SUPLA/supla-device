// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <srpc_mock.h>
#include <simple_time.h>
#include <supla/network/async_client.h>
#include <supla/device/last_state_logger.h>
#include <supla/device/register_device.h>
#include <supla/protocol/supla_srpc.h>
#include <SuplaDevice.h>
#include <string>
#include <vector>

extern "C" unsigned int supla_test_log_calls();

namespace {
using Result = Supla::Client::ConnectResult;
class StepClient : public Supla::AsyncClient {
 public:
  using AsyncClient::Failure;
  using AsyncClient::kCannotResolveHostname;
  using AsyncClient::kFailedConnectToHost;
  using AsyncClient::kCaParseFailed;
  using AsyncClient::kTlsSetupFailed;
  using AsyncClient::kTlsHandshakeFailed;
  using AsyncClient::kTlsCertVerifyFailed;
  ~StepClient() override { stop(); }
  int available() override { return 0; }
  bool isSecure() const override { return online && sslEnabled; }
  int starts = 0;
  int polls = 0;
  int closes = 0;
  bool online = false;
  Result next = Result::Pending;
  int sendLimit = -1;
  int blockedAttempts = 0;
  SimpleTime *clock = nullptr;
  std::string written;
  std::vector<std::string> attempts;
  uint16_t startBudget = 0;
  bool progressOnPoll = false;
  int pollAdvanceMs = 0;
  // Reported with a failed next result, as transports do.
  Failure failure = Failure(0);
  int tlsError = 0;
  std::string currentHost() const { return connectionHost(); }

 protected:
  Result startTransport(const char *, uint16_t) override {
    ++starts;
    if (startBudget) noteProgress(startBudget);
    return result();
  }
  Result pollTransport() override {
    ++polls;
    if (progressOnPoll) noteProgress();
    if (clock) clock->advance(pollAdvanceMs);
    return result();
  }
  Result result() {
    online = next == Result::Connected;
    if (next == Result::Failed && failure) setFailure(failure, tlsError);
    return next;
  }
  void closeTransport() override { ++closes; online = false; }
  bool transportConnected() override { return online; }
  int readImp(uint8_t *, size_t) override { return -1; }
  int transmit(const uint8_t *data, size_t size) override {
    attempts.emplace_back(reinterpret_cast<const char *>(data), size);
    if (clock) clock->advance(10);
    if (blockedAttempts > 0) {
      --blockedAttempts;
      return -1;
    }
    if (sendLimit < 0) return -1;
    if (static_cast<size_t>(sendLimit) < size) size = sendLimit;
    written.append(reinterpret_cast<const char *>(data), size);
    return size;
  }
};
class StepSrpc : public Supla::Protocol::SuplaSrpc {
 public:
  explicit StepSrpc(SuplaDeviceClass *device) : SuplaSrpc(device, 29) {}
  uint16_t failures() const { return connectionFailCounter; }
  uint32_t pause() const { return waitForIterate; }
};
class RecordingLogger : public Supla::Device::LastStateLogger {
 public:
  void log(const char *text, int) override { entries.emplace_back(text); }
  std::vector<std::string> entries;
};
class AsyncClientReport : public ::testing::Test {
 protected:
  void SetUp() override {
    device.setLastStateLogger(&logger);
    client.setSdc(&device);
    client.setTimeoutMs(50);
  }
  void TearDown() override { device.setLastStateLogger(nullptr); }
  void fail(StepClient::Failure failure, int tlsError = 0) {
    client.next = Result::Failed;
    client.failure = failure;
    client.tlsError = tlsError;
    ASSERT_EQ(client.connectStep("svr.example", 2016), Result::Failed);
  }
  SimpleTime time;
  RecordingLogger logger;
  SuplaDeviceClass device;
  StepClient client;
};
}  // namespace

TEST(AsyncClient, PendingContinuesSameAttemptAndHonorsElapsedDeadline) {
  for (uint32_t start : {0U, UINT32_MAX - 50U}) {
    SCOPED_TRACE(start);
    SimpleTime time;
    time.value = start;
    StepClient client;
    client.setTimeoutMs(100);
    EXPECT_EQ(client.connectStep("server", 2016), Result::Pending);
    for (int i = 0; i < 9; ++i) {
      time.advance(10);
      EXPECT_EQ(client.connectStep("server", 2016), Result::Pending);
    }
    EXPECT_EQ(client.starts, 1);
    time.advance(10);
    EXPECT_EQ(client.connectStep("server", 2016), Result::Failed);
    EXPECT_FALSE(client.connected());
    EXPECT_EQ(client.polls, 9);
  }
}

TEST(AsyncClient, StopAndTargetOrCertificateChangeCancelAttempt) {
  SimpleTime time;
  StepClient client;
  ASSERT_EQ(client.connectStep("old", 2016), Result::Pending);
  client.stop();
  ASSERT_EQ(client.connectStep("new", 2016), Result::Pending);
  ASSERT_EQ(client.connectStep("third", 2016), Result::Pending);
  EXPECT_EQ(client.starts, 3);
  ASSERT_EQ(client.connectStep("third", 2015), Result::Pending);
  EXPECT_EQ(client.starts, 4);
  char ca[] = "CA1";
  client.setCACert(ca);
  ASSERT_EQ(client.connectStep("third", 2015), Result::Pending);
  ca[2] = '2';
  client.setCACert(ca);
  ASSERT_EQ(client.connectStep("third", 2015), Result::Pending);
  EXPECT_EQ(client.starts, 6);
}

TEST(AsyncClient, ConnectedWritesCompleteBeforeReturningWithoutQueue) {
  SimpleTime time;
  StepClient client;
  client.next = Result::Connected;
  client.sendLimit = 2;
  client.blockedAttempts = 1;
  client.clock = &time;
  ASSERT_EQ(client.connectStep("server", 2016), Result::Connected);
  ASSERT_EQ(client.write("abcdefgh", 8), 8U);
  EXPECT_EQ(client.written, "abcdefgh");
  ASSERT_GE(client.attempts.size(), 2U);
  EXPECT_EQ(client.attempts[0], client.attempts[1]);
}

TEST(AsyncClient, BlockedWriterTimesOutAndDoesNotReplayAfterReconnect) {
  SimpleTime time;
  StepClient client;
  client.clock = &time;
  client.setTimeoutMs(30);
  client.next = Result::Connected;
  ASSERT_EQ(client.connectStep("server", 2016), Result::Connected);
  EXPECT_EQ(client.write("old", 3), 0U);
  EXPECT_FALSE(client.connected());
  ASSERT_EQ(client.connectStep("server", 2016), Result::Connected);
  client.sendLimit = 512;
  ASSERT_EQ(client.write("fresh", 5), 5U);
  EXPECT_EQ(client.written, "fresh");
}

TEST(AsyncClient, ConnectedWriteHasNoSrpcSizeLimitAndPendingRejectsTx) {
  SimpleTime time;
  StepClient client;
  ASSERT_EQ(client.connectStep("server", 2016), Result::Pending);
  EXPECT_EQ(client.write("unsent", 6), 0U);
  EXPECT_TRUE(client.attempts.empty());
  client.next = Result::Connected;
  client.sendLimit = 512;
  ASSERT_EQ(client.connectStep("server", 2016), Result::Connected);
  const std::string data(8192, 'x');
  EXPECT_EQ(client.write(data.data(), data.size()), data.size());
  EXPECT_EQ(client.written, data);
}

TEST(AsyncClient, SrpcPendingDoesNotCountFailureOrRestartOrDuplicateAttempt) {
  SimpleTime time;
  Supla::RegisterDevice::resetToDefaults();
  Supla::RegisterDevice::setServerName("server");
  Supla::RegisterDevice::setEmail("user@example.com");
  SuplaDeviceClass device;
  StepSrpc protocol(&device);
  auto client = new StepClient;
  client->setTimeoutMs(1000);
  protocol.setNetworkClient(client);
  for (int i = 0; i < 50; ++i) {
    EXPECT_FALSE(protocol.iterate(time.value));
    EXPECT_EQ(protocol.failures(), 0);
    EXPECT_EQ(protocol.pause(), 0U);
    EXPECT_FALSE(protocol.isNetworkRestartRequested());
    time.advance(10);
  }
  EXPECT_EQ(client->starts, 1);
  time.advance(500);
  EXPECT_FALSE(protocol.iterate(time.value));
  EXPECT_EQ(protocol.failures(), 1);
  EXPECT_EQ(protocol.pause(), 1000U);
  Supla::RegisterDevice::resetToDefaults();
}

TEST(AsyncClient, SrpcPortChangeAndDisconnectCancelWithoutCountingFailure) {
  SimpleTime time;
  Supla::RegisterDevice::resetToDefaults();
  Supla::RegisterDevice::setServerName("server");
  Supla::RegisterDevice::setEmail("user@example.com");
  SuplaDeviceClass device;
  StepSrpc protocol(&device);
  auto client = new StepClient;
  protocol.setNetworkClient(client);
  ASSERT_FALSE(protocol.iterate(time.value));
  const int before = client->closes;
  protocol.setServerPort(1234);
  EXPECT_EQ(client->closes, before + 1);
  EXPECT_EQ(protocol.failures(), 0);
  ASSERT_FALSE(protocol.iterate(time.value));
  EXPECT_EQ(client->starts, 2);
  const int active = client->closes;
  protocol.disconnect();
  EXPECT_EQ(client->closes, active + 1);
  EXPECT_EQ(protocol.failures(), 0);
  Supla::RegisterDevice::resetToDefaults();
}

TEST(AsyncClient, SrpcPendingCompletesAndRegistersWithoutNewAttempt) {
  using ::testing::_;
  SimpleTime time;
  Supla::RegisterDevice::resetToDefaults();
  Supla::RegisterDevice::setServerName("server");
  Supla::RegisterDevice::setEmail("user@example.com");
  ::testing::NiceMock<SrpcMock> wire;
  SuplaDeviceClass device;
  StepSrpc protocol(&device);
  protocol.setServerPort(2016);
  auto client = new StepClient;
  protocol.setNetworkClient(client);
  for (int i = 0; i < 10; ++i) {
    ASSERT_FALSE(protocol.iterate(time.value));
    time.advance(10);
  }
  EXPECT_EQ(client->starts, 1);
  int handle = 0;
  EXPECT_CALL(wire, srpc_init(_)).WillOnce(::testing::Return(&handle));
  ON_CALL(wire, srpc_iterate(_)).WillByDefault(::testing::Return(1));
  EXPECT_CALL(wire, srpc_ds_async_registerdevice_in_chunks_g(_, _))
      .WillOnce(::testing::Return(1));
  client->next = Result::Connected;
  protocol.iterate(time.value);
  EXPECT_EQ(client->starts, 1);
  EXPECT_EQ(protocol.failures(), 0);
  EXPECT_TRUE(client->connected());
  TSD_SuplaRegisterDeviceResult result = {};
  result.result_code = SUPLA_RESULTCODE_TRUE;
  result.activity_timeout = 30;
  result.version = 29;
  result.version_min = 1;
  protocol.onRegisterResult(&result);
  EXPECT_TRUE(protocol.isRegisteredAndReady());
  Supla::RegisterDevice::resetToDefaults();
}

TEST(AsyncClient, HandshakeProgressRestartsInactivityDeadline) {
  SimpleTime time;
  StepClient client;
  client.setTimeoutMs(100);
  client.progressOnPoll = true;
  ASSERT_EQ(client.connectStep("server", 2016), Result::Pending);
  // A slow but progressing handshake is not limited by total elapsed time.
  for (int i = 0; i < 20; ++i) {
    time.advance(90);
    ASSERT_EQ(client.connectStep("server", 2016), Result::Pending) << i;
  }
  client.progressOnPoll = false;
  time.advance(90);
  EXPECT_EQ(client.connectStep("server", 2016), Result::Pending);
  time.advance(10);
  EXPECT_EQ(client.connectStep("server", 2016), Result::Failed);
  EXPECT_EQ(client.getConnectionError(),
            Supla::ConnectionError::CONNECTION_TIMEOUT);
  EXPECT_EQ(client.starts, 1);
}

TEST(AsyncClient, ResolverPhaseKeepsTenSecondAllowance) {
  SimpleTime time;
  StepClient client;
  client.setTimeoutMs(3000);
  client.startBudget = 10000;
  ASSERT_EQ(client.connectStep("server", 2016), Result::Pending);
  time.advance(9999);
  EXPECT_EQ(client.connectStep("server", 2016), Result::Pending);
  time.advance(1);
  EXPECT_EQ(client.connectStep("server", 2016), Result::Failed);
  // A shorter phase budget never reduces the configured timeout.
  client.startBudget = 10;
  ASSERT_EQ(client.connectStep("server", 2016), Result::Pending);
  time.advance(2999);
  EXPECT_EQ(client.connectStep("server", 2016), Result::Pending);
}

// Texts of the former EspIdfClient::logConnReason(), shared by all backends.
TEST_F(AsyncClientReport, FailuresUseEspIdfClientTexts) {
  ASSERT_EQ(client.connectStep("svr.example", 2016), Result::Pending);
  time.advance(50);
  ASSERT_EQ(client.connectStep("svr.example", 2016), Result::Failed);
  fail(StepClient::kCannotResolveHostname);
  fail(StepClient::kFailedConnectToHost);
  fail(StepClient::kCaParseFailed, 0x2180);
  fail(StepClient::kTlsHandshakeFailed, StepClient::kTlsCertVerifyFailed);
  fail(StepClient::kTlsHandshakeFailed, 62);
  fail(StepClient::kTlsSetupFailed, 0x7100);
  EXPECT_EQ(logger.entries, std::vector<std::string>({
      "Connection: connection timeout to host \"svr.example\"",
      "Connection: can't resolve hostname \"svr.example\"",
      "Connection: failed connect to host \"svr.example\"",
      "CA certificate parsing error - please provide correct one",
      "Connection TLS: handshake fail - server certificate verification "
      "error \"svr.example\"",
      "Connection TLS: handshake fail (TLS 0x3E, \"svr.example\")",
      "Connection: error 0x8017 (TLS 0x7100 \"svr.example\")",
  }));
}

TEST_F(AsyncClientReport, RepeatedFailureIsLoggedOnce) {
  fail(StepClient::kTlsHandshakeFailed, 62);
  fail(StepClient::kTlsHandshakeFailed, 62);
  // As in EspIdfClient, a successful connection does not reset the filter.
  client.next = Result::Connected;
  ASSERT_EQ(client.connectStep("svr.example", 2016), Result::Connected);
  client.stop();  // Cancellation is not a failure.
  fail(StepClient::kTlsHandshakeFailed, 62);
  fail(StepClient::kTlsHandshakeFailed, 54);
  fail(StepClient::kTlsHandshakeFailed, 62);
  // A failure without a reason (e.g. waiting for NTP) is not reported.
  fail(StepClient::Failure(0));
  client.setSdc(nullptr);
  fail(StepClient::kFailedConnectToHost);
  EXPECT_EQ(logger.entries.size(), 3U);
}

TEST(AsyncClient, HostIsKeptWhileConnectedAndLimitedTo255Bytes) {
  SimpleTime time;
  StepClient client;
  client.next = Result::Connected;
  ASSERT_EQ(client.connectStep("first.example", 2016), Result::Connected);
  EXPECT_EQ(client.currentHost(), "first.example");
  ASSERT_EQ(client.connectStep("first.example", 2016), Result::Connected);
  EXPECT_EQ(client.starts, 1);
  ASSERT_EQ(client.connectStep("second.example", 2016), Result::Connected);
  EXPECT_EQ(client.starts, 2);
  client.stop();
  EXPECT_EQ(client.currentHost(), "");
  const std::string longest(255, 'h');
  EXPECT_EQ(client.connectStep(longest.c_str(), 2015), Result::Connected);
  EXPECT_EQ(client.connectStep(std::string(256, 'h').c_str(), 2015),
            Result::Failed);
}

TEST(AsyncClient, LegacyConnectLogsAttemptOnceAndEndsAtDeadline) {
  SimpleTime time;
  StepClient client;
  client.setTimeoutMs(20);
  const unsigned int before = supla_test_log_calls();
  client.next = Result::Connected;
  ASSERT_EQ(client.connect("server", 2016), 1);
  // "Establishing connection ..." and "Connected via IP ...".
  EXPECT_EQ(supla_test_log_calls() - before, 2U);

  client.next = Result::Pending;
  client.clock = &time;
  client.pollAdvanceMs = 10;
  EXPECT_EQ(client.connect("server", 2016), 0);
  EXPECT_EQ(client.polls, 2);
  EXPECT_EQ(client.getConnectionError(),
            Supla::ConnectionError::CONNECTION_TIMEOUT);
}

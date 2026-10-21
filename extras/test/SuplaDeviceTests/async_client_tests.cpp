// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <srpc_mock.h>
#include <simple_time.h>
#include <supla/network/async_client.h>
#include <supla/device/register_device.h>
#include <supla/protocol/supla_srpc.h>
#include <SuplaDevice.h>
#include <string>
#include <vector>

namespace {
using Result = Supla::Client::ConnectResult;
class StepClient : public Supla::AsyncClient {
 public:
  ~StepClient() override { stop(); }
  int available() override { return 0; }
  bool isSecure() const override { return online && sslEnabled; }
  int starts = 0;
  int polls = 0;
  int closes = 0;
  bool online = false;
  Result next = Result::Pending;
  int sendLimit = -1;
  std::string written;
  std::vector<std::string> attempts;

 protected:
  Result startTransport(const char *, uint16_t) override {
    ++starts;
    online = next == Result::Connected;
    return next;
  }
  Result pollTransport() override {
    ++polls;
    online = next == Result::Connected;
    return next;
  }
  void closeTransport() override { ++closes; online = false; }
  bool transportConnected() override { return online; }
  int readImp(uint8_t *, size_t) override { return -1; }
  int transmit(const uint8_t *data, size_t size) override {
    attempts.emplace_back(reinterpret_cast<const char *>(data), size);
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

TEST(AsyncClient, PartialWritesPreserveOrderAndWantRetryBytes) {
  SimpleTime time;
  StepClient client;
  client.next = Result::Connected;
  ASSERT_EQ(client.connectStep("server", 2016), Result::Connected);
  ASSERT_EQ(client.write("abc", 3), 3U);
  client.service();
  ASSERT_TRUE(client.hasPendingWrites());
  ASSERT_EQ(client.write("defgh", 5), 5U);
  client.service();
  ASSERT_EQ(client.attempts.size(), 2U);
  EXPECT_EQ(client.attempts[0], "abc");
  EXPECT_EQ(client.attempts[1], "abc");
  client.sendLimit = 2;
  for (int i = 0; i < 5; ++i) {
    time.advance(10);
    client.service();
  }
  EXPECT_EQ(client.written, "abcdefgh");
  EXPECT_FALSE(client.hasPendingWrites());
}

TEST(AsyncClient, BlockedWriterTimesOutAndDoesNotReplayAfterReconnect) {
  SimpleTime time;
  StepClient client;
  client.setTimeoutMs(100);
  client.next = Result::Connected;
  ASSERT_EQ(client.connectStep("server", 2016), Result::Connected);
  ASSERT_EQ(client.write("old", 3), 3U);
  time.advance(100);
  client.service();
  EXPECT_FALSE(client.connected());
  EXPECT_FALSE(client.hasPendingWrites());
  ASSERT_EQ(client.connectStep("server", 2016), Result::Connected);
  client.sendLimit = 512;
  ASSERT_EQ(client.write("fresh", 5), 5U);
  client.service();
  EXPECT_EQ(client.written, "fresh");
}

TEST(AsyncClient, OutputCapacityIsBoundedAndOverflowClosesConnection) {
  SimpleTime time;
  StepClient client;
  client.next = Result::Connected;
  ASSERT_EQ(client.connectStep("server", 2016), Result::Connected);
  std::string data(512, 'x');
  int accepted = 0;
  while (client.write(data.data(), data.size())) {
    ++accepted;
    ASSERT_LT(accepted, 100);
  }
  EXPECT_GT(accepted, 1);
  EXPECT_FALSE(client.connected());
  EXPECT_FALSE(client.hasPendingWrites());
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
  EXPECT_FALSE(client->hasPendingWrites());
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

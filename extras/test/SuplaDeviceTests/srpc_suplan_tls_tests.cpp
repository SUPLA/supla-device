// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <SuplaDevice.h>
#include <simple_time.h>
#include <srpc_mock.h>
#include <supla/device/register_device.h>
#include <supla/network/client.h>
#include <supla/protocol/supla_srpc.h>
#include <supla/storage/key_value.h>
#include <supla/storage/storage.h>

#include <memory>
#include <string>

namespace {
using ::testing::_;
using ::testing::Return;
std::string &tlsDiagnostic() {
  static std::string message;
  return message;
}
class TlsConfig : public Supla::KeyValue {
 public:
  bool init() override { return true; }
  using KeyValue::getBlobSize;
  bool commit() override { return ++commits != failCommitNumber; }
  int commits = 0;
  int failCommitNumber = -1;
};
class TlsClient : public Supla::Client {
 public:
  int available() override { return 0; }
  void service() override { ++serviceCalls; }
  void stop() override { online = false; }
  uint8_t connected() override { return online; }
  void setTimeoutMs(uint16_t) override {}
  bool isSecure() const override {
    return online && sslEnabled && tlsSession;
  }
  bool online = false;
  bool tlsSession = true;
  bool succeeds = true;
  bool lastTls = false;
  uint16_t lastPort = 0;
  int attempts = 0;
  int serviceCalls = 0;

 protected:
  int connectImp(const char *, uint16_t port) override {
    ++attempts;
    lastTls = sslEnabled;
    lastPort = port;
    online = succeeds;
    return online;
  }
  size_t writeImp(const uint8_t *, size_t count) override { return count; }
  int readImp(uint8_t *, size_t) override { return 0; }
};
class TlsSrpc : public Supla::Protocol::SuplaSrpc {
 public:
  explicit TlsSrpc(SuplaDeviceClass *device) : SuplaSrpc(device, 29) {}
  uint32_t retryDelay() const { return waitForIterate; }
};
class TlsStack : public Supla::Protocol::ProtocolLayer {
 public:
  explicit TlsStack(SuplaDeviceClass *device) : ProtocolLayer(device) {}
  bool isSupLan() const override { return true; }
  bool isSupLanServerCapable() const override { return true; }
  void onInit() override {}
  bool onLoadConfig() override { return true; }
  bool verifyConfig() override { return true; }
  bool isEnabled() override { return true; }
  void disconnect() override {}
  bool iterate(uint32_t) override { return false; }
  bool isNetworkRestartRequested() override { return false; }
  uint32_t getConnectionFailTime() override { return 0; }
  bool isRegisteredAndReady() override { return true; }
  void sendActionTrigger(uint8_t, uint32_t) override {}
  void sendChannelValueChanged(uint8_t, int8_t *, uint8_t, uint32_t) override {}
  void sendExtendedChannelValueChanged(
      uint8_t, TSuplaChannelExtendedValue *) override {}
};
class SupLanTls : public ::testing::Test {
 protected:
  void SetUp() override {
    previousTls = Supla::Protocol::SuplaSrpc::isSuplaSSLEnabled;
    Supla::Protocol::SuplaSrpc::isSuplaSSLEnabled = true;
    Supla::RegisterDevice::resetToDefaults();
    Supla::Storage::SetConfigInstance(&config);
    Supla::RegisterDevice::setServerName("server.example");
    Supla::RegisterDevice::setEmail("user@example.com");
    device.setStatusFuncImpl([](int, const char *message) {
      tlsDiagnostic() = message;
    });
    protocol = std::make_unique<TlsSrpc>(&device);
    protocol->restoreServerIdentity();
    protocol->setSuplaCACert("CA");
    protocol->setSupla3rdPartyCACert("CA");
    client = new TlsClient;
    protocol->setNetworkClient(client);
    client->setCACert("CA");
    ON_CALL(wire, srpc_init(_)).WillByDefault(Return(&handle));
    ON_CALL(wire, srpc_iterate(_)).WillByDefault(Return(SUPLA_RESULT_TRUE));
    ON_CALL(wire, srpc_ds_async_registerdevice_in_chunks_g(_, _))
        .WillByDefault([this](void *, TDS_SuplaRegisterDeviceHeader *header) {
          ++registrations;
          advertisedSupLan = header->Flags & SUPLA_DEVICE_FLAG_SUPLAN_SUPPORTED;
          return 1;
        });
  }
  void TearDown() override {
    protocol.reset();
    stack.reset();
    Supla::Storage::SetConfigInstance(nullptr);
    Supla::RegisterDevice::resetToDefaults();
    Supla::Protocol::SuplaSrpc::isSuplaSSLEnabled = previousTls;
  }
  void enable() { stack = std::make_unique<TlsStack>(&device); }
  SimpleTime time;
  TlsConfig config;
  ::testing::NiceMock<SrpcMock> wire;
  SuplaDeviceClass device;
  std::unique_ptr<TlsSrpc> protocol;
  std::unique_ptr<TlsStack> stack;
  TlsClient *client = nullptr;
  bool previousTls = true;
  int handle = 0;
  int registrations = 0;
  bool advertisedSupLan = false;
};
TEST_F(SupLanTls, TlsRegistersWithFlag) {
  enable();
  protocol->iterate(1);
  EXPECT_EQ(registrations, 1);
  EXPECT_TRUE(advertisedSupLan);
  EXPECT_TRUE(client->lastTls);
}
TEST_F(SupLanTls, PlaintextConfigurationNeverConnectsOrRegisters) {
  enable();
  protocol->setServerPort(2015);
  protocol->iterate(1);
  EXPECT_EQ(client->attempts, 0);
  EXPECT_EQ(registrations, 0);
  EXPECT_NE(tlsDiagnostic().find("requires TLS"), std::string::npos);
  EXPECT_GT(protocol->retryDelay(), 0u);
}
TEST_F(SupLanTls, LegacyPlaintextStillRegisters) {
  protocol->setServerPort(2015);
  client->setSSLEnabled(false);
  protocol->iterate(1);
  EXPECT_EQ(registrations, 1);
  EXPECT_FALSE(advertisedSupLan);
  EXPECT_FALSE(client->lastTls);
}
TEST_F(SupLanTls, NonTlsConnectionCannotRegister) {
  enable();
  client->tlsSession = false;
  protocol->iterate(1);
  EXPECT_EQ(registrations, 0);
  EXPECT_FALSE(client->online);
}
TEST_F(SupLanTls, TlsWithoutCertificateValidationRegisters) {
  enable();
  client->setCACert(nullptr);
  protocol->iterate(1);
  EXPECT_EQ(client->attempts, 1);
  EXPECT_EQ(registrations, 1);
  EXPECT_TRUE(client->lastTls);
}
TEST_F(SupLanTls, FailedTlsHandshakeCannotRegisterOrDowngrade) {
  enable();
  client->succeeds = false;
  protocol->iterate(1);
  EXPECT_EQ(client->attempts, 1);
  EXPECT_TRUE(client->lastTls);
  EXPECT_EQ(registrations, 0);
  EXPECT_FALSE(client->online);
  protocol->iterate(1002);
  EXPECT_EQ(client->attempts, 2);
  EXPECT_TRUE(client->lastTls);
  EXPECT_EQ(registrations, 0);
}
TEST_F(SupLanTls, ProgrammaticPlaintextDefaultNeverConnects) {
  enable();
  Supla::Protocol::SuplaSrpc::isSuplaSSLEnabled = false;
  protocol->iterate(1);
  EXPECT_EQ(client->attempts, 0);
  EXPECT_EQ(registrations, 0);
}
TEST_F(SupLanTls, StoredPlaintextRejectedAndTlsWithoutVerificationAllowed) {
  enable();
  ASSERT_TRUE(config.setSuplaServer("server.example"));
  ASSERT_TRUE(config.setEmail("user@example.com"));
  config.setSuplaServerPort(2015);
  protocol->onLoadConfig();
  protocol->iterate(1);
  EXPECT_EQ(client->attempts, 0);
  config.setSuplaServerPort(2016);
  config.setUInt8("security_level", 2);
  protocol->onLoadConfig();
  protocol->iterate(1002);
  EXPECT_EQ(client->attempts, 1);
  EXPECT_EQ(registrations, 1);
  EXPECT_TRUE(client->lastTls);
  config.setUInt8("security_level", 0);
  protocol->onLoadConfig();
  protocol->iterate(6003);
  EXPECT_EQ(registrations, 2);
}
TEST_F(SupLanTls, CustomTlsPortUsesActualTlsState) {
  enable();
  protocol->setServerPort(4242);
  protocol->iterate(1);
  EXPECT_EQ(registrations, 1);
  EXPECT_EQ(client->lastPort, 4242);
  EXPECT_TRUE(client->lastTls);
}
TEST_F(SupLanTls, ReconnectAndConfigChangeCannotDowngrade) {
  enable();
  protocol->iterate(1);
  ASSERT_EQ(registrations, 1);
  protocol->disconnect();
  protocol->setServerPort(2015);
  protocol->iterate(2);
  EXPECT_EQ(registrations, 1);
  EXPECT_EQ(client->attempts, 1);
  protocol->setServerPort(2016);
  protocol->iterate(1002);
  EXPECT_EQ(registrations, 2);
  EXPECT_TRUE(client->lastTls);
}
TEST_F(SupLanTls, TlsRequiredReportsTransportErrorAndUsesExistingBackoff) {
  enable();
  protocol->iterate(1);
  ASSERT_EQ(registrations, 1);
  TSD_SuplaRegisterDeviceResult result = {};
  result.result_code = SUPLA_RESULTCODE_TLS_REQUIRED;
  protocol->onRegisterResult(&result);
  EXPECT_EQ(device.getCurrentStatus(), STATUS_SERVER_DISCONNECTED);
  EXPECT_NE(tlsDiagnostic().find("requires TLS"),
            std::string::npos);
  protocol->iterate(2);
  EXPECT_FALSE(protocol->isRegisteredAndReady());
  for (uint32_t now = 3; now < 1000; ++now) protocol->iterate(now);
  EXPECT_EQ(registrations, 1);
  protocol->iterate(1002);
  EXPECT_EQ(registrations, 2);
  EXPECT_TRUE(advertisedSupLan);
  EXPECT_TRUE(client->lastTls);
}
TEST_F(SupLanTls, FlagAloneCannotUseAnAlreadyOpenPlaintextConnection) {
  client->online = true;
  client->setSSLEnabled(false);
  Supla::RegisterDevice::addFlags(SUPLA_DEVICE_FLAG_SUPLAN_SUPPORTED);
  protocol->iterate(1);
  EXPECT_EQ(registrations, 0);
  EXPECT_EQ(client->serviceCalls, 0);  // No buffered dispatch before TLS gate.
  EXPECT_FALSE(client->online);
  EXPECT_TRUE(Supla::RegisterDevice::getRegDevHeaderPtr()->Flags &
              SUPLA_DEVICE_FLAG_SUPLAN_SUPPORTED);
}
TEST_F(SupLanTls, ClearingFlagDoesNotPermitPlaintextWithActiveStack) {
  enable();
  protocol->iterate(1);
  ASSERT_EQ(registrations, 1);
  TSD_SuplaRegisterDeviceResult result = {};
  result.result_code = SUPLA_RESULTCODE_TRUE;
  result.activity_timeout = 30;
  protocol->onRegisterResult(&result);
  ASSERT_TRUE(protocol->isRegisteredAndReady());
  Supla::RegisterDevice::getRegDevHeaderPtr()->Flags &=
      ~SUPLA_DEVICE_FLAG_SUPLAN_SUPPORTED;
  client->setSSLEnabled(false);
  EXPECT_FALSE(protocol->isRegisteredAndReady());
  const char packet[] = "not-sent";
  EXPECT_EQ(Supla::dataWrite(const_cast<char *>(packet), sizeof(packet),
                            protocol.get()), 0);
  protocol->iterate(2);
  EXPECT_FALSE(client->online);
  EXPECT_EQ(registrations, 1);
}
TEST_F(SupLanTls, RegistrationRechecksTransportAfterSrpcCallback) {
  enable();
  ON_CALL(wire, srpc_iterate(_)).WillByDefault([this](void *) {
    client->setSSLEnabled(false);
    return SUPLA_RESULT_TRUE;
  });
  protocol->iterate(1);
  EXPECT_EQ(registrations, 0);
  EXPECT_FALSE(client->online);
}
TEST_F(SupLanTls, AssociationCallbacksRejectNonTlsTransport) {
  enable();
  protocol->iterate(1);
  client->tlsSession = false;
  TSDS_SuplaSetSuplanSourceAssociation source = {};
  TSDS_SuplaSetSuplanDestinationAssociation destination = {};
  EXPECT_CALL(wire, sourceAssociationResult(_, _)).Times(0);
  EXPECT_CALL(wire, destinationAssociationResult(_, _)).Times(0);
  protocol->onSetSuplanSourceAssociation(&source);
  protocol->onSetSuplanDestinationAssociation(&destination);
}
}  // namespace

TEST_F(SupLanTls, ChannelConfigUploadWaitsForCurrentRegistrationIdentities) {
  enable();
  protocol->iterate(1);
  TSD_SuplaRegisterDeviceResult result = {};
  result.result_code = SUPLA_RESULTCODE_TRUE;
  result.activity_timeout = 30;
  protocol->onRegisterResult(&result);
  ASSERT_TRUE(protocol->isRegisteredAndReady());
  TChannelConfig_HVAC value = {};
  EXPECT_FALSE(protocol->setChannelConfig(0, SUPLA_CHANNELFNC_HVAC_THERMOSTAT,
      &value, sizeof(value), SUPLA_CONFIG_TYPE_DEFAULT));
}

TEST_F(SupLanTls, SleepingDeviceWaitsForDurableIdentitySync) {
  enable();
  Supla::RegisterDevice::addFlags(SUPLA_DEVICE_FLAG_SLEEP_MODE_ENABLED);
  protocol->iterate(1);
  TSD_SuplaRegisterDeviceResult result = {};
  result.result_code = SUPLA_RESULTCODE_TRUE;
  result.activity_timeout = 30;
  protocol->onRegisterResult(&result);
  ASSERT_TRUE(protocol->isRegisteredAndReady());
  TSD_SuplaDeviceIdentities ids = {};
  ids.DeviceId = 101;
  ASSERT_EQ(protocol->serverIdentity().accept(ids).Result,
            SUPLA_SUPLAN_RESULT_OK);
  protocol->onDeviceSyncDone();
  ASSERT_FALSE(protocol->isUpdatePending());

  // Exercise both the ready-identity commit and the fence-erasure commit.
  for (int phase = 1; phase <= 2; ++phase) {
    ids.DeviceId += 101;
    ASSERT_EQ(protocol->serverIdentity().accept(ids).Result,
              SUPLA_SUPLAN_RESULT_OK);
    ASSERT_GE(config.getBlobSize("sl-migration"), 0);
    config.failCommitNumber = config.commits + phase;
    protocol->onDeviceSyncDone();
    EXPECT_FALSE(protocol->serverIdentity().serverSyncComplete());
    EXPECT_TRUE(protocol->isUpdatePending());
    EXPECT_GE(config.getBlobSize("sl-migration"), 0);

    config.failCommitNumber = -1;
    // A failed identity write invalidates durability; replaying the current
    // authenticated snapshot repairs it before retrying SYNC_DONE.
    ASSERT_EQ(protocol->serverIdentity().accept(ids).Result,
              SUPLA_SUPLAN_RESULT_OK);
    protocol->onDeviceSyncDone();
    EXPECT_TRUE(protocol->serverIdentity().serverSyncComplete());
    EXPECT_LT(config.getBlobSize("sl-migration"), 0);
    EXPECT_FALSE(protocol->isUpdatePending());
  }
}

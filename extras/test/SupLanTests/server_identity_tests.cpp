// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <LittleFS.h>
#include <crypto_test_hooks.h>
#include <SuplaDevice.h>
#include <gtest/gtest.h>
#include <network_client_mock.h>
#include <simple_time.h>
#include <srpc_mock.h>
#include <supla/channels/channel.h>
#include <supla/crc16.h>
#include <supla/device/register_device.h>
#include <supla/device/server_identity.h>
#include <supla/protocol/supla_srpc.h>
#include <supla/storage/key_value.h>
#include <supla/storage/littlefs_config.h>

#include <array>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace {
using Supla::Device::ServerChannelLocation;
using Supla::Device::ServerIdentity;
using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

class DurableConfig : public Supla::KeyValue {
 public:
  bool init() override { return true; }
  bool commit() override {
    ++commits;
    if (commits == failCommit) {
      if (loseDurableOnFailure) {
        durable.clear();
      }
      return false;
    }
    std::array<uint8_t, 2048> bytes = {};
    size_t size = serializeToMemory(bytes.data(), bytes.size());
    if (size == SIZE_MAX) {
      return false;
    }
    durable.assign(bytes.begin(), bytes.begin() + size);
    return true;
  }
  void reboot() {
    removeAllMemory();
    if (!durable.empty()) {
      ASSERT_TRUE(initFromMemory(durable.data(), durable.size()));
    }
  }
  std::vector<uint8_t> blob(const char *key) {
    int size = getBlobSize(key);
    if (size < 0) {
      return {};
    }
    std::vector<uint8_t> data(size);
    EXPECT_TRUE(getBlob(key, reinterpret_cast<char *>(data.data()), size));
    return data;
  }
  void install(const char *key, const std::vector<uint8_t> &data) {
    ASSERT_TRUE(
        setBlob(key, reinterpret_cast<const char *>(data.data()), data.size()));
    ASSERT_TRUE(commit());
  }
  std::vector<uint8_t> durable;
  int commits = 0;
  int failCommit = -1;
  bool loseDurableOnFailure = false;
};

void seal(std::vector<uint8_t> *bytes) {
  auto &b = *bytes;
  uint16_t crc = calculateCrc16(b.data(), b.size() - 2);
  b[b.size() - 2] = static_cast<uint8_t>(crc);
  b[b.size() - 1] = static_cast<uint8_t>(crc >> 8);
}

class ServerIdentityTests : public ::testing::Test {
 protected:
  void SetUp() override {
    Supla::Channel::resetToDefaults();
    Supla::RegisterDevice::resetToDefaults();
    first = new Supla::Channel(7);
    second = new Supla::Channel(3);
    first->setType(SUPLA_CHANNELTYPE_THERMOMETER);
    second->setType(SUPLA_CHANNELTYPE_RELAY);
    state.load(&config);
    registerState();
  }
  void TearDown() override {
    delete second;
    delete first;
    Supla::Channel::resetToDefaults();
    Supla::RegisterDevice::resetToDefaults();
  }
  void registerState() {
    ASSERT_TRUE(state.registrationStarted());
    state.registrationSucceeded();
  }
  TSD_SuplaDeviceIdentities snapshot() {
    TSD_SuplaDeviceIdentities s = {};
    s.DeviceId = 101;
    s.ChannelCount = 2;
    s.ChannelId[0] = 501;
    s.ChannelId[1] = 502;
    return s;
  }
  void acceptInitial() {
    ASSERT_EQ(state.accept(snapshot()).Result, SUPLA_SUPLAN_RESULT_OK);
  }
  DurableConfig config;
  ServerIdentity state;
  Supla::Channel *first = nullptr;
  Supla::Channel *second = nullptr;
};

TEST_F(ServerIdentityTests, MissingIdentityIsUnresolved) {
  EXPECT_EQ(state.resolve(501).location, ServerChannelLocation::kUnresolved);
  uint32_t id = 0;
  EXPECT_FALSE(state.reverse(7, &id));
}
TEST_F(ServerIdentityTests, PositionalMapUsesExplicitNumbersInBothDirections) {
  acceptInitial();
  EXPECT_EQ(state.resolve(501).location, ServerChannelLocation::kLocal);
  EXPECT_EQ(state.resolve(501).channelNumber, 7);
  EXPECT_EQ(state.resolve(502).channelNumber, 3);
  EXPECT_EQ(state.resolve(7).location, ServerChannelLocation::kRemote);
  EXPECT_EQ(state.resolve(0).location, ServerChannelLocation::kUnresolved);
  EXPECT_EQ(state.resolve(UINT32_MAX).location,
            ServerChannelLocation::kUnresolved);
  uint32_t id = 0;
  ASSERT_TRUE(state.reverse(3, &id));
  EXPECT_EQ(id, 502);
  EXPECT_FALSE(state.reverse(1, &id));
  EXPECT_FALSE(state.reverse(3, nullptr));
}
TEST_F(ServerIdentityTests,
       ColdBootRestoresIdentityAndRootWithoutRegistration) {
  acceptInitial();
  auto epoch = state.rootEpoch();
  auto root = config.blob("sl-root");
  config.reboot();
  ServerIdentity restarted;
  restarted.load(&config);
  EXPECT_EQ(restarted.resolve(501).channelNumber, 7);
  EXPECT_EQ(restarted.resolve(999).location, ServerChannelLocation::kRemote);
  EXPECT_EQ(restarted.rootEpoch(), epoch);
  EXPECT_EQ(config.blob("sl-root"), root);
  EXPECT_FALSE(restarted.identityTransition());
  EXPECT_EQ(config.commits, 2);
}
TEST_F(ServerIdentityTests, ConfigurationForgetsAssociationBeforeRenumbering) {
  acceptInitial();
  ASSERT_TRUE(state.forgetChannel(7));
  ASSERT_TRUE(first->setChannelNumber(9));
  EXPECT_TRUE(state.identityAvailable());
  EXPECT_EQ(first->getServerChannelId(), 0u);
  EXPECT_EQ(state.resolve(502).location, ServerChannelLocation::kUnresolved);
  EXPECT_TRUE(state.identityTransition());
  ServerIdentity restarted;
  restarted.load(&config);
  EXPECT_TRUE(restarted.identityAvailable());
  EXPECT_EQ(first->getServerChannelId(), 0u);
  EXPECT_EQ(restarted.resolve(502).location,
            ServerChannelLocation::kUnresolved);
}
TEST_F(ServerIdentityTests, IdentityDoesNotPersistTypeOrSubdevice) {
  acceptInitial();
  first->setType(SUPLA_CHANNELTYPE_RELAY);
  first->setSubDeviceId(1);
  EXPECT_TRUE(state.identityAvailable());
  ServerIdentity restarted;
  restarted.load(&config);
  EXPECT_EQ(restarted.resolve(501).channelNumber, 7);
}
TEST_F(ServerIdentityTests, DifferentRebootOrderRestoresByNumber) {
  acceptInitial();
  state.load(nullptr);  // Power off before rebuilding Channels.
  delete second;
  delete first;
  second = new Supla::Channel(3);
  first = new Supla::Channel(7);
  ServerIdentity restarted;
  restarted.load(&config);
  EXPECT_TRUE(restarted.identityAvailable());
  EXPECT_EQ(restarted.resolve(501).channelNumber, 7);
  EXPECT_EQ(restarted.resolve(502).channelNumber, 3);
  auto reordered = snapshot();
  reordered.ChannelId[0] = 502;
  reordered.ChannelId[1] = 501;
  ASSERT_TRUE(restarted.registrationStarted());
  restarted.registrationSucceeded();
  int before = config.commits;
  EXPECT_EQ(restarted.accept(reordered).Result, SUPLA_SUPLAN_RESULT_OK);
  EXPECT_EQ(config.commits, before);
  EXPECT_FALSE(restarted.identityTransition());
}
TEST_F(ServerIdentityTests,
       CorruptTruncatedUnsupportedAndTrailingRecordsRejected) {
  acceptInitial();
  const auto original = config.blob("sl-identity");
  for (int kind = 0; kind < 4; ++kind) {
    auto bytes = original;
    if (kind == 0) {
      bytes[8] ^= 1;
    }
    if (kind == 1) {
      bytes.pop_back();
    }
    if (kind == 2) {
      bytes[0] = 99;
      seal(&bytes);
    }
    if (kind == 3) {
      bytes.push_back(0);
      seal(&bytes);
    }
    config.install("sl-identity", bytes);
    ServerIdentity restarted;
    restarted.load(&config);
    EXPECT_EQ(restarted.resolve(501).location,
              ServerChannelLocation::kUnresolved)
        << kind;
    EXPECT_NE(restarted.rootEpoch(), 0u);
  }
}
TEST_F(ServerIdentityTests, LogicalCorruptionRejectedEvenWithCorrectChecksum) {
  acceptInitial();
  auto bytes = config.blob("sl-identity");
  // Duplicate ChannelId in an otherwise complete coherent envelope.
  memcpy(bytes.data() + 12, bytes.data() + 7, 4);
  seal(&bytes);
  config.install("sl-identity", bytes);
  ServerIdentity restarted;
  restarted.load(&config);
  EXPECT_FALSE(restarted.identityAvailable());
}
TEST_F(ServerIdentityTests, IdenticalSnapshotDoesNotCommitOrStartTransition) {
  acceptInitial();
  state.syncDone();
  auto durable = config.durable;
  EXPECT_EQ(state.accept(snapshot()).Result, SUPLA_SUPLAN_RESULT_OK);
  EXPECT_EQ(config.commits, 2);
  EXPECT_EQ(config.durable, durable);
  EXPECT_FALSE(state.identityTransition());
  EXPECT_TRUE(state.serverSyncComplete());
}
TEST_F(ServerIdentityTests, ChangedSnapshotIsDurableBeforeActivation) {
  acceptInitial();
  state.syncDone();
  auto changed = snapshot();
  changed.ChannelId[0] = 600;
  EXPECT_EQ(state.accept(changed).Result, SUPLA_SUPLAN_RESULT_OK);
  EXPECT_EQ(first->getServerChannelId(), 600u);
  EXPECT_EQ(state.resolve(501).location, ServerChannelLocation::kUnresolved);
  EXPECT_EQ(config.commits, 3);
  EXPECT_TRUE(state.identityTransition());
  config.reboot();
  ServerIdentity restarted;
  restarted.load(&config);
  EXPECT_EQ(restarted.resolve(600).location,
            ServerChannelLocation::kUnresolved);
}
TEST_F(ServerIdentityTests, InvalidSnapshotsNeverWriteRootOrIdentity) {
  for (int kind = 0; kind < 6; ++kind) {
    auto s = snapshot();
    if (kind == 0) {
      s.ChannelCount = -1;
    }
    if (kind == 1) {
      s.ChannelCount = SUPLA_CHANNELMAXCOUNT + 1;
    }
    if (kind == 2) {
      s.ChannelCount = 1;
    }
    if (kind == 3) {
      s.DeviceId = 0;
    }
    if (kind == 4) {
      s.ChannelId[0] = 0;
    }
    if (kind == 5) {
      s.ChannelId[1] = s.ChannelId[0];
    }
    EXPECT_EQ(state.accept(s).Result, SUPLA_SUPLAN_RESULT_INVALID_ARGUMENT);
  }
  EXPECT_EQ(config.commits, 0);
  EXPECT_TRUE(config.durable.empty());
}
TEST_F(ServerIdentityTests, SnapshotRequiresSuccessfulCurrentRegistration) {
  state.disconnected();
  EXPECT_EQ(state.accept(snapshot()).Result,
            SUPLA_SUPLAN_RESULT_INVALID_ARGUMENT);
  ASSERT_TRUE(state.registrationStarted());
  EXPECT_EQ(state.accept(snapshot()).Result,
            SUPLA_SUPLAN_RESULT_INVALID_ARGUMENT);
  ASSERT_TRUE(first->setChannelNumber(9));
  state.registrationSucceeded();
  EXPECT_EQ(state.accept(snapshot()).Result,
            SUPLA_SUPLAN_RESULT_INVALID_ARGUMENT);
  EXPECT_EQ(config.commits, 0);
}
TEST_F(ServerIdentityTests,
       RootCommitFailureDoesNotInstallRuntimeOrReportSuccess) {
  config.failCommit = 1;
  EXPECT_EQ(state.accept(snapshot()).Result,
            SUPLA_SUPLAN_RESULT_PERSISTENCE_ERROR);
  EXPECT_EQ(state.rootEpoch(), 0u);
  EXPECT_FALSE(state.identityAvailable());
  config.failCommit = -1;
  acceptInitial();
  EXPECT_NE(state.rootEpoch(), 0u);
}
TEST_F(ServerIdentityTests,
       IdentityCommitFailurePreservesRootAndPreviousRuntime) {
  acceptInitial();
  state.syncDone();
  auto epoch = state.rootEpoch();
  auto durable = config.durable;
  auto changed = snapshot();
  changed.DeviceId = 102;
  changed.ChannelId[0] = 600;
  config.failCommit = config.commits + 1;
  auto result = state.accept(changed);
  EXPECT_EQ(result.Result, SUPLA_SUPLAN_RESULT_PERSISTENCE_ERROR);
  EXPECT_EQ(result.RootEpoch, epoch);
  EXPECT_EQ(state.serverDeviceId(), 101);
  EXPECT_EQ(state.resolve(501).channelNumber, 7);
  EXPECT_FALSE(state.identityTransition());
  EXPECT_EQ(config.durable, durable);
  // A later unrelated commit cannot persist the rejected candidate.
  ASSERT_TRUE(config.commit());
  config.reboot();
  ServerIdentity restarted;
  restarted.load(&config);
  EXPECT_EQ(restarted.serverDeviceId(), 101);
}
TEST_F(ServerIdentityTests,
       FirstIdentityFailureRetainsIndependentlyCommittedRoot) {
  config.failCommit = 2;
  auto result = state.accept(snapshot());
  EXPECT_EQ(result.Result, SUPLA_SUPLAN_RESULT_PERSISTENCE_ERROR);
  EXPECT_NE(result.RootEpoch, 0u);
  EXPECT_FALSE(state.identityAvailable());
  config.reboot();
  ServerIdentity restarted;
  restarted.load(&config);
  EXPECT_EQ(restarted.rootEpoch(), result.RootEpoch);
  EXPECT_FALSE(restarted.identityAvailable());
}
TEST_F(ServerIdentityTests,
       ReconnectAndDeviceIdChangeNeverRotateOrRewriteRoot) {
  acceptInitial();
  auto root = config.blob("sl-root");
  auto epoch = state.rootEpoch();
  state.syncDone();
  state.disconnected();
  EXPECT_TRUE(state.identityAvailable());
  registerState();
  EXPECT_EQ(state.accept(snapshot()).Result, SUPLA_SUPLAN_RESULT_OK);
  EXPECT_EQ(config.commits, 2);
  auto changed = snapshot();
  changed.DeviceId = 999;
  EXPECT_EQ(state.accept(changed).Result, SUPLA_SUPLAN_RESULT_OK);
  EXPECT_EQ(config.commits, 3);
  EXPECT_EQ(state.rootEpoch(), epoch);
  EXPECT_EQ(config.blob("sl-root"), root);
}
TEST_F(ServerIdentityTests, InvalidRootEpochAndCorruptRootAreRegenerated) {
  acceptInitial();
  auto original = config.blob("sl-root");
  for (int kind = 0; kind < 5; ++kind) {
    auto bytes = original;
    if (kind < 2) {
      memset(bytes.data() + 1, kind ? 0xFF : 0, 4);
      seal(&bytes);
    }
    if (kind == 2) {
      bytes[8] ^= 1;
    }
    if (kind == 3) {
      bytes.pop_back();
    }
    if (kind == 4) {
      bytes[0] = 99;
      seal(&bytes);
    }
    config.install("sl-root", bytes);
    ServerIdentity restarted;
    restarted.load(&config);
    EXPECT_EQ(restarted.rootEpoch(), 0u);
    ASSERT_TRUE(restarted.registrationStarted());
    restarted.registrationSucceeded();
    auto result = restarted.accept(snapshot());
    ASSERT_EQ(result.Result, SUPLA_SUPLAN_RESULT_OK);
    EXPECT_NE(result.RootEpoch, 0u);
    EXPECT_NE(result.RootEpoch, UINT32_MAX);
    EXPECT_NE(config.blob("sl-root"), original);
  }
}
TEST_F(ServerIdentityTests, TransitionSurvivesDisconnectAndIdenticalReplay) {
  acceptInitial();
  state.syncDone();
  auto changed = snapshot();
  changed.ChannelId[0] = 600;
  ASSERT_EQ(state.accept(changed).Result, SUPLA_SUPLAN_RESULT_OK);
  EXPECT_TRUE(state.identityTransition());
  state.disconnected();
  state.syncDone();
  EXPECT_TRUE(state.identityTransition());
  registerState();
  EXPECT_EQ(state.accept(changed).Result, SUPLA_SUPLAN_RESULT_OK);
  EXPECT_TRUE(state.identityTransition());
  state.syncDone();
  EXPECT_FALSE(state.identityTransition());
}
TEST_F(ServerIdentityTests, FactoryResetRemovesBothRecords) {
  acceptInitial();
  SimpleTime time;
  SuplaDeviceClass device;
  device.resetToFactorySettings();
  EXPECT_EQ(state.rootEpoch(), 0u);
  EXPECT_FALSE(state.identityAvailable());
  config.reboot();
  ServerIdentity restarted;
  restarted.load(&config);
  EXPECT_EQ(restarted.rootEpoch(), 0u);
  EXPECT_FALSE(restarted.identityAvailable());
}

class TestSrpc : public Supla::Protocol::SuplaSrpc {
 public:
  explicit TestSrpc(SuplaDeviceClass *sdc, int version = 29)
      : SuplaSrpc(sdc, version) {}
  void attach(void *handle) { srpc = handle; }
  void initializeForTest() { initializeSrpc(); }
  void deinitializeForTest() { deinitializeSrpc(); }
  ~TestSrpc() { srpc = nullptr; }
};

TEST_F(ServerIdentityTests,
       NormalSrpcCallbackSendsDurableEpochAndReleasesTransition) {
  SimpleTime time;
  NiceMock<SrpcMock> wire;
  SuplaDeviceClass device;
  TestSrpc protocol(&device);
  protocol.restoreServerIdentity();
  protocol.onInit();
  int handle = 0;
  protocol.attach(&handle);
  protocol.serverIdentity().registrationStarted();
  TSD_SuplaRegisterDeviceResult registration = {};
  registration.result_code = SUPLA_RESULTCODE_TRUE;
  registration.activity_timeout = 30;
  protocol.onRegisterResult(&registration);
  auto data = snapshot();
  EXPECT_CALL(wire, srpc_getdata(&handle, _, _))
      .WillOnce([&data](void *, TsrpcReceivedData *rd, unsigned int) {
        rd->call_id = SUPLA_SD_CALL_SUPLAN_DEVICE_IDENTITIES;
        rd->data.sd_suplan_device_identities = &data;
        return SUPLA_RESULT_TRUE;
      });
  EXPECT_CALL(wire, deviceIdentitiesResult(&handle, _))
      .WillOnce(
          [this, &protocol](void *, TDS_SuplaDeviceIdentitiesResult *result) {
            EXPECT_EQ(result->Result, SUPLA_SUPLAN_RESULT_OK);
            EXPECT_EQ(result->RootEpoch, protocol.serverIdentity().rootEpoch());
            EXPECT_NE(result->RootEpoch, 0u);
            EXPECT_EQ(config.commits, 2);
            return 1;
          });
  Supla::messageReceived(&handle, 1, SUPLA_SD_CALL_SUPLAN_DEVICE_IDENTITIES,
                         &protocol, 29);
  EXPECT_FALSE(protocol.serverIdentity().identityTransition());
  protocol.onDeviceSyncDone();
  EXPECT_FALSE(protocol.serverIdentity().identityTransition());
  EXPECT_EQ(Supla::RegisterDevice::getRegDevHeaderPtr()->Flags &
                SUPLA_DEVICE_FLAG_SUPLAN_SUPPORTED,
            0);
}

TEST_F(ServerIdentityTests,
       LittleFsSmallConfigBufferStoresSeparateBoundedRecords) {
  LittleFS.reset();
  Supla::LittleFsConfig fs(64);
  ServerIdentity local;
  local.load(&fs);
  ASSERT_TRUE(local.registrationStarted());
  local.registrationSucceeded();
  ASSERT_EQ(local.accept(snapshot()).Result, SUPLA_SUPLAN_RESULT_OK);
  EXPECT_EQ(LittleFS.open("/supla/sl-root", "r").size(), 39u);
  EXPECT_EQ(static_cast<Supla::Config &>(fs).getBlobSize("sl-identity"), 18);
  ServerIdentity rebooted;
  rebooted.load(&fs);
  EXPECT_EQ(rebooted.resolve(501).channelNumber, 7);
  fs.removeAll();
  rebooted.load(&fs);
  EXPECT_FALSE(rebooted.identityAvailable());
  EXPECT_EQ(rebooted.rootEpoch(), 0u);
  LittleFS.reset();
}

TEST_F(ServerIdentityTests, MaximumTopologyFitsCompactLittleFsRecord) {
  delete first;
  delete second;
  first = second = nullptr;
  std::vector<std::unique_ptr<Supla::Channel>> channels;
  TSD_SuplaDeviceIdentities data = {};
  data.DeviceId = 101;
  data.ChannelCount = SUPLA_CHANNELMAXCOUNT;
  for (int i = 0; i < SUPLA_CHANNELMAXCOUNT; ++i) {
    channels.emplace_back(new Supla::Channel(SUPLA_CHANNELMAXCOUNT - 1 - i));
    data.ChannelId[i] = 1000 + i;
  }
  LittleFS.reset();
  Supla::LittleFsConfig fs(64);
  ServerIdentity local;
  local.load(&fs);
  ASSERT_TRUE(local.registrationStarted());
  local.registrationSucceeded();
  ASSERT_EQ(local.accept(data).Result, SUPLA_SUPLAN_RESULT_OK);
  EXPECT_EQ(LittleFS.open("/supla/sl-identity", "r").size(), 648u);
  ServerIdentity rebooted;
  rebooted.load(&fs);
  EXPECT_EQ(rebooted.resolve(1000).channelNumber, 127);
  EXPECT_EQ(rebooted.resolve(1127).channelNumber, 0);
  LittleFS.reset();
}
TEST_F(ServerIdentityTests, LittleFsShortBlobWriteCannotReturnProvisioningOk) {
  LittleFS.reset();
  LittleFS.maxWrite = 10;
  Supla::LittleFsConfig fs(64);
  ServerIdentity local;
  local.load(&fs);
  ASSERT_TRUE(local.registrationStarted());
  local.registrationSucceeded();
  EXPECT_EQ(local.accept(snapshot()).Result,
            SUPLA_SUPLAN_RESULT_PERSISTENCE_ERROR);
  EXPECT_EQ(local.rootEpoch(), 0u);
  EXPECT_FALSE(local.identityAvailable());
  LittleFS.reset();
}

TEST_F(ServerIdentityTests, LittleFsRootFinalizationFailureCannotReturnOk) {
  for (bool syncFailure : {false, true}) {
    LittleFS.reset();
    Supla::LittleFsConfig fs(64);
    ServerIdentity local;
    local.load(&fs);
    ASSERT_TRUE(local.registrationStarted());
    local.registrationSucceeded();
    if (syncFailure) {
      LittleFS.failSyncPath = "/supla/sl-root";
    } else {
      LittleFS.failClosePath = "/supla/sl-root";
    }
    auto result = local.accept(snapshot());
    EXPECT_EQ(result.Result, SUPLA_SUPLAN_RESULT_PERSISTENCE_ERROR);
    EXPECT_EQ(result.RootEpoch, 0u);
    EXPECT_FALSE(local.identityAvailable());
    LittleFS.failSyncPath.clear();
    LittleFS.failClosePath.clear();
    ASSERT_EQ(local.accept(snapshot()).Result, SUPLA_SUPLAN_RESULT_OK);
    ServerIdentity restarted;
    restarted.load(&fs);
    EXPECT_EQ(restarted.rootEpoch(), local.rootEpoch());
    EXPECT_EQ(restarted.resolve(501).channelNumber, 7);
  }
  LittleFS.reset();
}

TEST_F(ServerIdentityTests, LittleFsConfigFinalizationFailureCannotReturnOk) {
  LittleFS.reset();
  Supla::LittleFsConfig fs(64);
  ServerIdentity local;
  local.load(&fs);
  ASSERT_TRUE(local.registrationStarted());
  local.registrationSucceeded();
  LittleFS.failClosePath = "/supla-dev.cfg";
  EXPECT_EQ(local.accept(snapshot()).Result,
            SUPLA_SUPLAN_RESULT_PERSISTENCE_ERROR);
  EXPECT_NE(local.rootEpoch(), 0u);
  EXPECT_FALSE(local.identityAvailable());
  LittleFS.failClosePath.clear();
  EXPECT_EQ(local.accept(snapshot()).Result, SUPLA_SUPLAN_RESULT_OK);
  LittleFS.reset();
}

TEST_F(ServerIdentityTests,
       LittleFsIdentityFinalizationFailurePreservesAcceptedRuntime) {
  LittleFS.reset();
  std::vector<std::unique_ptr<Supla::Channel>> extra;
  for (int n = 0; n < 3; ++n) {
    extra.emplace_back(new Supla::Channel(n));
  }
  auto initial = snapshot();
  initial.ChannelCount = 5;
  for (int n = 2; n < 5; ++n) {
    initial.ChannelId[n] = 501 + n;
  }
  Supla::LittleFsConfig fs(64);
  ServerIdentity local;
  local.load(&fs);
  ASSERT_TRUE(local.registrationStarted());
  local.registrationSucceeded();
  ASSERT_EQ(local.accept(initial).Result, SUPLA_SUPLAN_RESULT_OK);
  local.syncDone();
  const auto epoch = local.rootEpoch();
  auto changed = initial;
  changed.ChannelId[0] = 600;
  LittleFS.failSyncPath = "/supla/sl-identity";
  EXPECT_EQ(local.accept(changed).Result,
            SUPLA_SUPLAN_RESULT_PERSISTENCE_ERROR);
  EXPECT_EQ(local.rootEpoch(), epoch);
  EXPECT_EQ(local.resolve(501).channelNumber, 7);
  EXPECT_EQ(local.resolve(600).location, ServerChannelLocation::kRemote);
  EXPECT_FALSE(local.identityTransition());
  EXPECT_TRUE(local.serverSyncComplete());
  LittleFS.failSyncPath.clear();
  ASSERT_EQ(local.accept(initial).Result, SUPLA_SUPLAN_RESULT_OK);
  ServerIdentity restarted;
  restarted.load(&fs);
  EXPECT_EQ(restarted.rootEpoch(), epoch);
  EXPECT_EQ(restarted.resolve(501).channelNumber, 7);
  LittleFS.reset();
}

TEST_F(ServerIdentityTests,
       LittleFsFailedRollbackRequiresDurableRepairBeforeIdenticalOk) {
  LittleFS.reset();
  std::vector<std::unique_ptr<Supla::Channel>> extra;
  for (int n = 0; n < 3; ++n) {
    extra.emplace_back(new Supla::Channel(n));
  }
  auto initial = snapshot();
  initial.ChannelCount = 5;
  for (int n = 2; n < 5; ++n) {
    initial.ChannelId[n] = 501 + n;
  }
  Supla::LittleFsConfig fs(64);
  ServerIdentity local;
  local.load(&fs);
  ASSERT_TRUE(local.registrationStarted());
  local.registrationSucceeded();
  ASSERT_EQ(local.accept(initial).Result, SUPLA_SUPLAN_RESULT_OK);
  local.syncDone();
  const auto epoch = local.rootEpoch();
  std::array<uint8_t, 32> root;
  memcpy(root.data(), local.rootKey(), root.size());

  auto changed = initial;
  changed.ChannelId[0] = 600;
  LittleFS.maxWrite = 10;
  EXPECT_EQ(local.accept(changed).Result,
            SUPLA_SUPLAN_RESULT_PERSISTENCE_ERROR);
  EXPECT_EQ(LittleFS.open("/supla/sl-identity", "r").size(), 10u);
  // A replay must fail while storage still cannot repair the previous state.
  EXPECT_EQ(local.accept(initial).Result,
            SUPLA_SUPLAN_RESULT_PERSISTENCE_ERROR);
  EXPECT_EQ(local.rootEpoch(), epoch);
  EXPECT_EQ(memcmp(local.rootKey(), root.data(), root.size()), 0);
  EXPECT_EQ(local.resolve(501).location, ServerChannelLocation::kLocal);
  EXPECT_FALSE(local.identityTransition());
  EXPECT_TRUE(local.serverSyncComplete());

  LittleFS.maxWrite = SIZE_MAX;
  ASSERT_EQ(local.accept(initial).Result, SUPLA_SUPLAN_RESULT_OK);
  EXPECT_FALSE(local.identityTransition());
  EXPECT_TRUE(local.serverSyncComplete());
  ServerIdentity restarted;
  restarted.load(&fs);
  EXPECT_EQ(restarted.resolve(501).location, ServerChannelLocation::kLocal);
  EXPECT_EQ(restarted.resolve(501).channelNumber, 7);
  EXPECT_EQ(restarted.rootEpoch(), epoch);
  ASSERT_NE(restarted.rootKey(), nullptr);
  EXPECT_EQ(memcmp(restarted.rootKey(), root.data(), root.size()), 0);
  EXPECT_EQ(LittleFS.open("/supla/sl-root", "r").size(), 39u);
  EXPECT_EQ(LittleFS.open("/supla/sl-identity", "r").size(), 33u);
  // Once durability is confirmed again, idempotent replay performs no writes.
  LittleFS.maxWrite = 0;
  EXPECT_EQ(local.accept(initial).Result, SUPLA_SUPLAN_RESULT_OK);
  EXPECT_EQ(LittleFS.open("/supla/sl-identity", "r").size(), 33u);
  LittleFS.reset();
}

TEST_F(ServerIdentityTests,
       FailedSharedConfigCommitRepairsRootAndIdentityWithoutRotation) {
  acceptInitial();
  const auto epoch = state.rootEpoch();
  const auto root = config.blob("sl-root");
  auto changed = snapshot();
  changed.ChannelId[0] = 600;
  config.loseDurableOnFailure = true;
  config.failCommit = config.commits + 1;
  EXPECT_EQ(state.accept(changed).Result,
            SUPLA_SUPLAN_RESULT_PERSISTENCE_ERROR);
  EXPECT_TRUE(config.durable.empty());
  config.failCommit = config.commits + 1;
  EXPECT_EQ(state.accept(snapshot()).Result,
            SUPLA_SUPLAN_RESULT_PERSISTENCE_ERROR);
  EXPECT_EQ(state.rootEpoch(), epoch);
  EXPECT_EQ(state.resolve(501).location, ServerChannelLocation::kLocal);
  EXPECT_FALSE(state.identityTransition());

  config.failCommit = -1;
  ASSERT_EQ(state.accept(snapshot()).Result, SUPLA_SUPLAN_RESULT_OK);
  EXPECT_EQ(config.commits, 6);
  EXPECT_EQ(config.blob("sl-root"), root);
  EXPECT_FALSE(state.identityTransition());
  EXPECT_FALSE(state.serverSyncComplete());
  ASSERT_EQ(state.accept(snapshot()).Result, SUPLA_SUPLAN_RESULT_OK);
  EXPECT_EQ(config.commits, 6);
  config.reboot();
  ServerIdentity restarted;
  restarted.load(&config);
  EXPECT_EQ(restarted.rootEpoch(), epoch);
  EXPECT_EQ(restarted.resolve(501).channelNumber, 7);
  EXPECT_EQ(config.blob("sl-root"), root);
}

TEST_F(ServerIdentityTests,
       NormalRegistrationGatesIdentityAndReportsCommitFailure) {
  Supla::RegisterDevice::setServerName("supla.example");
  Supla::RegisterDevice::setEmail("user@example.com");
  SimpleTime time;
  NiceMock<SrpcMock> wire;
  SuplaDeviceClass device;
  TestSrpc protocol(&device);
  protocol.restoreServerIdentity();
  protocol.onInit();
  int handle = 0;
  auto *client = new NiceMock<NetworkClientMock>;
  EXPECT_CALL(wire, srpc_init(_)).WillOnce(Return(&handle));
  EXPECT_CALL(*client, connected()).WillRepeatedly(Return(1));
  EXPECT_CALL(wire, srpc_iterate(_)).WillRepeatedly(Return(SUPLA_RESULT_TRUE));
  EXPECT_CALL(wire, srpc_ds_async_registerdevice_in_chunks_g(_, _))
      .WillOnce([](void *, TDS_SuplaRegisterDeviceHeader *header) {
        EXPECT_EQ(header->channel_count, 2);
        EXPECT_NE(header->Flags & SUPLA_DEVICE_FLAG_SUPLAN_SUPPORTED, 0);
        return 1;
      });
  protocol.setNetworkClient(client);
  protocol.initializeForTest();
  EXPECT_FALSE(protocol.iterate(0));
  auto data = snapshot();
  EXPECT_CALL(wire, deviceIdentitiesResult(_, _))
      .WillOnce([](void *, TDS_SuplaDeviceIdentitiesResult *result) {
        EXPECT_EQ(result->Result, SUPLA_SUPLAN_RESULT_INVALID_ARGUMENT);
        return 1;
      });
  protocol.onDeviceIdentities(&data);
  EXPECT_EQ(config.commits, 0);
  TSD_SuplaRegisterDeviceResult registration = {};
  registration.result_code = SUPLA_RESULTCODE_TRUE;
  registration.activity_timeout = 30;
  protocol.onRegisterResult(&registration);
  config.failCommit = 1;
  EXPECT_CALL(wire, deviceIdentitiesResult(_, _))
      .WillOnce([](void *, TDS_SuplaDeviceIdentitiesResult *result) {
        EXPECT_EQ(result->Result, SUPLA_SUPLAN_RESULT_PERSISTENCE_ERROR);
        EXPECT_EQ(result->RootEpoch, 0u);
        return 1;
      });
  protocol.onDeviceIdentities(&data);
  EXPECT_FALSE(protocol.serverIdentity().identityAvailable());
  EXPECT_CALL(wire, deviceIdentitiesResult(_, _))
      .WillOnce([&protocol](void *, TDS_SuplaDeviceIdentitiesResult *result) {
        EXPECT_EQ(result->Result, SUPLA_SUPLAN_RESULT_OK);
        EXPECT_EQ(result->RootEpoch, protocol.serverIdentity().rootEpoch());
        return 1;
      });
  protocol.onDeviceIdentities(&data);
  EXPECT_EQ(protocol.serverIdentity().resolve(501).channelNumber, 7);
  protocol.deinitializeForTest();
  EXPECT_TRUE(protocol.serverIdentity().identityAvailable());
  EXPECT_FALSE(protocol.serverIdentity().identityTransition());
}
TEST_F(ServerIdentityTests,
       ColdBootThenIdenticalReplayDoesNotWriteRootOrIdentity) {
  acceptInitial();
  config.reboot();
  ServerIdentity restarted;
  restarted.load(&config);
  ASSERT_TRUE(restarted.registrationStarted());
  restarted.registrationSucceeded();
  ASSERT_EQ(restarted.accept(snapshot()).Result, SUPLA_SUPLAN_RESULT_OK);
  EXPECT_EQ(config.commits, 2);
  EXPECT_FALSE(restarted.identityTransition());
}
TEST_F(ServerIdentityTests, AppendPreservesMapAndUsesRegisteredPrefix) {
  acceptInitial();
  state.syncDone();
  int before = config.commits;
  auto extra = std::unique_ptr<Supla::Channel>(new Supla::Channel(4));
  EXPECT_TRUE(state.identityAvailable());
  EXPECT_FALSE(state.identityTransition());
  EXPECT_EQ(state.resolve(501).channelNumber, 7);
  EXPECT_EQ(state.resolve(800).location, ServerChannelLocation::kRemote);
  uint32_t id = 0;
  EXPECT_FALSE(state.reverse(4, &id));
  EXPECT_EQ(state.accept(snapshot()).Result, SUPLA_SUPLAN_RESULT_OK);
  EXPECT_EQ(config.commits, before);
  EXPECT_EQ(extra->getServerChannelId(), 0u);
  registerState();
  auto extended = snapshot();
  extended.ChannelCount = 3;
  extended.ChannelId[2] = 800;
  EXPECT_EQ(state.accept(extended).Result, SUPLA_SUPLAN_RESULT_OK);
  EXPECT_EQ(extra->getServerChannelId(), 800u);
  EXPECT_EQ(state.resolve(800).channelNumber, 4);
  EXPECT_FALSE(state.identityTransition());
  EXPECT_EQ(config.commits, before + 1);
  EXPECT_EQ(config.blob("sl-identity").size(), 23u);
}
TEST_F(ServerIdentityTests, ColdBootWithAdditionalChannelKeepsExistingMap) {
  acceptInitial();
  auto extra = std::unique_ptr<Supla::Channel>(new Supla::Channel(4));
  ServerIdentity restarted;
  restarted.load(&config);
  EXPECT_EQ(restarted.resolve(501).channelNumber, 7);
  EXPECT_EQ(restarted.resolve(502).channelNumber, 3);
  EXPECT_EQ(extra->getServerChannelId(), 0u);
  EXPECT_EQ(restarted.resolve(800).location, ServerChannelLocation::kRemote);
}
TEST_F(ServerIdentityTests, AppendBeforeFirstIdentitiesUsesCapturedCount) {
  auto extra = std::unique_ptr<Supla::Channel>(new Supla::Channel(4));
  EXPECT_EQ(state.accept(snapshot()).Result, SUPLA_SUPLAN_RESULT_OK);
  EXPECT_EQ(first->getServerChannelId(), 501u);
  EXPECT_EQ(second->getServerChannelId(), 502u);
  EXPECT_EQ(extra->getServerChannelId(), 0u);
}
TEST_F(ServerIdentityTests, RemovalInvalidatesRegistrationContext) {
  delete second;
  second = nullptr;
  EXPECT_TRUE(state.registrationInvalidated());
  EXPECT_EQ(state.accept(snapshot()).Result,
            SUPLA_SUPLAN_RESULT_INVALID_ARGUMENT);
  EXPECT_EQ(config.commits, 0);
}
TEST_F(ServerIdentityTests, RenumberInvalidatesRegistrationContext) {
  EXPECT_TRUE(first->setChannelNumber(9));
  EXPECT_TRUE(state.registrationInvalidated());
  EXPECT_EQ(state.accept(snapshot()).Result,
            SUPLA_SUPLAN_RESULT_INVALID_ARGUMENT);
  EXPECT_EQ(config.commits, 0);
}
TEST_F(ServerIdentityTests, ForgetFailurePreservesAssociation) {
  acceptInitial();
  state.syncDone();
  config.failCommit = config.commits + 1;
  EXPECT_FALSE(state.forgetChannel(7));
  EXPECT_EQ(first->getServerChannelId(), 501u);
  EXPECT_FALSE(state.identityTransition());
}
TEST_F(ServerIdentityTests, DuplicateNumbersRejectedWithValidChecksum) {
  acceptInitial();
  auto bytes = config.blob("sl-identity");
  bytes[11] = bytes[6];
  seal(&bytes);
  config.install("sl-identity", bytes);
  ServerIdentity restarted;
  restarted.load(&config);
  EXPECT_FALSE(restarted.identityAvailable());
}
TEST_F(ServerIdentityTests, ValidEmptyMapDiffersFromMissingRecord) {
  delete second;
  delete first;
  second = first = nullptr;
  registerState();
  auto empty = snapshot();
  empty.ChannelCount = 0;
  EXPECT_EQ(state.accept(empty).Result, SUPLA_SUPLAN_RESULT_OK);
  EXPECT_EQ(state.resolve(501).location, ServerChannelLocation::kRemote);
  EXPECT_EQ(config.blob("sl-identity").size(), 8u);
  ServerIdentity restarted;
  restarted.load(&config);
  EXPECT_EQ(restarted.resolve(501).location, ServerChannelLocation::kRemote);
}
TEST_F(ServerIdentityTests, FailedExtensionKeepsKnownIdsAndNewChannelUnmapped) {
  acceptInitial();
  state.syncDone();
  auto extra = std::unique_ptr<Supla::Channel>(new Supla::Channel(4));
  registerState();
  auto extended = snapshot();
  extended.ChannelCount = 3;
  extended.ChannelId[2] = 800;
  auto durable = config.durable;
  config.failCommit = config.commits + 1;
  EXPECT_EQ(state.accept(extended).Result,
            SUPLA_SUPLAN_RESULT_PERSISTENCE_ERROR);
  EXPECT_EQ(config.durable, durable);
  EXPECT_EQ(first->getServerChannelId(), 501u);
  EXPECT_EQ(second->getServerChannelId(), 502u);
  EXPECT_EQ(extra->getServerChannelId(), 0u);
  EXPECT_FALSE(state.identityTransition());
}
TEST_F(ServerIdentityTests, IdentityRecordHasCanonicalExplicitPairs) {
  acceptInitial();
  auto record = config.blob("sl-identity");
  ASSERT_EQ(record.size(), 18u);
  EXPECT_EQ(record[0], 2);
  EXPECT_EQ(record[1], 2);
  EXPECT_EQ(record[2], 101);
  EXPECT_EQ(record[6], 3);
  EXPECT_EQ(record[7], 502 & 255);
  EXPECT_EQ(record[8], 502 >> 8);
  EXPECT_EQ(record[11], 7);
  EXPECT_EQ(record[12], 501 & 255);
}
TEST_F(ServerIdentityTests, DeviceIdChangeRequiresBarrierWithoutRotatingRoot) {
  acceptInitial();
  state.syncDone();
  auto root = config.blob("sl-root");
  auto changed = snapshot();
  changed.DeviceId = 102;
  EXPECT_EQ(state.accept(changed).Result, SUPLA_SUPLAN_RESULT_OK);
  EXPECT_TRUE(state.identityTransition());
  EXPECT_EQ(config.blob("sl-root"), root);
  state.syncDone();
  EXPECT_FALSE(state.identityTransition());
}
TEST_F(ServerIdentityTests, RemovedAndRecreatedChannelInvalidatesResponse) {
  delete second;
  second = new Supla::Channel(3);
  EXPECT_TRUE(state.registrationInvalidated());
  EXPECT_EQ(state.accept(snapshot()).Result,
            SUPLA_SUPLAN_RESULT_INVALID_ARGUMENT);
  EXPECT_EQ(second->getServerChannelId(), 0u);
}
TEST_F(ServerIdentityTests, NormalSrpcAbortsRegistrationAfterRemoval) {
  Supla::RegisterDevice::setServerName("supla.example");
  Supla::RegisterDevice::setEmail("user@example.com");
  SimpleTime time;
  NiceMock<SrpcMock> wire;
  SuplaDeviceClass device;
  TestSrpc protocol(&device);
  protocol.restoreServerIdentity();
  protocol.onInit();
  int handle = 0;
  auto *client = new NiceMock<NetworkClientMock>;
  EXPECT_CALL(wire, srpc_init(_)).WillOnce(Return(&handle));
  EXPECT_CALL(*client, connected()).WillRepeatedly(Return(1));
  EXPECT_CALL(wire, srpc_iterate(_)).WillRepeatedly(Return(SUPLA_RESULT_TRUE));
  EXPECT_CALL(wire, srpc_ds_async_registerdevice_in_chunks_g(_, _))
      .WillOnce(Return(1));
  protocol.setNetworkClient(client);
  protocol.initializeForTest();
  EXPECT_FALSE(protocol.iterate(0));
  delete second;
  second = nullptr;
  EXPECT_TRUE(protocol.serverIdentity().registrationInvalidated());
  EXPECT_CALL(wire, srpc_free(&handle)).Times(1);
  EXPECT_FALSE(protocol.iterate(1000));
  EXPECT_FALSE(protocol.serverIdentity().registrationContextValid());
}
TEST_F(ServerIdentityTests,
       MissingStoredChannelRequiresSynchronizationBeforeResolution) {
  acceptInitial();
  delete second;
  second = nullptr;
  ServerIdentity restarted;
  restarted.load(&config);
  EXPECT_TRUE(restarted.identityAvailable());
  EXPECT_EQ(restarted.resolve(501).location,
            ServerChannelLocation::kUnresolved);
  EXPECT_EQ(restarted.resolve(502).location,
            ServerChannelLocation::kUnresolved);
  EXPECT_TRUE(restarted.identityTransition());
  ASSERT_TRUE(restarted.registrationStarted());
  restarted.registrationSucceeded();
  auto current = snapshot();
  current.ChannelCount = 1;
  EXPECT_EQ(restarted.accept(current).Result, SUPLA_SUPLAN_RESULT_OK);
  EXPECT_EQ(config.blob("sl-identity").size(), 13u);
  EXPECT_TRUE(restarted.identityTransition());
  restarted.syncDone();
  EXPECT_FALSE(restarted.identityTransition());
}
TEST_F(ServerIdentityTests, RandomFailureNeverCreatesOrAcknowledgesRoot) {
  ScopedCryptoTestState rng(17);
  rng.failRandom = true;
  EXPECT_EQ(state.accept(snapshot()).Result,
            SUPLA_SUPLAN_RESULT_PERSISTENCE_ERROR);
  EXPECT_EQ(state.rootEpoch(), 0u);
  EXPECT_EQ(state.rootKey(), nullptr);
  EXPECT_EQ(config.commits, 0);
}
TEST_F(ServerIdentityTests, RemovedMappedIdentityNeverBecomesRemoteBeforeSync) {
  acceptInitial();
  state.syncDone();
  delete second;
  second = new Supla::Channel(3);
  EXPECT_EQ(second->getServerChannelId(), 0u);
  EXPECT_EQ(state.resolve(502).location, ServerChannelLocation::kUnresolved);
  EXPECT_TRUE(state.identityTransition());
  config.reboot();
  ServerIdentity restarted;
  restarted.load(&config);
  EXPECT_EQ(second->getServerChannelId(), 0u);
  EXPECT_EQ(restarted.resolve(502).location,
            ServerChannelLocation::kUnresolved);
  ASSERT_TRUE(restarted.registrationStarted());
  restarted.registrationSucceeded();
  restarted.syncDone();  // Sync alone cannot open a locally changed map.
  EXPECT_TRUE(restarted.identityTransition());
  auto current = snapshot();
  current.ChannelId[1] = 700;
  ASSERT_EQ(restarted.accept(current).Result, SUPLA_SUPLAN_RESULT_OK);
  restarted.disconnected();
  EXPECT_EQ(restarted.resolve(502).location,
            ServerChannelLocation::kUnresolved);
  ASSERT_TRUE(restarted.registrationStarted());
  restarted.registrationSucceeded();
  restarted.syncDone();  // Even after reconnect, identities must be accepted.
  EXPECT_TRUE(restarted.identityTransition());
  ASSERT_EQ(restarted.accept(current).Result, SUPLA_SUPLAN_RESULT_OK);
  int before = config.commits;
  restarted.syncDone();
  EXPECT_FALSE(restarted.identityTransition());
  EXPECT_EQ(restarted.resolve(502).location, ServerChannelLocation::kRemote);
  EXPECT_EQ(restarted.resolve(700).channelNumber, 3);
  EXPECT_EQ(config.commits, before + 1);
  config.reboot();
  ServerIdentity offline;
  offline.load(&config);
  EXPECT_FALSE(offline.identityTransition());
  EXPECT_EQ(offline.resolve(700).channelNumber, 3);
}
TEST_F(ServerIdentityTests, RenumberPersistenceFailureRefusesReassignment) {
  acceptInitial();
  state.syncDone();
  config.failCommit = config.commits + 1;
  EXPECT_FALSE(first->setChannelNumber(9));
  EXPECT_EQ(first->getChannelNumber(), 7);
  EXPECT_EQ(first->getServerChannelId(), 501u);
  EXPECT_FALSE(state.identityTransition());
}
TEST_F(ServerIdentityTests, RenumberDurablyForgetsAssociationAndStartsBarrier) {
  acceptInitial();
  state.syncDone();
  ASSERT_TRUE(first->setChannelNumber(9));
  EXPECT_EQ(first->getServerChannelId(), 0u);
  EXPECT_EQ(state.resolve(501).location, ServerChannelLocation::kUnresolved);
  config.reboot();
  ServerIdentity restarted;
  restarted.load(&config);
  EXPECT_TRUE(restarted.identityTransition());
  EXPECT_EQ(first->getServerChannelId(), 0u);
}
TEST_F(ServerIdentityTests, FailedSyncMarkerWriteKeepsBarrierClosed) {
  acceptInitial();
  state.syncDone();
  auto changed = snapshot();
  changed.DeviceId = 102;
  ASSERT_EQ(state.accept(changed).Result, SUPLA_SUPLAN_RESULT_OK);
  config.failCommit = config.commits + 1;
  state.syncDone();
  EXPECT_TRUE(state.identityTransition());
  EXPECT_FALSE(state.serverSyncComplete());
  EXPECT_EQ(state.resolve(501).location, ServerChannelLocation::kUnresolved);
  config.reboot();
  ServerIdentity restarted;
  restarted.load(&config);
  EXPECT_TRUE(restarted.identityTransition());
}
TEST_F(ServerIdentityTests,
       RemovingPostRegistrationUnmappedChannelKeepsContext) {
  auto extra = new Supla::Channel(4);
  delete extra;
  EXPECT_TRUE(state.registrationContextValid());
  acceptInitial();
  EXPECT_EQ(state.resolve(501).channelNumber, 7);
}
TEST_F(ServerIdentityTests, SupportedExplicitPairRecordRestoresWithoutWrite) {
  acceptInitial();
  auto record = config.blob("sl-identity");
  record[0] = 1;
  seal(&record);
  config.install("sl-identity", record);
  config.reboot();
  ServerIdentity restarted;
  restarted.load(&config);
  ASSERT_TRUE(restarted.registrationStarted());
  restarted.registrationSucceeded();
  int before = config.commits;
  EXPECT_EQ(restarted.accept(snapshot()).Result, SUPLA_SUPLAN_RESULT_OK);
  EXPECT_EQ(restarted.resolve(501).channelNumber, 7);
  EXPECT_EQ(config.commits, before);
}
class ReorderableChannel : public Supla::Channel {
 public:
  explicit ReorderableChannel(int number) : Channel(number) {}
  static void reversePair(ReorderableChannel *a, ReorderableChannel *b) {
    firstPtr = b;
    b->nextPtr = a;
    a->nextPtr = nullptr;
  }
};
TEST_F(ServerIdentityTests,
       ResponseUsesTransmittedSequenceAfterRuntimeReorder) {
  delete second;
  delete first;
  auto a = new ReorderableChannel(7);
  auto b = new ReorderableChannel(3);
  first = a;
  second = b;
  registerState();
  ReorderableChannel::reversePair(a, b);
  ASSERT_NE(ServerIdentity::registrationChannel_E(0), nullptr);
  EXPECT_EQ(ServerIdentity::registrationChannel_E(0)->Number, 7);
  EXPECT_EQ(ServerIdentity::registrationChannel_E(1)->Number, 3);
  EXPECT_EQ(ServerIdentity::registrationChannel_D(0)->Number, 7);
  EXPECT_EQ(ServerIdentity::registrationChannel_D(1)->Number, 3);
  EXPECT_EQ(ServerIdentity::registrationChannel_E(2), nullptr);
  EXPECT_TRUE(state.registrationContextValid());
  acceptInitial();
  EXPECT_EQ(state.resolve(501).channelNumber, 7);
  EXPECT_EQ(state.resolve(502).channelNumber, 3);
  state.syncDone();
  registerState();
  auto reordered = snapshot();
  reordered.ChannelId[0] = 502;
  reordered.ChannelId[1] = 501;
  int before = config.commits;
  EXPECT_EQ(state.accept(reordered).Result, SUPLA_SUPLAN_RESULT_OK);
  EXPECT_EQ(config.commits, before);
  EXPECT_FALSE(state.identityTransition());
}
TEST_F(ServerIdentityTests,
       ExplicitSecurityRotationChangesPairOnlyAfterCommit) {
  acceptInitial();
  auto root = config.blob("sl-root");
  auto identity = config.blob("sl-identity");
  auto epoch = state.rootEpoch();
  config.failCommit = config.commits + 1;
  EXPECT_FALSE(state.rotateRoot());
  EXPECT_EQ(state.rootEpoch(), epoch);
  EXPECT_EQ(config.blob("sl-root"), root);
  config.failCommit = -1;
  ASSERT_TRUE(state.rotateRoot());
  EXPECT_NE(state.rootEpoch(), epoch);
  EXPECT_NE(config.blob("sl-root"), root);
  EXPECT_EQ(config.blob("sl-identity"), identity);
  EXPECT_EQ(state.resolve(501).channelNumber, 7);
  EXPECT_FALSE(state.registrationContextValid());
  EXPECT_FALSE(state.serverSyncComplete());
  config.reboot();
  ServerIdentity restarted;
  restarted.load(&config);
  EXPECT_EQ(restarted.rootEpoch(), state.rootEpoch());
}
TEST_F(ServerIdentityTests, RemovalWriteFailureForgetsMapBeforeNumberReuse) {
  acceptInitial();
  state.syncDone();
  config.failCommit = config.commits + 1;
  delete second;
  second = new Supla::Channel(3);
  EXPECT_EQ(state.resolve(502).location, ServerChannelLocation::kUnresolved);
  EXPECT_FALSE(state.identityAvailable());
  EXPECT_EQ(second->getServerChannelId(), 0u);
  config.reboot();
  ServerIdentity restarted;
  restarted.load(&config);
  EXPECT_TRUE(restarted.identityAvailable());
  EXPECT_TRUE(restarted.identityTransition());
  EXPECT_EQ(restarted.resolve(502).location,
            ServerChannelLocation::kUnresolved);
  EXPECT_EQ(second->getServerChannelId(), 0u);
  ASSERT_TRUE(restarted.registrationStarted());
  restarted.registrationSucceeded();
  auto replacement = snapshot();
  replacement.ChannelId[1] = 700;
  ASSERT_EQ(restarted.accept(replacement).Result, SUPLA_SUPLAN_RESULT_OK);
  EXPECT_EQ(restarted.resolve(502).location,
            ServerChannelLocation::kUnresolved);
  restarted.syncDone();
  EXPECT_EQ(restarted.resolve(700).channelNumber, 3);
}
TEST_F(ServerIdentityTests, UniformRootKeysAreRejectedEvenWithValidChecksum) {
  acceptInitial();
  auto original = config.blob("sl-root");
  for (uint8_t value : {0, 255}) {
    auto bytes = original;
    memset(bytes.data() + 5, value, 32);
    seal(&bytes);
    config.install("sl-root", bytes);
    ServerIdentity restarted;
    restarted.load(&config);
    EXPECT_EQ(restarted.rootKey(), nullptr);
    EXPECT_EQ(restarted.rootEpoch(), 0u);
    ASSERT_TRUE(restarted.registrationStarted());
    restarted.registrationSucceeded();
    EXPECT_EQ(restarted.accept(snapshot()).Result, SUPLA_SUPLAN_RESULT_OK);
    EXPECT_NE(config.blob("sl-root"), bytes);
  }
}
TEST_F(ServerIdentityTests, OlderProtocolCannotAdvertiseOrAcceptIdentities) {
  Supla::RegisterDevice::setServerName("supla.example");
  Supla::RegisterDevice::setEmail("user@example.com");
  Supla::RegisterDevice::addFlags(SUPLA_DEVICE_FLAG_SUPLAN_SUPPORTED);
  SimpleTime time;
  NiceMock<SrpcMock> wire;
  SuplaDeviceClass device;
  TestSrpc protocol(&device, 28);
  protocol.restoreServerIdentity();
  protocol.onInit();
  int handle = 0;
  auto *client = new NiceMock<NetworkClientMock>;
  EXPECT_CALL(wire, srpc_init(_)).WillOnce(Return(&handle));
  EXPECT_CALL(*client, connected()).WillRepeatedly(Return(1));
  EXPECT_CALL(wire, srpc_iterate(_)).WillRepeatedly(Return(SUPLA_RESULT_TRUE));
  EXPECT_CALL(wire, srpc_ds_async_registerdevice_in_chunks_g(_, _))
      .WillOnce([](void *, TDS_SuplaRegisterDeviceHeader *header) {
        EXPECT_EQ(header->Flags & SUPLA_DEVICE_FLAG_SUPLAN_SUPPORTED, 0);
        return 1;
      });
  protocol.setNetworkClient(client);
  protocol.initializeForTest();
  EXPECT_FALSE(protocol.iterate(0));
  auto data = snapshot();
  EXPECT_CALL(wire, deviceIdentitiesResult(_, _))
      .WillOnce([](void *, TDS_SuplaDeviceIdentitiesResult *result) {
        EXPECT_EQ(result->Result, SUPLA_SUPLAN_RESULT_UNSUPPORTED);
        return 1;
      });
  protocol.onDeviceIdentities(&data);
  EXPECT_EQ(config.commits, 0);
  ServerIdentity unavailable;
  EXPECT_FALSE(unavailable.capable());
}
TEST_F(ServerIdentityTests, ExplicitSrpcRootRotationForcesNewBootstrap) {
  SimpleTime time;
  NiceMock<SrpcMock> wire;
  SuplaDeviceClass device;
  TestSrpc protocol(&device);
  protocol.restoreServerIdentity();
  protocol.onInit();
  int handle = 0;
  protocol.attach(&handle);
  ASSERT_TRUE(protocol.serverIdentity().registrationStarted());
  protocol.serverIdentity().registrationSucceeded();
  ASSERT_EQ(protocol.serverIdentity().accept(snapshot()).Result,
            SUPLA_SUPLAN_RESULT_OK);
  auto epoch = protocol.serverIdentity().rootEpoch();
  EXPECT_CALL(wire, srpc_free(&handle)).Times(1);
  ASSERT_TRUE(protocol.rotateServerRoot());
  EXPECT_NE(protocol.serverIdentity().rootEpoch(), epoch);
  EXPECT_FALSE(protocol.serverIdentity().registrationContextValid());
  EXPECT_FALSE(protocol.isRegisteredAndReady());
}
}  // namespace

// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later
#include <gtest/gtest.h>
#include <simple_time.h>
#include <crypto_test_hooks.h>
#include <supla/channels/channel.h>
#include <supla/crc16.h>
#include <supla/channels/channel_handle.h>
#include <supla/control/hvac_config.h>
#include <supla/control/valve_base.h>
#include <supla/sensor/container.h>
#include <supla/storage/key_value.h>
#include <supla/storage/storage.h>
#include <supla/suplan/suplan_server_identity.h>

#include <array>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace {
using namespace Supla;  // NOLINT(build/namespaces)
using namespace Supla::Device;  // NOLINT(build/namespaces)
class MigrationConfig : public KeyValue {
 public:
  using KeyValue::getBlobSize;
  bool init() override { return true; }
  bool commit() override {
    ++commits;
    if (commits == failCommit) return false;
    std::array<uint8_t, 16384> bytes = {};
    const auto size = serializeToMemory(bytes.data(), bytes.size());
    if (size == SIZE_MAX) return false;
    disk.assign(bytes.begin(), bytes.begin() + size);
    return true;
  }
  bool setBlob(const char *key, const char *value, size_t size) override {
    return !failWrite && KeyValue::setBlob(key, value, size);
  }
  bool eraseKey(const char *key) override {
    return !failErase && KeyValue::eraseKey(key);
  }
  void reboot() {
    removeAllMemory();
    ASSERT_TRUE(initFromMemory(disk.data(), disk.size()));
  }
  template <class T> void put(const char *key, const T &value) {
    ASSERT_TRUE(setBlob(key, reinterpret_cast<const char *>(&value),
                        sizeof(value)));
  }
  template <class T> T get(const char *key) {
    T value;
    EXPECT_EQ(getBlobSize(key), sizeof(value));
    EXPECT_TRUE(getBlob(key, reinterpret_cast<char *>(&value), sizeof(value)));
    return value;
  }
  int commits = 0;
  int failCommit = -1;
  bool failWrite = false;
  bool failErase = false;
  std::vector<uint8_t> disk;
};
class ServerMigrationTests : public testing::Test {
 protected:
  void SetUp() override {
    Channel::resetToDefaults();
    for (int n = 0; n < 8; ++n) channels.emplace_back(new Channel(n));
    identity.load(&config);
    initial.DeviceId = 101;
    initial.ChannelCount = 8;
    for (int i = 0; i < 8; ++i) initial.ChannelId[i] = 501 + i;
    ASSERT_EQ(accept(initial), SUPLA_SUPLAN_RESULT_OK);
    identity.syncDone();
    ASSERT_TRUE(identity.serverSyncComplete());
  }
  void TearDown() override {
    identity.load(nullptr);
    channels.clear();
    Channel::resetToDefaults();
  }
  uint8_t accept(const TSD_SuplaDeviceIdentities &snapshot) {
    EXPECT_TRUE(identity.registrationStarted());
    identity.registrationSucceeded();
    return identity.accept(snapshot).Result;
  }
  TSD_SuplaDeviceIdentities next() {
    auto result = initial;
    result.DeviceId = 202;
    for (int i = 0; i < 8; ++i) result.ChannelId[i] = 701 + i;
    // Former FOREIGN ChannelId collides with a new OWN ChannelId.
    result.ChannelId[7] = 901;
    return result;
  }
  void install() {
    Control::HvacStoredConfigV2 hvac;
    hvac.function = SUPLA_CHANNELFNC_HVAC_THERMOSTAT;
    hvac.referenceNamespace =
        Control::HvacReferenceNamespace::SERVER_CHANNEL_ID;
    hvac.scalars.MinOnTimeS = 123;
    for (int i = 0; i < 6; ++i)
      hvac.references[i].channelId = i % 2 ? 901 : 501 + i;
    hvac.serverNone = {identity.rootEpoch(), 101, 501};
    config.put("1_hvac_cfg2", hvac);
    Sensor::ContainerStoredConfigV2 container;
    container.config.alarmAboveLevel = 98;
    container.config.reserved[31] = 19;
    for (int i = 0; i < 10; ++i) {
      container.config.sensorData[i].fillLevel = i * 10;
      container.config.sensorData[i].source.assign(i % 2
          ? ChannelReference::server(901) : ChannelReference::server(501));
    }
    config.put("2_cnt_cfg2", container);
    Control::ValveStoredConfigV2 valve;
    valve.config.closeValveOnFloodType = 1;
    valve.config.reserved[30] = 17;
    for (int i = 0; i < 20; ++i)
      valve.config.sensorData[i].assign(i % 2
          ? ChannelReference::server(901) : ChannelReference::server(501));
    config.put("3_valve_cfg2", valve);
    ASSERT_TRUE(config.commit());
  }
  void sanitized() {
    const auto hvac = config.get<Control::HvacStoredConfigV2>("1_hvac_cfg2");
    EXPECT_EQ(hvac.referenceNamespace,
              Control::HvacReferenceNamespace::LOCAL_CHANNEL_NUMBER);
    EXPECT_EQ(hvac.function, SUPLA_CHANNELFNC_HVAC_THERMOSTAT);
    EXPECT_EQ(hvac.scalars.MinOnTimeS, 123);
    for (int i = 0; i < 6; ++i) {
      EXPECT_EQ(hvac.references[i].local.isSet, i % 2 ? 0 : 1);
      EXPECT_EQ(hvac.references[i].local.channelNo, i % 2 ? 0 : i);
    }
    EXPECT_EQ(hvac.serverNone.rootEpoch, 0u);
    const auto container =
        config.get<Sensor::ContainerStoredConfigV2>("2_cnt_cfg2");
    EXPECT_EQ(container.config.alarmAboveLevel, 98);
    EXPECT_EQ(container.config.reserved[31], 19);
    for (int i = 0; i < 10; ++i) {
      EXPECT_EQ(container.config.sensorData[i].fillLevel, i * 10);
      EXPECT_EQ(container.config.sensorData[i].source.reference(), i % 2
          ? ChannelReference() : ChannelReference::local(0));
    }
    const auto valve = config.get<Control::ValveStoredConfigV2>("3_valve_cfg2");
    EXPECT_EQ(valve.config.closeValveOnFloodType, 1);
    EXPECT_EQ(valve.config.reserved[30], 17);
    for (int i = 0; i < 20; ++i)
      EXPECT_EQ(valve.config.sensorData[i].reference(), i % 2
          ? ChannelReference() : ChannelReference::local(0));
  }
  SimpleTime time;
  ScopedCryptoTestState crypto{17};
  MigrationConfig config;
  ServerIdentity identity;
  TSD_SuplaDeviceIdentities initial = {};
  std::vector<std::unique_ptr<Channel>> channels;
};
TEST_F(ServerMigrationTests, AllSlotsUseOldMapAndPreserveScalarsAndRoot) {
  install();
  const auto epoch = identity.rootEpoch();
  ASSERT_EQ(accept(next()), SUPLA_SUPLAN_RESULT_OK);
  sanitized();
  EXPECT_EQ(identity.rootEpoch(), epoch);
  EXPECT_TRUE(identity.identityTransition());
  EXPECT_FALSE(identity.serverSyncComplete());
  const auto stored = config.get<Control::HvacStoredConfigV2>("1_hvac_cfg2");
  Control::HvacConfiguration migrated;
  ASSERT_TRUE(Control::restoreHvacConfig(stored, &migrated));
  TChannelConfig_HVAC wire;
  ASSERT_TRUE(migrated.serverWire(&identity, &wire));
  EXPECT_EQ(wire.MainThermometerChannelId, 701u);
  EXPECT_EQ(wire.AuxThermometerChannelId, 0u);
  EXPECT_EQ(wire.PumpSwitchChannelId, 705u);
  EXPECT_EQ(wire.HeatOrColdSourceSwitchChannelId, 0u);
  identity.syncDone();
  EXPECT_TRUE(identity.serverSyncComplete());
  EXPECT_LT(config.getBlobSize("sl-migration"), 0);
  config.reboot();
  identity.load(&config);
  EXPECT_EQ(identity.resolve(701).channelNumber, 0);
  sanitized();
}
TEST_F(ServerMigrationTests, IdenticalAndReorderedMapsDoNotWrite) {
  install();
  const auto disk = config.disk;
  const int commits = config.commits;
  ASSERT_EQ(accept(initial), SUPLA_SUPLAN_RESULT_OK);
  identity.syncDone();
  EXPECT_EQ(config.commits, commits);
  EXPECT_EQ(config.disk, disk);
  // Registration order changes when a physical Channel is renumbered only
  // through its lifecycle. Existing order-permutation tests cover that path.
}
TEST_F(ServerMigrationTests, SameDeviceSelectiveChangeKeepsForeignReferences) {
  install();
  auto replacement = initial;
  replacement.ChannelId[0] = 701;
  ASSERT_EQ(accept(replacement), SUPLA_SUPLAN_RESULT_OK);
  const auto hvac = config.get<Control::HvacStoredConfigV2>("1_hvac_cfg2");
  EXPECT_EQ(hvac.referenceNamespace,
            Control::HvacReferenceNamespace::SERVER_CHANNEL_ID);
  EXPECT_EQ(hvac.references[0].channelId, 0u);
  EXPECT_EQ(hvac.references[1].channelId, 901u);
  EXPECT_EQ(hvac.references[2].channelId, 503u);
}
TEST_F(ServerMigrationTests, ExplicitServerResetIsDurableOfflineWithSameIds) {
  ASSERT_TRUE(config.setSuplaServerFromDiscovery("old.example"));
  install();
  ASSERT_TRUE(config.setSuplaServer("new.example"));
  EXPECT_TRUE(identity.scopeFenced());
  sanitized();
  // Commit the user configuration independently of migration normalization.
  ASSERT_TRUE(config.commit());
  config.reboot();
  identity.load(&config);
  EXPECT_FALSE(identity.identityAvailable());
  ASSERT_EQ(accept(initial), SUPLA_SUPLAN_RESULT_OK);
  sanitized();
  identity.syncDone();
  EXPECT_TRUE(identity.serverSyncComplete());
}
TEST_F(ServerMigrationTests, DiscoveryAndReconnectDoNotResetScope) {
  install();
  ASSERT_TRUE(config.setSuplaServerFromDiscovery("resolved-1.example"));
  ASSERT_TRUE(config.commit());
  identity.disconnected();
  ASSERT_TRUE(config.setSuplaServerFromDiscovery("resolved-2.example"));
  ASSERT_EQ(accept(initial), SUPLA_SUPLAN_RESULT_OK);
  EXPECT_LT(config.getBlobSize("sl-migration"), 0);
  const auto hvac = config.get<Control::HvacStoredConfigV2>("1_hvac_cfg2");
  EXPECT_EQ(hvac.references[1].channelId, 901u);
}
TEST_F(ServerMigrationTests, CommitFailureAtEveryPhaseReplaysAfterReboot) {
  install();
  const auto baseline = config.disk;
  for (int phase = 1; phase <= 8; ++phase) {
    SCOPED_TRACE(phase);
    identity.load(nullptr);
    config.disk = baseline;
    config.reboot();
    identity.load(&config);
    config.failCommit = config.commits + phase;
    const auto result = accept(next());
    if (result == SUPLA_SUPLAN_RESULT_OK) identity.syncDone();
    EXPECT_FALSE(identity.serverSyncComplete());
    config.failCommit = -1;
    identity.load(nullptr);
    config.reboot();
    identity.load(&config);
    ASSERT_EQ(accept(next()), SUPLA_SUPLAN_RESULT_OK);
    sanitized();
    identity.syncDone();
    EXPECT_TRUE(identity.serverSyncComplete());
  }
}
TEST_F(ServerMigrationTests, FailedFenceDoesNotChangeConfiguredServer) {
  ASSERT_TRUE(config.setSuplaServerFromDiscovery("old.example"));
  install();
  config.failWrite = true;
  EXPECT_FALSE(config.setSuplaServer("new.example"));
  char server[SUPLA_SERVER_NAME_MAXSIZE] = {};
  ASSERT_TRUE(config.getSuplaServer(server));
  EXPECT_STREQ(server, "old.example");
  EXPECT_TRUE(identity.scopeFenced());
}
TEST_F(ServerMigrationTests, RemovedAndReusedLocalNumberIsNotRebound) {
  install();
  ASSERT_TRUE(identity.forgetChannel(0));
  channels[0].reset();
  channels[0].reset(new Channel(0));
  ASSERT_EQ(accept(next()), SUPLA_SUPLAN_RESULT_OK);
  const auto hvac = config.get<Control::HvacStoredConfigV2>("1_hvac_cfg2");
  EXPECT_EQ(hvac.references[0].channelId, 0u);
  const auto container =
      config.get<Sensor::ContainerStoredConfigV2>("2_cnt_cfg2");
  EXPECT_EQ(container.config.sensorData[0].source.reference(),
            ChannelReference());
}
TEST_F(ServerMigrationTests, CorruptFenceRemainsFenced) {
  install();
  const uint8_t invalid[] = {1, 2, 3};
  config.put("sl-migration", invalid);
  ASSERT_TRUE(config.commit());
  identity.load(&config);
  EXPECT_FALSE(identity.identityAvailable());
  EXPECT_NE(accept(next()), SUPLA_SUPLAN_RESULT_OK);
  identity.syncDone();
  EXPECT_FALSE(identity.serverSyncComplete());
}
TEST_F(ServerMigrationTests, DamagedFeatureStaysFencedWithoutPublishingNewMap) {
  install();
  auto damaged = config.get<Control::HvacStoredConfigV2>("1_hvac_cfg2");
  damaged.referenceNamespace =
      static_cast<Control::HvacReferenceNamespace>(19);
  config.put("1_hvac_cfg2", damaged);
  ASSERT_TRUE(config.commit());
  EXPECT_NE(accept(next()), SUPLA_SUPLAN_RESULT_OK);
  EXPECT_EQ(identity.resolve(901).location, ServerChannelLocation::kUnresolved);
  config.reboot();
  identity.load(&config);
  EXPECT_FALSE(identity.identityAvailable());
}
}  // namespace

TEST_F(ServerMigrationTests, DeferredServerResetWaitsForDynamicTopology) {
  install();
  ASSERT_TRUE(ServerIdentity::configuredService(&config, "old.example"));
  identity.load(nullptr);
  channels.clear();
  ASSERT_TRUE(ServerIdentity::configuredService(&config, "new.example"));
  // The user intent is durable, but references have not been guessed against
  // the empty pre-topology Channel directory.
  auto record = config.get<Control::HvacStoredConfigV2>("1_hvac_cfg2");
  EXPECT_EQ(record.references[0].channelId, 501u);
  config.reboot();
  for (int n = 0; n < 8; ++n) channels.emplace_back(new Channel(n));
  identity.load(&config);
  sanitized();
  ASSERT_EQ(accept(initial), SUPLA_SUPLAN_RESULT_OK);
  identity.syncDone();
  EXPECT_TRUE(identity.serverSyncComplete());
}

TEST_F(ServerMigrationTests, MissingOldMapClearsServerRefsWithoutNumericGuess) {
  install();
  auto container = config.get<Sensor::ContainerStoredConfigV2>("2_cnt_cfg2");
  container.config.sensorData[0].source.assign(ChannelReference::local(0));
  config.put("2_cnt_cfg2", container);
  ASSERT_TRUE(config.eraseKey("sl-identity"));
  ASSERT_TRUE(config.commit());
  identity.load(&config);
  ASSERT_EQ(accept(next()), SUPLA_SUPLAN_RESULT_OK);
  auto record = config.get<Control::HvacStoredConfigV2>("1_hvac_cfg2");
  for (const auto &reference : record.references)
    EXPECT_EQ(reference.local.isSet, 0);
  container = config.get<Sensor::ContainerStoredConfigV2>("2_cnt_cfg2");
  EXPECT_EQ(container.config.sensorData[0].source.reference(),
            ChannelReference::local(0));
  EXPECT_EQ(container.config.sensorData[1].source.reference(),
            ChannelReference());
}

TEST_F(ServerMigrationTests, PeerEraseFailureStaysFencedAcrossReboot) {
  install();
  const uint8_t opaque[] = {1, 1, 2};
  config.put("sl-peer-0", opaque);
  ASSERT_TRUE(config.commit());
  config.failErase = true;
  EXPECT_NE(accept(next()), SUPLA_SUPLAN_RESULT_OK);
  EXPECT_TRUE(identity.fullScopeFenced());
  config.reboot();
  identity.load(&config);
  EXPECT_FALSE(identity.identityAvailable());
  config.failErase = false;
  ASSERT_EQ(accept(next()), SUPLA_SUPLAN_RESULT_OK);
  EXPECT_LT(config.getBlobSize("sl-peer-0"), 0);
  sanitized();
}

TEST_F(ServerMigrationTests, AnotherMapChangeBeforeSyncUsesLatestOwnMap) {
  install();
  auto changed = initial;
  changed.ChannelId[0] = 701;
  ASSERT_EQ(accept(changed), SUPLA_SUPLAN_RESULT_OK);
  auto hvac = config.get<Control::HvacStoredConfigV2>("1_hvac_cfg2");
  hvac.references[0].channelId = 701;  // New authoritative config before sync.
  config.put("1_hvac_cfg2", hvac);
  ASSERT_TRUE(config.commit());
  changed.ChannelId[0] = 801;
  ASSERT_EQ(accept(changed), SUPLA_SUPLAN_RESULT_OK);
  hvac = config.get<Control::HvacStoredConfigV2>("1_hvac_cfg2");
  EXPECT_EQ(hvac.references[0].channelId, 0u);
  EXPECT_EQ(hvac.references[1].channelId, 901u);
  identity.syncDone();
  EXPECT_TRUE(identity.serverSyncComplete());
}

TEST_F(ServerMigrationTests, AnotherDeviceIdBeforeSyncPreservesLatestOwnRefs) {
  install();
  ASSERT_EQ(accept(next()), SUPLA_SUPLAN_RESULT_OK);
  auto hvac = config.get<Control::HvacStoredConfigV2>("1_hvac_cfg2");
  hvac.referenceNamespace =
      Control::HvacReferenceNamespace::SERVER_CHANNEL_ID;
  for (auto &reference : hvac.references) reference.channelId = 0;
  hvac.references[0].channelId = 701;
  hvac.references[1].channelId = 9909;
  config.put("1_hvac_cfg2", hvac);
  ASSERT_TRUE(config.commit());
  auto third = next();
  third.DeviceId = 303;
  for (int i = 0; i < 8; ++i) third.ChannelId[i] = 801 + i;
  ASSERT_EQ(accept(third), SUPLA_SUPLAN_RESULT_OK);
  hvac = config.get<Control::HvacStoredConfigV2>("1_hvac_cfg2");
  EXPECT_EQ(hvac.references[0].local.isSet, 1);
  EXPECT_EQ(hvac.references[0].local.channelNo, 0);
  EXPECT_EQ(hvac.references[1].local.isSet, 0);
}

TEST_F(ServerMigrationTests, ValidCrcButOversizedFenceCountIsRejected) {
  install();
  std::array<uint8_t, 46> damaged = {};
  damaged[0] = 1;
  damaged[1] = 2;
  damaged[2] = 1;
  damaged[3] = 255;
  const auto crc = calculateCrc16(damaged.data(), damaged.size() - 2);
  damaged[44] = crc;
  damaged[45] = crc >> 8;
  config.put("sl-migration", damaged);
  ASSERT_TRUE(config.commit());
  identity.load(&config);
  EXPECT_NE(accept(next()), SUPLA_SUPLAN_RESULT_OK);
  EXPECT_TRUE(identity.fullScopeFenced());
}

TEST_F(ServerMigrationTests, AnotherExplicitServerBeforeSyncSanitizesAgain) {
  install();
  ASSERT_TRUE(ServerIdentity::configuredServerChanged(&config));
  ASSERT_EQ(accept(initial), SUPLA_SUPLAN_RESULT_OK);
  auto hvac = config.get<Control::HvacStoredConfigV2>("1_hvac_cfg2");
  hvac.referenceNamespace =
      Control::HvacReferenceNamespace::SERVER_CHANNEL_ID;
  for (auto &reference : hvac.references) reference.channelId = 0;
  hvac.references[0].channelId = 501;
  hvac.references[1].channelId = 901;
  config.put("1_hvac_cfg2", hvac);
  ASSERT_TRUE(config.commit());
  ASSERT_TRUE(ServerIdentity::configuredServerChanged(&config));
  hvac = config.get<Control::HvacStoredConfigV2>("1_hvac_cfg2");
  EXPECT_EQ(hvac.references[0].local.isSet, 1);
  EXPECT_EQ(hvac.references[0].local.channelNo, 0);
  EXPECT_EQ(hvac.references[1].local.isSet, 0);
}

TEST_F(ServerMigrationTests, LegacyServiceBaselineDoesNotGuessDiscoveryOrigin) {
  install();
  ASSERT_TRUE(config.setSuplaServerFromDiscovery("resolved-cluster.example"));
  const int before = config.commits;
  ASSERT_TRUE(ServerIdentity::configuredService(&config, "configured.example"));
  EXPECT_EQ(config.commits, before + 1);
  EXPECT_TRUE(identity.serverSyncComplete());
  EXPECT_LT(config.getBlobSize("sl-migration"), 0);
  auto stored = config.get<Control::HvacStoredConfigV2>("1_hvac_cfg2");
  EXPECT_EQ(stored.references[1].channelId, 901u);

  // Once the explicit-service baseline is available, an offline change is
  // detectable even with identical DEVICE_IDENTITIES on the next server.
  ASSERT_TRUE(ServerIdentity::configuredService(&config, "new.example"));
  EXPECT_TRUE(identity.fullScopeFenced());
  stored = config.get<Control::HvacStoredConfigV2>("1_hvac_cfg2");
  EXPECT_EQ(stored.referenceNamespace,
            Control::HvacReferenceNamespace::LOCAL_CHANNEL_NUMBER);
  EXPECT_EQ(stored.references[1].local.isSet, 0);
}

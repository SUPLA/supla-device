// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <SuplaDevice.h>
#include <fcntl.h>
#include <gtest/gtest.h>
#include <simple_time.h>
#include <srpc_mock.h>
#include <supla/channels/channel.h>
#include <supla/device/register_device.h>
#include <supla/protocol/supla_srpc.h>
#include <supla/protocol/suplan_protocol.h>
#include <supla/storage/key_value.h>
#include <supla/suplan/suplan_server_associations.h>
#include <supla/suplan/suplan_server_identity.h>
#include <suplan/suplan_crypto.h>
#include <suplan/suplan_wire.h>
#include <suplan_crypto_openssl.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include <array>
#include <cstring>
#include <vector>

namespace {
using namespace Supla::SupLan;  // NOLINT(build/namespaces)
using Supla::Device::ServerAssociations;
using Supla::Device::ServerIdentity;

class AssociationConfig : public Supla::KeyValue {
 public:
  using Supla::KeyValue::getBlobSize;
  bool init() override { return true; }
  bool setBlob(const char *key, const char *value, size_t size) override {
    if (failWrite) return false;
    return KeyValue::setBlob(key, value, size);
  }
  bool commit() override {
    ++writes;
    if (failCommit) return false;
    std::array<uint8_t, 16384> bytes = {};
    size_t size = serializeToMemory(bytes.data(), bytes.size());
    if (size == SIZE_MAX) return false;
    disk.assign(bytes.begin(), bytes.begin() + size);
    return true;
  }
  void reboot() {
    removeAllMemory();
    if (!disk.empty()) {
      EXPECT_TRUE(initFromMemory(disk.data(), disk.size()));
    }
  }
  std::vector<uint8_t> blob(const char *key) {
    int size = getBlobSize(key);
    if (size < 0) return {};
    std::vector<uint8_t> value(size);
    EXPECT_TRUE(getBlob(key, reinterpret_cast<char *>(value.data()), size));
    return value;
  }
  int writes = 0;
  bool failWrite = false;
  bool failCommit = false;
  std::vector<uint8_t> disk;
};
class IdleDatagrams : public DatagramPort {
 public:
  bool sendUnicast(const Endpoint &, const uint8_t *, size_t) override {
    return true;
  }
  bool sendLocateMulticast(const uint8_t *, size_t) override { return true; }
  int pollReceive(uint8_t *, size_t, Endpoint *) override { return 0; }
  size_t maxDatagramPayload() const override { return 250; }
  uint32_t nowMs() const override { return now; }
  uint32_t now = 0;
};

class ServerAssociationTests : public ::testing::Test {
 protected:
  void SetUp() override {
    Supla::Channel::resetToDefaults();
    Supla::RegisterDevice::resetToDefaults();
    channel = new Supla::Channel(7);
    channel->setType(SUPLA_CHANNELTYPE_RELAY);
    identity.load(&config);
    ASSERT_TRUE(identity.registrationStarted());
    identity.registrationSucceeded();
    TSD_SuplaDeviceIdentities snapshot = {};
    snapshot.DeviceId = 101;
    snapshot.ChannelCount = 1;
    snapshot.ChannelId[0] = 501;
    ASSERT_EQ(identity.accept(snapshot).Result, SUPLA_SUPLAN_RESULT_OK);
    identity.syncDone();
    associations.load(&config, &identity);
    source.PeerContext = {1, 0, 1, 101, 1, 202, identity.rootEpoch(), 1};
    source.AclRevision = 1;
    source.AclEntryCount = 1;
    source.Acl[0] = {1, 501, 1};
    destination.PeerContext = {1, 0, 1, 202, 1, 101, 345, 1};
    destination.AclRevision = 1;
    destination.PeerKeySize = 32;
    std::memset(destination.PeerKey, 0x42, 32);
    destination.ResourceCount = 1;
    destination.Resources[0] = {1, 601, 1};
  }
  void TearDown() override {
    associations.load(nullptr, nullptr);
    identity.load(nullptr);
    delete channel;
    Supla::Channel::resetToDefaults();
    Supla::RegisterDevice::resetToDefaults();
  }
  uint8_t src() { return associations.accept(source).Result; }
  uint8_t dst() { return associations.accept(destination).Result; }
  void reboot() {
    associations.load(nullptr, nullptr);
    identity.load(nullptr);
    config.reboot();
    identity.load(&config);
    associations.load(&config, &identity);
  }
  AssociationConfig config;
  ServerIdentity identity;
  PeerTable peers;
  IdleDatagrams datagrams;
  OpenSslCryptoPort crypto;
  Runtime runtime{&crypto, &datagrams,           nullptr,
                  &peers,  {kNodeIdDevice, 101}, 29};
  ServerAssociations associations{&peers, &runtime};
  Supla::Channel *channel = nullptr;
  TSDS_SuplaSetSuplanSourceAssociation source = {};
  TSDS_SuplaSetSuplanDestinationAssociation destination = {};
};

TEST_F(ServerAssociationTests, SourceInitialReplayAndDerivedKeyNotPersisted) {
  source.Flags = SUPLA_SUPLAN_SOURCE_FLAG_RETURN_PEER_KEY;
  auto result = associations.accept(source);
  ASSERT_EQ(result.Result, SUPLA_SUPLAN_RESULT_OK);
  ASSERT_EQ(result.PeerKeySize, 32);
  PeerContext context = {1, 0, {1, 101}, {1, 202}, identity.rootEpoch(), 1};
  PeerMaterial expected = {};
  ASSERT_TRUE(derivePeerMaterial(&context, identity.rootKey(), &expected));
  EXPECT_EQ(std::memcmp(result.PeerKey, expected.peerKey, 32), 0);
  auto blob = config.blob("sl-peer-0");
  ASSERT_EQ(blob.size(), 42u);
  EXPECT_EQ(blob[0], 1);
  EXPECT_EQ(blob[1], 1);
  int writes = config.writes;
  auto replay = associations.accept(source);
  EXPECT_EQ(replay.Result, 0);
  EXPECT_EQ(std::memcmp(result.PeerKey, replay.PeerKey, 32), 0);
  EXPECT_EQ(config.writes, writes);
  source.Flags = 0;
  replay = associations.accept(source);
  EXPECT_EQ(replay.PeerKeySize, 0);
  for (auto byte : replay.PeerKey) EXPECT_EQ(byte, 0);
}
TEST_F(ServerAssociationTests, SourceRevisionAndGenerationNamespaces) {
  ASSERT_EQ(src(), 0);
  source.AclRevision = 2;
  source.Acl[0].Permissions = 2;
  ASSERT_EQ(src(), 0);
  EXPECT_TRUE(peers.authorize(0, {1, 501}, kPermissionControl));
  int writes = config.writes;
  source.AclRevision = 1;
  EXPECT_EQ(src(), SUPLA_SUPLAN_RESULT_STALE_REVISION);
  source.AclRevision = 2;
  source.Acl[0].Permissions = 4;
  EXPECT_EQ(src(), SUPLA_SUPLAN_RESULT_REVISION_CONFLICT);
  EXPECT_EQ(config.writes, writes);
  source.PeerContext.PeerGeneration = 2;
  source.AclRevision = 1;
  EXPECT_EQ(src(), 0);
  source.PeerContext.PeerGeneration = 1;
  source.AclRevision = 100;
  EXPECT_EQ(src(), SUPLA_SUPLAN_RESULT_STALE_GENERATION);
}
TEST_F(ServerAssociationTests, SourceRootEpochMismatchDoesNotWrite) {
  ASSERT_EQ(src(), 0);
  int writes = config.writes;
  ++source.PeerContext.RootEpoch;
  EXPECT_EQ(src(), SUPLA_SUPLAN_RESULT_ROOT_EPOCH_MISMATCH);
  EXPECT_EQ(config.writes, writes);
}
TEST_F(ServerAssociationTests, SourceFlagsAndCanonicalValidation) {
  source.Flags = 2;
  EXPECT_EQ(src(), SUPLA_SUPLAN_RESULT_INVALID_ARGUMENT);
  source.Flags = 1;
  source.AclEntryCount = 0;
  EXPECT_EQ(src(), SUPLA_SUPLAN_RESULT_INVALID_ARGUMENT);
  source.Flags = 0;
  source.AclEntryCount = 1;
  source.Acl[0].Permissions = 3;
  EXPECT_EQ(src(), SUPLA_SUPLAN_RESULT_INVALID_ARGUMENT);
  source.Acl[0].Permissions = 8;
  EXPECT_EQ(src(), SUPLA_SUPLAN_RESULT_INVALID_ARGUMENT);
  source.Acl[0] = {3, 501, 1};
  EXPECT_EQ(src(), SUPLA_SUPLAN_RESULT_UNSUPPORTED);
}
TEST_F(ServerAssociationTests, SourceOwnershipAndHierarchicalUnion) {
  source.Acl[0] = {1, 601, 1};
  EXPECT_EQ(src(), SUPLA_SUPLAN_RESULT_NOT_AUTHORIZED);
  source.Acl[0] = {2, 202, 1};
  EXPECT_EQ(src(), SUPLA_SUPLAN_RESULT_NOT_AUTHORIZED);
  source.AclEntryCount = 2;
  source.Acl[0] = {1, 501, 4};
  source.Acl[1] = {2, 101, 2};
  ASSERT_EQ(src(), 0);
  EXPECT_EQ(peers.effectivePermissions(0, {1, 501}), 7);
  EXPECT_TRUE(peers.authorize(0, {2, 101}, 1));
  EXPECT_FALSE(peers.authorize(0, {1, 601}, 1));
  EXPECT_FALSE(peers.authorize(0, {2, 202}, 1));
}
TEST_F(ServerAssociationTests, SourceRebootRestoresRootDerivedMaterial) {
  ASSERT_EQ(src(), 0);
  PeerMaterial before = {};
  ASSERT_TRUE(peers.materialFor(0, &before));
  int writes = config.writes;
  reboot();
  ASSERT_EQ(peers.size(), 1);
  PeerMaterial after = {};
  ASSERT_TRUE(peers.materialFor(0, &after));
  EXPECT_EQ(std::memcmp(before.peerKey, after.peerKey, 32), 0);
  EXPECT_TRUE(peers.authorize(0, {1, 501}, 1));
  EXPECT_EQ(config.writes, writes);
  EXPECT_EQ(src(), 0);
  EXPECT_EQ(config.writes, writes);
}
TEST_F(ServerAssociationTests, DestinationInitialAuthorizationOnlyReplay) {
  ASSERT_EQ(dst(), 0);
  auto blob = config.blob("sl-peer-0");
  ASSERT_EQ(blob.size(), 74u);
  EXPECT_EQ(std::memcmp(blob.data() + 34, destination.PeerKey, 32), 0);
  int writes = config.writes;
  EXPECT_EQ(dst(), 0);
  destination.PeerKeySize = 0;
  EXPECT_EQ(dst(), 0);
  EXPECT_EQ(config.writes, writes);
  destination.AclRevision = 2;
  destination.Resources[0].Permissions = 2;
  ASSERT_EQ(dst(), 0);
  EXPECT_EQ(std::memcmp(peers.get(0)->peerKey, destination.PeerKey, 32), 0);
  EXPECT_TRUE(peers.authorize(0, {1, 601}, 2));
}
TEST_F(ServerAssociationTests, DestinationExactCredentialRequired) {
  destination.PeerKeySize = 0;
  EXPECT_EQ(dst(), SUPLA_SUPLAN_RESULT_PEER_KEY_REQUIRED);
  EXPECT_EQ(peers.size(), 0);
  destination.PeerKeySize = 32;
  ASSERT_EQ(dst(), 0);
  destination.PeerKeySize = 0;
  ++destination.PeerContext.PeerGeneration;
  EXPECT_EQ(dst(), SUPLA_SUPLAN_RESULT_PEER_KEY_REQUIRED);
  ++destination.PeerContext.RootEpoch;
  EXPECT_EQ(dst(), SUPLA_SUPLAN_RESULT_PEER_KEY_REQUIRED);
  EXPECT_EQ(peers.size(), 1);
}
TEST_F(ServerAssociationTests, DestinationRevisionConflictAndStale) {
  ASSERT_EQ(dst(), 0);
  destination.AclRevision = 2;
  ASSERT_EQ(dst(), 0);
  int writes = config.writes;
  destination.AclRevision = 1;
  EXPECT_EQ(dst(), SUPLA_SUPLAN_RESULT_STALE_REVISION);
  destination.AclRevision = 2;
  destination.Resources[0].Permissions = 4;
  EXPECT_EQ(dst(), SUPLA_SUPLAN_RESULT_REVISION_CONFLICT);
  destination.Resources[0].Permissions = 1;
  destination.PeerKey[0] ^= 1;
  EXPECT_EQ(dst(), SUPLA_SUPLAN_RESULT_REVISION_CONFLICT);
  destination.AclRevision = 3;
  EXPECT_EQ(dst(), SUPLA_SUPLAN_RESULT_REVISION_CONFLICT);
  EXPECT_EQ(config.writes, writes);
}
TEST_F(ServerAssociationTests, DestinationGenerationAndRootReplacement) {
  destination.AclRevision = 50;
  ASSERT_EQ(dst(), 0);
  ++destination.PeerContext.PeerGeneration;
  destination.AclRevision = 1;
  destination.PeerKey[0] ^= 1;
  ASSERT_EQ(dst(), 0);
  EXPECT_EQ(peers.get(0)->aclRevision, 1u);
  --destination.PeerContext.PeerGeneration;
  EXPECT_EQ(dst(), SUPLA_SUPLAN_RESULT_STALE_GENERATION);
  ++destination.PeerContext.RootEpoch;
  EXPECT_EQ(dst(), 0);  // No generation ordering across root epochs.
}
TEST_F(ServerAssociationTests, DestinationRebootRestoresCredentialAndExpected) {
  ASSERT_EQ(dst(), 0);
  reboot();
  ASSERT_EQ(peers.size(), 1);
  EXPECT_EQ(std::memcmp(peers.get(0)->peerKey, destination.PeerKey, 32), 0);
  EXPECT_TRUE(peers.authorize(0, {1, 601}, 1));
  EXPECT_FALSE(peers.authorize(0, {1, 602}, 1));
  destination.PeerKeySize = 0;
  EXPECT_EQ(dst(), 0);
}
TEST_F(ServerAssociationTests,
       CommitFailurePreservesPreviousRuntimeAndStaging) {
  ASSERT_EQ(dst(), 0);
  auto before = config.blob("sl-peer-0");
  destination.AclRevision = 2;
  destination.Resources[0].Permissions = 4;
  config.failCommit = true;
  EXPECT_EQ(dst(), SUPLA_SUPLAN_RESULT_PERSISTENCE_ERROR);
  EXPECT_EQ(peers.get(0)->aclRevision, 1u);
  EXPECT_TRUE(peers.authorize(0, {1, 601}, 1));
  EXPECT_FALSE(peers.authorize(0, {1, 601}, 4));
  EXPECT_EQ(config.blob("sl-peer-0"), before);
  config.failCommit = false;
  ASSERT_TRUE(config.commit());
  reboot();
  EXPECT_EQ(peers.get(0)->aclRevision, 1u);
}
TEST_F(ServerAssociationTests, FirstWriteFailureDoesNotActivateAssociation) {
  config.failWrite = true;
  EXPECT_EQ(src(), SUPLA_SUPLAN_RESULT_PERSISTENCE_ERROR);
  EXPECT_EQ(peers.size(), 0);
  EXPECT_EQ(config.getBlobSize("sl-peer-0"), -1);
  config.failWrite = false;
  EXPECT_EQ(src(), 0);
}
TEST_F(ServerAssociationTests,
       FailedCommitIdenticalReplayRequiresDurabilityRepair) {
  ASSERT_EQ(src(), 0);
  source.AclRevision = 2;
  config.failCommit = true;
  EXPECT_EQ(src(), SUPLA_SUPLAN_RESULT_PERSISTENCE_ERROR);
  source.AclRevision = 1;
  EXPECT_EQ(src(), SUPLA_SUPLAN_RESULT_PERSISTENCE_ERROR);
  config.failCommit = false;
  int writes = config.writes;
  EXPECT_EQ(src(), 0);
  EXPECT_EQ(config.writes, writes + 1);
  EXPECT_EQ(src(), 0);
  EXPECT_EQ(config.writes, writes + 1);
}
TEST_F(ServerAssociationTests,
       SourceEmptyCleanupReclaimsAndReplayDoesNotAllocate) {
  ASSERT_EQ(src(), 0);
  source.AclRevision = 2;
  source.AclEntryCount = 0;
  EXPECT_EQ(src(), 0);
  EXPECT_EQ(peers.size(), 0);
  EXPECT_EQ(config.getBlobSize("sl-peer-0"), -1);
  int writes = config.writes;
  EXPECT_EQ(src(), 0);
  EXPECT_EQ(config.writes, writes);
  source.PeerContext.PeerGeneration = 2;
  source.AclRevision = 1;
  source.AclEntryCount = 1;
  EXPECT_EQ(src(), 0);
  EXPECT_EQ(peers.size(), 1);
  EXPECT_EQ(config.blob("sl-peer-0").size(), 42u);
}
TEST_F(ServerAssociationTests, DestinationEmptyCleanupWithoutCredential) {
  ASSERT_EQ(dst(), 0);
  ++destination.PeerContext.PeerGeneration;
  destination.AclRevision = 1;
  destination.PeerKeySize = 0;
  destination.ResourceCount = 0;
  EXPECT_EQ(dst(), 0);
  EXPECT_EQ(peers.size(), 0);
  EXPECT_EQ(config.getBlobSize("sl-peer-0"), -1);
  int writes = config.writes;
  EXPECT_EQ(dst(), 0);
  EXPECT_EQ(config.writes, writes);
  reboot();
  EXPECT_EQ(peers.size(), 0);
}
TEST_F(ServerAssociationTests, EmptyCleanupOfNeverInstalledPeerDoesNotWrite) {
  source.AclEntryCount = 0;
  destination.PeerKeySize = 0;
  destination.ResourceCount = 0;
  int writes = config.writes;
  EXPECT_EQ(src(), 0);
  EXPECT_EQ(dst(), 0);
  EXPECT_EQ(peers.size(), 0);
  EXPECT_EQ(config.writes, writes);
}
TEST_F(ServerAssociationTests,
       RemovalOfMiddlePeerPreservesIndexesAndRecoversSlot) {
  for (uint32_t peer = 202; peer <= 204; ++peer) {
    source.PeerContext.DestinationNodeId = peer;
    ASSERT_EQ(src(), 0);
  }
  auto last = *peers.get(2);
  source.PeerContext.DestinationNodeId = 203;
  source.AclRevision = 2;
  source.AclEntryCount = 0;
  ASSERT_EQ(src(), 0);
  EXPECT_EQ(peers.size(), 2);
  EXPECT_EQ(peers.get(1), nullptr);
  ASSERT_NE(peers.get(2), nullptr);
  EXPECT_EQ(std::memcmp(peers.get(2)->contextBytes, last.contextBytes, 27), 0);
  EXPECT_TRUE(peers.authorize(0, {1, 501}, 1));
  EXPECT_TRUE(peers.authorize(2, {1, 501}, 1));
  source.PeerContext.DestinationNodeId = 205;
  source.AclRevision = 1;
  source.AclEntryCount = 1;
  ASSERT_EQ(src(), 0);
  PeerContext context = {};
  ASSERT_TRUE(decodePeerContext(peers.get(1)->contextBytes, &context));
  EXPECT_EQ(context.destination.nodeId, 205u);
  reboot();
  EXPECT_EQ(peers.size(), 3);
}
TEST_F(ServerAssociationTests,
       RemovedPeerHasNoLocateHandshakeRetryOrRecoveryState) {
  ASSERT_EQ(dst(), 0);
  ASSERT_TRUE(runtime.requestRead(0, {1, 601}));
  EXPECT_GT(runtime.poolDiagnostics().locates.used, 0);
  destination.AclRevision = 2;
  destination.ResourceCount = 0;
  destination.PeerKeySize = 0;
  ASSERT_EQ(dst(), 0);
  auto pools = runtime.poolDiagnostics();
  EXPECT_EQ(pools.sessions.used, 0);
  EXPECT_EQ(pools.pending.used, 0);
  EXPECT_EQ(pools.locates.used, 0);
  EXPECT_EQ(pools.interests.used, 0);
  EXPECT_EQ(pools.retries.used, 0);
  EXPECT_EQ(pools.deferredEvents.used, 0);
  EXPECT_FALSE(runtime.recoveryStatus(0).active);
  runtime.iterate();
  EXPECT_FALSE(runtime.requestRead(0, {1, 601}));
}
TEST_F(ServerAssociationTests,
       PeerCapacityAtomicAndEmptyCleanupAtFullCapacity) {
  for (unsigned i = 0; i < SUPLAN_MAX_PERSISTENT_PEERS; ++i) {
    source.PeerContext.DestinationNodeId = 202 + i;
    ASSERT_EQ(src(), 0);
  }
  int writes = config.writes;
  source.PeerContext.DestinationNodeId = 999;
  EXPECT_EQ(src(), SUPLA_SUPLAN_RESULT_CAPACITY_EXCEEDED);
  EXPECT_EQ(peers.size(), SUPLAN_MAX_PERSISTENT_PEERS);
  EXPECT_EQ(config.writes, writes);
  source.AclEntryCount = 0;
  EXPECT_EQ(src(), 0);
  EXPECT_EQ(config.writes, writes);
  source.PeerContext.DestinationNodeId = 203;
  source.AclRevision = 2;
  EXPECT_EQ(src(), 0);
  source.PeerContext.DestinationNodeId = 999;
  source.AclRevision = 1;
  source.AclEntryCount = 1;
  EXPECT_EQ(src(), 0);
}
TEST_F(ServerAssociationTests, ExpectedCapacityRejectsWholeReplacement) {
  unsigned remaining = SUPLAN_MAX_TOTAL_EXPECTED_ENTRIES;
  unsigned peer = 202;
  while (remaining) {
    unsigned count = remaining > 89 ? 89 : remaining;
    destination.PeerContext.SourceNodeId = peer++;
    destination.ResourceCount = count;
    for (unsigned i = 0; i < count; ++i) {
      destination.Resources[i] = {1, 601 + i, 1};
    }
    ASSERT_EQ(dst(), 0);
    remaining -= count;
  }
  int writes = config.writes;
  ++destination.AclRevision;
  ++destination.ResourceCount;
  destination.Resources[destination.ResourceCount - 1] = {1, 900, 1};
  EXPECT_EQ(dst(), SUPLA_SUPLAN_RESULT_CAPACITY_EXCEEDED);
  EXPECT_EQ(peers.entryCount(true), SUPLAN_MAX_TOTAL_EXPECTED_ENTRIES);
  EXPECT_EQ(config.writes, writes);
}
TEST_F(ServerAssociationTests, AclAndExpectedUseIndependentCapacityPools) {
  source.AclEntryCount = 2;
  source.Acl[1] = {2, 101, 1};
  for (unsigned i = 0; i < SUPLAN_MAX_PERSISTENT_PEERS / 2; ++i) {
    source.PeerContext.DestinationNodeId = 300 + i;
    ASSERT_EQ(src(), 0);
  }
  unsigned remaining = SUPLAN_MAX_TOTAL_EXPECTED_ENTRIES;
  unsigned peer = 202;
  while (remaining) {
    unsigned count = remaining > 89 ? 89 : remaining;
    destination.PeerContext.SourceNodeId = peer++;
    destination.ResourceCount = count;
    for (unsigned i = 0; i < count; ++i) {
      destination.Resources[i] = {1, 601 + i, 1};
    }
    ASSERT_EQ(dst(), 0);
    remaining -= count;
  }
  EXPECT_EQ(peers.entryCount(true), SUPLAN_MAX_TOTAL_EXPECTED_ENTRIES);
  EXPECT_EQ(peers.entryCount(false), SUPLAN_MAX_PERSISTENT_PEERS);
  reboot();
  EXPECT_EQ(peers.entryCount(true), SUPLAN_MAX_TOTAL_EXPECTED_ENTRIES);
}
TEST_F(ServerAssociationTests, SourceAclCapacityRejectsWholeReplacement) {
  auto extra = new Supla::Channel(8);
  ASSERT_TRUE(identity.registrationStarted());
  identity.registrationSucceeded();
  TSD_SuplaDeviceIdentities snapshot = {};
  snapshot.DeviceId = 101;
  snapshot.ChannelCount = 2;
  snapshot.ChannelId[0] = 501;
  snapshot.ChannelId[1] = 502;
  ASSERT_EQ(identity.accept(snapshot).Result, 0);
  identity.syncDone();
  source.AclEntryCount = 2;
  source.Acl[1] = {2, 101, 1};
  for (unsigned i = 0; i < SUPLAN_MAX_PERSISTENT_PEERS; ++i) {
    source.PeerContext.DestinationNodeId = 300 + i;
    ASSERT_EQ(src(), 0);
  }
  EXPECT_EQ(peers.entryCount(false), SUPLAN_MAX_TOTAL_ACL_ENTRIES);
  int writes = config.writes;
  source.AclEntryCount = 3;
  source.Acl[1] = {1, 502, 1};
  source.Acl[2] = {2, 101, 1};
  source.AclRevision = 2;
  EXPECT_EQ(src(), SUPLA_SUPLAN_RESULT_CAPACITY_EXCEEDED);
  EXPECT_EQ(peers.entryCount(false), SUPLAN_MAX_TOTAL_ACL_ENTRIES);
  EXPECT_EQ(config.writes, writes);
  associations.load(nullptr, nullptr);
  identity.load(nullptr);
  delete extra;
}
TEST_F(ServerAssociationTests, CorruptOrUnknownRecordDoesNotRestore) {
  ASSERT_EQ(dst(), 0);
  auto blob = config.blob("sl-peer-0");
  blob[0] = 99;
  ASSERT_TRUE(config.setBlob(
      "sl-peer-0", reinterpret_cast<const char *>(blob.data()), blob.size()));
  ASSERT_TRUE(config.commit());
  reboot();
  EXPECT_EQ(peers.size(), 0);
  destination.PeerKeySize = 0;
  EXPECT_EQ(dst(), SUPLA_SUPLAN_RESULT_PEER_KEY_REQUIRED);
}
TEST_F(ServerAssociationTests, RootRotationMakesSourceRuntimeIneligible) {
  ASSERT_EQ(src(), 0);
  auto record = config.blob("sl-peer-0");
  ASSERT_TRUE(identity.rotateRoot());
  EXPECT_FALSE(peers.authorize(0, {1, 501}, 1));
  PeerMaterial material = {};
  EXPECT_FALSE(peers.materialFor(0, &material));
  associations.identityChanged();
  EXPECT_EQ(peers.size(), 0);
  EXPECT_EQ(config.blob("sl-peer-0"), record);  // No unnecessary flash erase.
  reboot();
  EXPECT_EQ(peers.size(), 0);
}
TEST_F(ServerAssociationTests,
       TransitionBlocksDataButAllowsNewMapProvisioning) {
  ASSERT_EQ(src(), 0);
  ASSERT_TRUE(identity.forgetChannel(7));
  associations.identityChanged();
  EXPECT_FALSE(peers.authorize(0, {1, 501}, 1));
  source.Acl[0] = {2, 101, 1};
  source.AclRevision = 2;
  EXPECT_EQ(src(), 0);
  EXPECT_FALSE(peers.authorize(0, {2, 101}, 1));
}

class AssociationSrpc : public Supla::Protocol::SuplaSrpc {
 public:
  explicit AssociationSrpc(SuplaDeviceClass *device, int protocol = 29)
      : SuplaSrpc(device, protocol) {}
  void prepare(void *handle, bool ready = true) {
    srpc = handle;
    registered = ready ? 1 : 0;
  }
  ~AssociationSrpc() { srpc = nullptr; }
};
TEST_F(ServerAssociationTests, SrpcDispatchesProductionSourceAndDestination) {
  SimpleTime time;
  testing::NiceMock<SrpcMock> wire;
  SuplaDeviceClass device;
  AssociationSrpc srpc(&device);
  srpc.serverIdentity().load(&config);
  Supla::Protocol::SupLan adapter(&device, &peers, nullptr, 0);
  adapter.attachRuntime(&runtime);
  adapter.attachServerAssociations(&associations);
  int handle = 0;
  srpc.prepare(&handle);
  source.Flags = 1;
  EXPECT_CALL(wire, srpc_getdata(&handle, testing::_, testing::_))
      .WillOnce([this](void *, TsrpcReceivedData *received, unsigned int) {
        received->call_id = SUPLA_SD_CALL_SET_SUPLAN_SOURCE_ASSOCIATION;
        received->data.sd_set_suplan_source_association = &source;
        return SUPLA_RESULT_TRUE;
      });
  EXPECT_CALL(wire, sourceAssociationResult(&handle, testing::_))
      .WillOnce(
          [this](void *, TDS_SuplaSetSuplanSourceAssociationResult *result) {
            EXPECT_EQ(result->Result, 0);
            EXPECT_EQ(result->PeerKeySize, 32);
            EXPECT_EQ(result->AclRevision, 1u);
            EXPECT_EQ(config.blob("sl-peer-0").size(), 42u);
            return 1;
          });
  Supla::messageReceived(
      &handle, 1, SUPLA_SD_CALL_SET_SUPLAN_SOURCE_ASSOCIATION, &srpc, 29);
  EXPECT_CALL(wire, srpc_getdata(&handle, testing::_, testing::_))
      .WillOnce([this](void *, TsrpcReceivedData *received, unsigned int) {
        received->call_id = SUPLA_SD_CALL_SET_SUPLAN_DESTINATION_ASSOCIATION;
        received->data.sd_set_suplan_destination_association = &destination;
        return SUPLA_RESULT_TRUE;
      });
  EXPECT_CALL(wire, destinationAssociationResult(&handle, testing::_))
      .WillOnce([this](void *,
                       TDS_SuplaSetSuplanDestinationAssociationResult *result) {
        EXPECT_EQ(result->Result, 0);
        EXPECT_EQ(result->PeerContext.SourceNodeId, 202u);
        EXPECT_EQ(config.blob("sl-peer-1").size(), 74u);
        return 1;
      });
  Supla::messageReceived(
      &handle, 1, SUPLA_SD_CALL_SET_SUPLAN_DESTINATION_ASSOCIATION, &srpc, 29);
}
TEST_F(ServerAssociationTests, SrpcVersionAndRegistrationGates) {
  SimpleTime time;
  testing::NiceMock<SrpcMock> wire;
  SuplaDeviceClass device;
  AssociationSrpc oldSrpc(&device, 28);
  AssociationSrpc current(&device);
  oldSrpc.serverIdentity().load(&config);
  current.serverIdentity().load(&config);
  int handle = 0;
  oldSrpc.prepare(&handle);
  current.prepare(&handle, false);
  EXPECT_CALL(wire, sourceAssociationResult(&handle, testing::_))
      .WillOnce([](void *, TDS_SuplaSetSuplanSourceAssociationResult *result) {
        EXPECT_EQ(result->Result, SUPLA_SUPLAN_RESULT_UNSUPPORTED);
        EXPECT_EQ(result->PeerKeySize, 0);
        return 1;
      })
      .WillOnce([](void *, TDS_SuplaSetSuplanSourceAssociationResult *result) {
        EXPECT_EQ(result->Result, SUPLA_SUPLAN_RESULT_INVALID_ARGUMENT);
        return 1;
      });
  oldSrpc.onSetSuplanSourceAssociation(&source);
  current.onSetSuplanSourceAssociation(&source);
  EXPECT_EQ(peers.size(), 0);
}

// Separate address spaces preserve the real SDK's singleton Channel topology.
// Both children use the production manager and adapter, with an in-memory UDP
// transport only. No Server, YAML peer credentials or PoC profile is involved.
class ProcessDatagrams : public DatagramPort {
 public:
  explicit ProcessDatagrams(int socket) : socket_(socket) {}
  bool sendUnicast(const Endpoint &, const uint8_t *data,
                   size_t size) override {
    return send(socket_, data, size, 0) == static_cast<ssize_t>(size);
  }
  bool sendLocateMulticast(const uint8_t *data, size_t size) override {
    return sendUnicast({}, data, size);
  }
  int pollReceive(uint8_t *buffer, size_t capacity,
                  Endpoint *endpoint) override {
    *endpoint = {0x0100007f, 2017};
    int result = recv(socket_, buffer, capacity, MSG_DONTWAIT);
    // Harness barrier: finish READ delivery before testing credential cleanup.
    if (result == 1 && buffer[0] == 0xfe) {
      readReceived = true;
      return 0;
    }
    return result < 0 ? 0 : result;
  }
  size_t maxDatagramPayload() const override { return 250; }
  uint32_t nowMs() const override { return now; }
  uint32_t now = 0;
  bool readReceived = false;

 private:
  int socket_;
};
void countState(void *context, Supla::Protocol::SupLanApplicationEvent event,
                uint8_t, const ResourceId &resource, uint32_t,
                const uint8_t *payload, size_t size) {
  if (event == Supla::Protocol::kSupLanRemoteState && resource.id == 501 &&
      size == 14 && payload[6] == 1)
    ++*static_cast<int *>(context);
}
int runProvisionedEndpoint(int socket, bool isSource, bool rebootEndpoint) {
  Supla::Channel::resetToDefaults();
  Supla::RegisterDevice::resetToDefaults();
  Supla::Channel channel(7);
  channel.setType(SUPLA_CHANNELTYPE_RELAY);
  channel.setNewValue(true);
  AssociationConfig config;
  ServerIdentity identity;
  identity.load(&config);
  if (!identity.registrationStarted()) return 1;
  identity.registrationSucceeded();
  TSD_SuplaDeviceIdentities snapshot = {};
  snapshot.DeviceId = isSource ? 101 : 202;
  snapshot.ChannelCount = 1;
  snapshot.ChannelId[0] = isSource ? 501 : 601;
  if (identity.accept(snapshot).Result) return 2;
  identity.syncDone();
  PeerTable peers;
  OpenSslCryptoPort crypto;
  ProcessDatagrams datagrams(socket);
  int states = 0;
  Supla::Protocol::SupLan adapter(nullptr, &peers, nullptr, 0, countState,
                                  &states);
  Runtime runtime(&crypto, &datagrams, &adapter, &peers,
                  {kNodeIdDevice, static_cast<uint32_t>(snapshot.DeviceId)},
                  29);
  ServerAssociations associations(&peers, &runtime);
  adapter.attachRuntime(&runtime);
  adapter.attachServerAssociations(&associations);
  associations.load(&config, &identity);
  TDS_SuplaSetSuplanSourceAssociationResult key = {};
  TSDS_SuplaSetSuplanSourceAssociation source = {};
  source.PeerContext = {1, 0, 1, 101, 1, 202, identity.rootEpoch(), 1};
  source.AclRevision = 1;
  source.Flags = 1;
  source.AclEntryCount = 1;
  source.Acl[0] = {1, 501, 1};
  if (isSource) {
    key = associations.accept(source);
    if (key.Result || key.PeerKeySize != 32) return 3;
    if (send(socket, &key, sizeof(key), 0) != sizeof(key)) return 4;
  } else {
    if (recv(socket, &key, sizeof(key), 0) != sizeof(key)) return 5;
    TSDS_SuplaSetSuplanDestinationAssociation request = {};
    request.PeerContext = key.PeerContext;
    request.AclRevision = 1;
    request.PeerKeySize = 32;
    std::memcpy(request.PeerKey, key.PeerKey, 32);
    request.ResourceCount = 1;
    request.Resources[0] = {1, 501, 1};
    if (associations.accept(request).Result) return 6;
  }
  if (rebootEndpoint) {
    associations.load(nullptr, nullptr);
    identity.load(nullptr);
    config.reboot();
    identity.load(&config);
    associations.load(&config, &identity);
  }
  if (!isSource && !runtime.requestRead(0, {1, 501})) return 7;
  for (int step = 0; step < 800; ++step) {
    datagrams.now += 5;
    adapter.iterate(datagrams.now);
    if (!isSource && states && runtime.poolDiagnostics().sessions.used) {
      uint8_t complete = 0xfe;
      if (send(socket, &complete, sizeof(complete), 0) != sizeof(complete))
        return 14;
      return 0;
    }
    if (isSource && datagrams.readReceived &&
        runtime.diagnostics().readDispatched) {
      if (!runtime.poolDiagnostics().sessions.used ||
          !runtime.poolDiagnostics().interests.used)
        return 8;
      source.Flags = 0;
      source.AclRevision = 2;
      source.Acl[0].Permissions = 2;
      if (associations.accept(source).Result ||
          runtime.poolDiagnostics().sessions.used != 1 ||
          runtime.poolDiagnostics().interests.used != 1)
        return 12;
      source.PeerContext.PeerGeneration = 2;
      source.AclRevision = 1;
      source.Acl[0].Permissions = 1;
      if (associations.accept(source).Result ||
          runtime.poolDiagnostics().sessions.used ||
          runtime.poolDiagnostics().interests.used)
        return 13;
      // Reclaim the new generation after clearing REAL session and interest.
      source.Flags = 0;
      source.AclRevision = 2;
      source.AclEntryCount = 0;
      if (associations.accept(source).Result) return 9;
      auto pools = runtime.poolDiagnostics();
      if (pools.peers.used || pools.sessions.used || pools.interests.used ||
          pools.pending.used || pools.retries.used)
        return 10;
      return 0;
    }
    usleep(1000);
  }
  return 11;
}
TEST(SupLanProvisionedRead,
     ProductionAssociationFeedsSessionAndRealChannelRead) {
  for (int rebootMode = 0; rebootMode < 3; ++rebootMode) {
    int sockets[2];
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_DGRAM, 0, sockets), 0);
    timeval timeout = {2, 0};
    for (int socket : sockets) {
      ASSERT_EQ(setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &timeout,
                           sizeof(timeout)),
                0);
    }
    pid_t source = fork();
    ASSERT_GE(source, 0);
    if (source == 0) {
      close(sockets[1]);
      _exit(runProvisionedEndpoint(sockets[0], true, rebootMode == 1));
    }
    pid_t destination = fork();
    ASSERT_GE(destination, 0);
    if (destination == 0) {
      close(sockets[0]);
      _exit(runProvisionedEndpoint(sockets[1], false, rebootMode == 2));
    }
    close(sockets[0]);
    close(sockets[1]);
    int sourceStatus = 0;
    int destinationStatus = 0;
    ASSERT_EQ(waitpid(source, &sourceStatus, 0), source);
    ASSERT_EQ(waitpid(destination, &destinationStatus, 0), destination);
    ASSERT_TRUE(WIFEXITED(sourceStatus));
    ASSERT_TRUE(WIFEXITED(destinationStatus));
    EXPECT_EQ(WEXITSTATUS(sourceStatus), 0) << "reboot mode " << rebootMode;
    EXPECT_EQ(WEXITSTATUS(destinationStatus), 0)
        << "reboot mode " << rebootMode;
  }
}
}  // namespace

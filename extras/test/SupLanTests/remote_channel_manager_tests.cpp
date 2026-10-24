// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later
#include <gtest/gtest.h>
#include <supla/channels/channel.h>
#include <supla/channels/remote_channel_manager.h>
#include <supla/suplan/remote_resource_manager.h>
#include <cstring>
#include <vector>

namespace {
using Supla::RemoteChannelKey;
using Supla::RemoteDelivery;
using Supla::ChannelAccessStatus;
using Supla::Device::ConsumeIntent;
using Supla::Device::RemoteChannelBackend;
using Supla::Device::RemoteChannelManager;
using Supla::Device::RemoteChannelResource;
const RemoteChannelKey peer = {1001, RemoteDelivery::SupLanPeer};
const RemoteChannelKey stream = {1001, RemoteDelivery::ServerStream};

// Mirrors SRPC's existing cancelled-response tombstone: no second request may
// be admitted until the first wire response is drained or connection resets.
class Access : public Supla::Device::RemoteAccessPort {
 public:
  bool ensure(const TDS_SuplaEnsureResourceAccess &request) override {
    if (pending) return false;
    pending = true;
    requests.push_back(request);
    return true;
  }
  void cancelEnsure() override { discarded = pending; }
  void reply(RemoteChannelManager &manager, uint8_t status) {
    ASSERT_TRUE(pending);
    TSD_SuplaEnsureResourceAccessResult result = {};
    result.Result = SUPLA_SUPLAN_RESULT_OK;
    result.DeliveryMode = requests.back().DeliveryMode;
    result.AccessStatus = status;
    pending = false;
    if (!discarded) manager.ensureResult(result);
    discarded = false;
  }
  bool pending = false;
  bool discarded = false;
  std::vector<TDS_SuplaEnsureResourceAccess> requests;
};
class Manager : public RemoteChannelManager {
 public:
  explicit Manager(Access *port) : RemoteChannelManager(port) {}
  void publish(RemoteChannelKey key, double value, uint32_t now) {
    auto resource = findResource(key);
    ASSERT_NE(resource, nullptr);
    ASSERT_TRUE(resource->effective & Supla::SupLan::kPermissionRead);
    TDS_SuplaDeviceChannel_E snapshot = {};
    snapshot.Number = 0xff;
    snapshot.Type = SUPLA_CHANNELTYPE_THERMOMETER;
    snapshot.Default = SUPLA_CHANNELFNC_THERMOMETER;
    snapshot.ValueValidityTimeSec = 1;
    memcpy(snapshot.value, &value, sizeof(value));
    acceptSnapshot(resource, reinterpret_cast<const uint8_t *>(&snapshot),
                   sizeof(snapshot), now);
  }
  void revoke(RemoteChannelResource &resource) { invalidate(&resource); }
  void exhaustGenerations() { nextGeneration_ = UINT32_MAX; }
};
// Test-only storage adapter. It is not an SRPC/STREAM implementation.
class Backend : public RemoteChannelBackend {
 public:
  explicit Backend(Manager *manager, bool localTtl)
      : manager_(manager), localTtl_(localTtl) {}
  bool stableStorage() const override { return localTtl_; }
  int capacity() const override { return 2; }
  int consumerCapacity() const override { return 4; }
  ConsumeIntent intentAt(int i) override {
    return intents[i].intent(localTtl_ ? RemoteDelivery::SupLanPeer
                                     : RemoteDelivery::ServerStream);
  }
  bool storeIntent(int i, const ConsumeIntent &intent) override {
    intents[i].assign(intent);
    return true;
  }
  RemoteChannelResource &resourceAt(int i) override { return resources[i]; }
  void clearResource(int i) override { resources[i] = {}; }
  void reconcileResource(RemoteChannelResource &resource) override {
    ++reconciliations;
    if ((resource.effective & 1) && !(rights & 1)) manager_->revoke(resource);
    resource.effective = rights;
    resource.access = (rights & resource.permissions) == resource.permissions
        ? Supla::Device::RemoteAccess::READY
        : Supla::Device::RemoteAccess::PENDING;
  }
  void releaseResource(RemoteChannelResource &) override { ++releases; }
  void cancelControls(uint32_t owner) override {
    ++cancellations;
    cancelledOwner = owner;
  }
  void refreshResource(RemoteChannelResource &, uint32_t) override {}
  bool fresh(const RemoteChannelResource &resource,
              uint32_t now) const override {
    return !localTtl_ || (now - resource.receivedMs) / 1000 <
                            resource.snapshot.ValueValidityTimeSec;
  }
  bool reachable(const RemoteChannelResource &) const override {
    return online;
  }
  uint8_t rights = 1;
  bool online = true;
  int releases = 0;
  int reconciliations = 0;
  int cancellations = 0;
  uint32_t cancelledOwner = 0;
  Supla::Device::ChannelConsumer intents[4] = {};
  RemoteChannelResource resources[2];

 private:
  Manager *manager_;
  bool localTtl_;
};
ConsumeIntent demand(uint32_t owner, RemoteChannelKey key, uint8_t rights = 1,
                     bool manual = false) {
  return {owner, {Supla::SupLan::kResourceTypeChannel, key.channelId}, rights,
          manual, key.delivery};
}
class RemoteChannelManagement : public testing::Test {
 protected:
  void SetUp() override { Supla::Channel::resetToDefaults(); }
  Access port;
  Manager manager{&port};
  Backend d2d{&manager, true};
  Backend skeleton{&manager, false};
  void attach() {
    ASSERT_TRUE(manager.registerBackend(RemoteDelivery::SupLanPeer, &d2d));
    ASSERT_TRUE(manager.registerBackend(RemoteDelivery::ServerStream,
                                        &skeleton));
  }
};
TEST_F(RemoteChannelManagement,
       AbsentRemovalCancelsControlsWithoutRescanningUnchangedDemand) {
  attach();
  ASSERT_TRUE(manager.consume(demand(1, peer)));
  ASSERT_TRUE(manager.consume(demand(2, peer)));
  ASSERT_TRUE(manager.consume(demand(3, stream)));
  manager.publish(peer, 21, 0);
  manager.publish(stream, 12, 0);
  auto borrowed = manager.state(peer, 0);
  const auto admission = manager.admissionGeneration();
  const int reconciliations = d2d.reconciliations + skeleton.reconciliations;
  const int cancelled = d2d.cancellations + skeleton.cancellations;
  for (int i = 0; i < 34; ++i) manager.remove(UINT32_MAX);
  EXPECT_EQ(d2d.reconciliations + skeleton.reconciliations, reconciliations);
  EXPECT_EQ(d2d.cancellations + skeleton.cancellations, cancelled + 68);
  EXPECT_EQ(d2d.cancelledOwner, UINT32_MAX);
  EXPECT_EQ(skeleton.cancelledOwner, UINT32_MAX);
  EXPECT_EQ(manager.admissionGeneration(), admission);
  EXPECT_EQ(manager.consumerCount(), 3);
  EXPECT_EQ(manager.resourceCount(), 2);
  EXPECT_TRUE(borrowed.available());
  // Revocation still reconciles on the next real use, without any packet.
  d2d.rights = 0;
  EXPECT_FALSE(manager.state(peer, 0).available());
  EXPECT_FALSE(borrowed.available());
  EXPECT_TRUE(manager.state(stream, 0).available());
  manager.remove(1);
  EXPECT_EQ(manager.consumerCount(), 2);
  EXPECT_EQ(manager.resourceCount(), 2);
  manager.remove(2);
  EXPECT_EQ(manager.resourceCount(), 1);
  EXPECT_EQ(d2d.releases, 1);
  manager.remove(3);
  EXPECT_EQ(manager.resourceCount(), 0);
}

TEST_F(RemoteChannelManagement,
       SameKeySharesSnapshotDifferentModesStaySeparate) {
  attach();
  ASSERT_TRUE(manager.consume(demand(1, peer)));
  ASSERT_TRUE(manager.consume(demand(2, peer, 2)));
  ASSERT_TRUE(manager.consume(demand(3, stream)));
  EXPECT_EQ(manager.resourceCount(), 2);
  EXPECT_EQ(manager.consumerCount(), 3);
  manager.publish(peer, 21, 0);
  manager.publish(stream, 12, 0);
  EXPECT_DOUBLE_EQ(manager.state(peer, 0).temperature(), 21);
  EXPECT_DOUBLE_EQ(manager.state(stream, 0).temperature(), 12);
  EXPECT_EQ(manager.effectivePermissions(peer), 1);
  EXPECT_EQ(manager.status(peer, 2), ChannelAccessStatus::PROVISIONING);
  EXPECT_EQ(manager.status(stream, 1), ChannelAccessStatus::AUTHORIZED);
  EXPECT_FALSE(manager.state(peer, 1000).available());
  EXPECT_TRUE(manager.state(stream, 1000).available());
  d2d.online = false;
  EXPECT_EQ(manager.status(peer, 1), ChannelAccessStatus::AUTHORIZED);
  EXPECT_EQ(manager.status(stream, 1), ChannelAccessStatus::AUTHORIZED);
  manager.remove(1);
  EXPECT_EQ(manager.resourceCount(), 2);
  EXPECT_DOUBLE_EQ(manager.state(peer, 0).temperature(), 21);
  manager.remove(2);
  EXPECT_EQ(manager.resourceCount(), 1);
  EXPECT_EQ(d2d.releases, 1);
  EXPECT_DOUBLE_EQ(manager.state(stream, 0).temperature(), 12);
  manager.remove(3);
  EXPECT_EQ(manager.resourceCount(), 0);
}
TEST_F(RemoteChannelManagement, AuthorizationDoesNotRequireSessionOrSnapshot) {
  attach();
  d2d.rights = 3;
  ASSERT_TRUE(manager.consume(demand(1, peer, 2)));
  ASSERT_TRUE(manager.consume(demand(2, stream)));
  for (bool online : {false, true, false}) {
    d2d.online = skeleton.online = online;
    EXPECT_EQ(manager.transportAvailable(peer), online);
    EXPECT_EQ(manager.transportAvailable(stream), online);
    EXPECT_EQ(manager.status(peer, 2), ChannelAccessStatus::AUTHORIZED);
    EXPECT_EQ(manager.status(stream, 1), ChannelAccessStatus::AUTHORIZED);
    EXPECT_FALSE(manager.state(peer, 0).available());
    EXPECT_FALSE(manager.state(stream, 0).available());
  }
  manager.publish(peer, 21, 0);
  EXPECT_TRUE(manager.state(peer, 0).available());
  EXPECT_FALSE(manager.state(peer, 1000).available());
  EXPECT_EQ(manager.status(peer, 2), ChannelAccessStatus::AUTHORIZED);
  // Removing CONTROL immediately changes status despite the retained READ.
  d2d.rights = 1;
  EXPECT_EQ(manager.status(peer, 2), ChannelAccessStatus::PROVISIONING);
  EXPECT_EQ(manager.status(peer, 1), ChannelAccessStatus::AUTHORIZED);
}
TEST_F(RemoteChannelManagement, MissingStreamBackendNeverAllocatesOrEnsures) {
  ASSERT_TRUE(manager.registerBackend(RemoteDelivery::SupLanPeer, &d2d));
  EXPECT_FALSE(manager.supports(RemoteDelivery::ServerStream));
  EXPECT_FALSE(manager.consume(demand(1, stream, 1, true)));
  manager.iterate(0);
  EXPECT_EQ(manager.consumerCount(), 0);
  EXPECT_EQ(manager.resourceCount(), 0);
  EXPECT_TRUE(port.requests.empty());
  EXPECT_EQ(manager.status(stream, 1), ChannelAccessStatus::INVALID);
  EXPECT_FALSE(manager.state(stream, 0).available());
  Supla::SupLan::PeerTable peers;
  Supla::Device::RemoteResourceManager production(&peers, nullptr, &port);
  EXPECT_FALSE(production.supports(RemoteDelivery::ServerStream));
  EXPECT_FALSE(production.consume(demand(2, stream)));
  EXPECT_EQ(production.resourceCount(), 0);
}
TEST_F(RemoteChannelManagement, SerializedEnsureKeepsKeyAndExplicitMode) {
  attach();
  d2d.rights = skeleton.rights = 0;
  ASSERT_TRUE(manager.consume(demand(1, peer, 1, true)));
  ASSERT_TRUE(manager.consume(demand(2, stream, 1, true)));
  manager.iterate(0);
  ASSERT_EQ(port.requests.size(), 1U);
  const auto first = port.requests.back();
  TSD_SuplaEnsureResourceAccessResult wrong = {};
  wrong.Result = SUPLA_SUPLAN_RESULT_OK;
  wrong.AccessStatus = SUPLA_SUPLAN_ACCESS_STATUS_REJECTED;
  wrong.DeliveryMode = first.DeliveryMode == 1 ? 2 : 1;
  manager.ensureResult(wrong);
  EXPECT_EQ(manager.approvalStatus(peer), SUPLA_SUPLAN_ACCESS_STATUS_INVALID);
  EXPECT_EQ(manager.approvalStatus(stream), SUPLA_SUPLAN_ACCESS_STATUS_INVALID);
  manager.iterate(1);
  EXPECT_EQ(port.requests.size(), 1U);
  port.reply(manager, SUPLA_SUPLAN_ACCESS_STATUS_PENDING_APPROVAL);
  manager.iterate(2);
  ASSERT_EQ(port.requests.size(), 2U);
  EXPECT_NE(port.requests.back().DeliveryMode, first.DeliveryMode);
  EXPECT_EQ(port.requests.back().Resource.ResourceId, 1001);
  port.reply(manager, SUPLA_SUPLAN_ACCESS_STATUS_REJECTED);
  EXPECT_NE(manager.approvalStatus(peer), manager.approvalStatus(stream));
}
TEST_F(RemoteChannelManagement, CancelledEnsureDrainsBeforeReusedKeyAdmission) {
  attach();
  d2d.rights = 0;
  ASSERT_TRUE(manager.consume(demand(1, peer, 1, true)));
  manager.iterate(0);
  ASSERT_EQ(port.requests.size(), 1U);
  manager.remove(1);
  ASSERT_TRUE(port.discarded);
  ASSERT_TRUE(manager.consume(demand(2, peer, 1, true)));
  manager.iterate(1);
  EXPECT_EQ(port.requests.size(), 1U);
  port.reply(manager, SUPLA_SUPLAN_ACCESS_STATUS_REJECTED);
  EXPECT_EQ(manager.approvalStatus(peer), SUPLA_SUPLAN_ACCESS_STATUS_INVALID);
  manager.iterate(1001);
  ASSERT_EQ(port.requests.size(), 2U);
  port.reply(manager, SUPLA_SUPLAN_ACCESS_STATUS_GRANTED);
  EXPECT_EQ(manager.approvalStatus(peer), SUPLA_SUPLAN_ACCESS_STATUS_GRANTED);
  EXPECT_EQ(manager.effectivePermissions(peer), 0);
  EXPECT_FALSE(manager.state(peer, 1001).available());
}
TEST_F(RemoteChannelManagement,
       BorrowedViewCannotAliasReclaimedSlotOrRevocation) {
  attach();
  ASSERT_TRUE(manager.consume(demand(1, peer)));
  manager.publish(peer, 21, 0);
  const auto borrowed = manager.state(peer, 0);
  ASSERT_TRUE(borrowed.available());
  manager.remove(1);
  const RemoteChannelKey replacement = {2002, RemoteDelivery::SupLanPeer};
  ASSERT_TRUE(manager.consume(demand(2, replacement)));
  manager.publish(replacement, 12, 0);
  EXPECT_FALSE(borrowed.available());
  EXPECT_EQ(borrowed.type(), 0U);
  uint8_t bytes[8] = {};
  EXPECT_FALSE(borrowed.value(bytes));
  const auto live = manager.state(replacement, 0);
  d2d.rights = 0;
  EXPECT_FALSE(manager.state(replacement, 0).available());
  EXPECT_FALSE(live.available());
}
TEST_F(RemoteChannelManagement,
       IndependentCapacitiesNoEvictionAndSharedConsumers) {
  attach();
  ASSERT_TRUE(manager.consume(demand(1, peer)));
  ASSERT_TRUE(manager.consume(demand(2, peer)));
  ASSERT_TRUE(manager.consume(demand(3, peer)));
  EXPECT_EQ(manager.resourceCount(), 1);
  const RemoteChannelKey second = {2002, RemoteDelivery::SupLanPeer};
  ASSERT_TRUE(manager.consume(demand(4, second)));
  EXPECT_FALSE(manager.consume(demand(5, {3003, RemoteDelivery::SupLanPeer})));
  ASSERT_TRUE(manager.consume(demand(6, stream)));
  EXPECT_EQ(manager.resourceCount(), 3);
  manager.remove(4);
  EXPECT_TRUE(manager.consume(demand(5, {3003, RemoteDelivery::SupLanPeer})));
  EXPECT_EQ(manager.consumerCount(), 5);
  EXPECT_EQ(manager.resourceCount(), 3);
}
TEST_F(RemoteChannelManagement, SkeletonRejectsStreamControlAndActionDemand) {
  attach();
  EXPECT_FALSE(manager.consume(demand(1, stream, 2, true)));
  EXPECT_FALSE(manager.consume(demand(1, stream, 4, true)));
  EXPECT_EQ(manager.consumerCount(), 0);
  manager.iterate(0);
  EXPECT_TRUE(port.requests.empty());
}
TEST_F(RemoteChannelManagement, BackendStorageCannotBeAliasedAcrossModes) {
  ASSERT_TRUE(manager.registerBackend(RemoteDelivery::SupLanPeer, &d2d));
  EXPECT_FALSE(manager.registerBackend(RemoteDelivery::ServerStream, &d2d));
  EXPECT_FALSE(manager.supports(RemoteDelivery::ServerStream));
}
TEST_F(RemoteChannelManagement, BackendDetachPreservesOtherModeAndFencesViews) {
  attach();
  ASSERT_TRUE(manager.consume(demand(1, peer)));
  ASSERT_TRUE(manager.consume(demand(2, stream)));
  manager.publish(peer, 21, 0);
  manager.publish(stream, 12, 0);
  const auto old = manager.state(peer, 0);
  manager.unregisterBackend(RemoteDelivery::SupLanPeer);
  EXPECT_FALSE(old.available());
  EXPECT_FALSE(manager.hasConsumer(1));
  EXPECT_TRUE(manager.hasConsumer(2));
  EXPECT_EQ(manager.resourceCount(), 1);
  EXPECT_DOUBLE_EQ(manager.state(stream, 0).temperature(), 12);
}
TEST_F(RemoteChannelManagement, MixedRightsAggregateWithIndependentApproval) {
  attach();
  ASSERT_TRUE(manager.consume(demand(1, peer, 1)));
  ASSERT_TRUE(manager.consume(demand(2, peer, 2, true)));
  ASSERT_TRUE(manager.consume(demand(3, peer, 4)));
  manager.publish(peer, 21, 0);
  manager.iterate(0);
  ASSERT_EQ(port.requests.size(), 1U);
  EXPECT_EQ(port.requests.front().Permissions, 3);
  EXPECT_EQ(port.requests.front().Flags,
            SUPLA_SUPLAN_ENSURE_ACCESS_FLAG_ALLOW_APPROVAL);
  port.reply(manager, SUPLA_SUPLAN_ACCESS_STATUS_REJECTED);
  EXPECT_EQ(manager.status(peer, 2), ChannelAccessStatus::REJECTED);
  EXPECT_DOUBLE_EQ(manager.state(peer, 0).temperature(), 21);
  manager.remove(2);
  manager.iterate(1);
  ASSERT_EQ(port.requests.size(), 2U);
  EXPECT_EQ(port.requests.back().Permissions, 5);
  EXPECT_EQ(port.requests.back().Flags, 0);
  EXPECT_EQ(manager.resourceCount(), 1);
}
TEST_F(RemoteChannelManagement, ZeroOwnerCannotAlterLiveAdmissionOrCache) {
  attach();
  ASSERT_TRUE(manager.consume(demand(1, peer)));
  manager.publish(peer, 21, 0);
  const auto generation = manager.admissionGeneration();
  const auto view = manager.state(peer, 0);
  EXPECT_FALSE(manager.hasConsumer(0));
  manager.remove(0);
  EXPECT_EQ(manager.admissionGeneration(), generation);
  EXPECT_TRUE(manager.hasConsumer(1));
  EXPECT_DOUBLE_EQ(view.temperature(), 21);
  EXPECT_EQ(manager.consumerCount(), 1);
}
TEST_F(RemoteChannelManagement, FullOwnerAndChannelDomainsSurviveModeReentry) {
  attach();
  const RemoteChannelKey largest = {INT32_MAX, RemoteDelivery::SupLanPeer};
  ASSERT_TRUE(manager.consume(demand(UINT32_MAX, largest, 7, true)));
  ASSERT_TRUE(manager.consume(demand(0x7fffffffU, largest, 1, false)));
  EXPECT_EQ(manager.consumerCount(), 2);
  EXPECT_EQ(manager.resourceCount(), 1);
  auto invalid = demand(UINT32_MAX, largest);
  for (uint8_t type : {0, 2, 255}) {
    invalid.resource.type = type;
    EXPECT_FALSE(manager.consume(invalid));
    EXPECT_TRUE(manager.hasConsumer(UINT32_MAX));
    EXPECT_EQ(manager.resourceCount(), 1);
  }
  EXPECT_FALSE(manager.consume(demand(UINT32_MAX,
      {UINT32_MAX, RemoteDelivery::SupLanPeer})));
  ASSERT_TRUE(manager.consume(demand(UINT32_MAX, stream, 1, true)));
  EXPECT_EQ(manager.consumerCount(), 2);
  EXPECT_EQ(manager.resourceCount(), 2);
  manager.remove(0x7fffffffU);
  EXPECT_EQ(manager.resourceCount(), 1);
  manager.remove(UINT32_MAX);
  EXPECT_EQ(manager.consumerCount(), 0);
  EXPECT_EQ(manager.resourceCount(), 0);
  EXPECT_TRUE(manager.consume(demand(UINT32_MAX, largest, 4, false)));
}
TEST_F(RemoteChannelManagement, ExhaustedGenerationNeverReusesFreedResource) {
  attach();
  ASSERT_TRUE(manager.consume(demand(UINT32_MAX, peer)));
  manager.publish(peer, 21, 0);
  const auto old = manager.state(peer, 0);
  manager.exhaustGenerations();
  manager.remove(UINT32_MAX);
  EXPECT_FALSE(old.available());
  EXPECT_FALSE(manager.consume(demand(UINT32_MAX, peer)));
  EXPECT_FALSE(manager.consume(demand(1, stream)));
  EXPECT_EQ(manager.resourceCount(), 0);
  EXPECT_EQ(manager.consumerCount(), 0);
}
TEST(RemoteConsumerCapacity, IndependentAdmissionAndReclamation) {
  Supla::SupLan::PeerTable peers;
  Supla::Device::RemoteResourceManager manager(&peers, nullptr, nullptr);
  const int consumers = Supla::Device::RemoteResourceManager::kConsumerCapacity;
  const int resources = Supla::Device::RemoteResourceManager::kCapacity;
  for (int i = 0; i < consumers; ++i)
    ASSERT_TRUE(manager.consume(demand(0x90000000U + i, peer)));
  EXPECT_EQ(manager.consumerCount(), consumers);
  EXPECT_EQ(manager.resourceCount(), 1);
  EXPECT_FALSE(manager.consume(demand(UINT32_MAX, peer)));
  manager.remove(0x90000000U + consumers / 2);
  EXPECT_TRUE(manager.consume(demand(UINT32_MAX, peer)));
  for (int i = 0; i < consumers; ++i) manager.remove(0x90000000U + i);
  manager.remove(UINT32_MAX);
  EXPECT_EQ(manager.resourceCount(), 0);
  const int count = consumers < resources ? consumers : resources;
  for (int i = 0; i < count; ++i)
    ASSERT_TRUE(manager.consume(demand(1U + i,
        {1001U + i, RemoteDelivery::SupLanPeer})));
  EXPECT_EQ(manager.resourceCount(), count);
  EXPECT_FALSE(manager.consume(demand(UINT32_MAX,
      {1001U + count, RemoteDelivery::SupLanPeer})));
  manager.remove(count);
  EXPECT_TRUE(manager.consume(demand(UINT32_MAX,
      {1001U + count, RemoteDelivery::SupLanPeer})));
}
}  // namespace

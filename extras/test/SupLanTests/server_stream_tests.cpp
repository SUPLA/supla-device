// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later
#include <SuplaDevice.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <network_client_mock.h>
#include <simple_time.h>
#include <srpc_mock.h>
#include <supla/channels/server_stream_backend.h>
#include <supla/channels/channel.h>
#include <supla/protocol/supla_srpc.h>
#include <supla/device/register_device.h>
#include <supla/events.h>
#include <supla/element.h>
#include <supla/storage/key_value.h>
#include <supla/suplan/owned_stack.h>
#include <suplan_bootstrap_port.h>
#include <cstring>
#include <memory>
#include <utility>
#include <vector>
#include "suplan_crypto_openssl.h"

namespace {
using namespace Supla;  // NOLINT(build/namespaces)
using ::testing::_;
using ::testing::Return;
class StreamConfig : public KeyValue {
 public:
  bool init() override { return true; }
  bool commit() override { return true; }
};
class StreamDevice : public SuplaDeviceClass {
 public:
  void use(Protocol::SuplaSrpc *protocol) { srpcLayer = protocol; }
};
class StreamSrpc : public Protocol::SuplaSrpc {
 public:
  explicit StreamSrpc(SuplaDeviceClass *device) : SuplaSrpc(device, 29) {}
  void connected(void *handle) { srpc = handle; registered = 1; }
  void lost() { deinitializeSrpc(); registered = 0; }
  ~StreamSrpc() override { srpc = nullptr; }
};
using Stack = Device::OwnedSupLanStack<SupLan::OpenSslCryptoPort, BootstrapUdp>;
class ServerStream : public testing::Test {
 protected:
  ServerStream() : protocol(&device) {
    Channel::resetToDefaults();
    RegisterDevice::resetToDefaults();
    local.reset(new Channel(0));
    device.use(&protocol);
    client = new testing::NiceMock<NetworkClientMock>;
    ON_CALL(*client, connected()).WillByDefault(Return(1));
    ON_CALL(*client, isSecure()).WillByDefault(Return(true));
    protocol.setNetworkClient(client);
    protocol.connected(&transport);
    stack.reset(new Stack(&device, 29));
    EXPECT_TRUE(stack->initialize());
    for (auto layer = Protocol::ProtocolLayer::first(); layer;
         layer = layer->next()) {
      if (layer->getSdc() == &device && layer->isSupLan())
        lan = static_cast<Protocol::SupLan *>(layer);
    }
    EXPECT_NE(lan, nullptr);
    if (lan) lan->onInit();
    bootstrap();
    ON_CALL(wire, ensureResourceAccess(&transport, _))
        .WillByDefault([&](void *, TDS_SuplaEnsureResourceAccess *request) {
          requests.push_back(*request);
          return 1;
        });
  }
  ~ServerStream() override {
    stack.reset();
    device.use(nullptr);
  }
  void bootstrap() {
    auto &identity = protocol.serverIdentity();
    identity.load(&storage);
    ASSERT_TRUE(identity.registrationStarted());
    identity.registrationSucceeded();
    TSD_SuplaDeviceIdentities ids = {};
    ids.DeviceId = 202;
    ids.ChannelCount = 1;
    ids.ChannelId[0] = 601;
    ASSERT_EQ(identity.accept(ids).Result, SUPLA_SUPLAN_RESULT_OK);
    identity.syncDone();
  }
  Device::RemoteResourceManager *manager() { return lan->remoteResources(); }
  void tick(uint32_t ms = 0) {
    if (ms) time.advance(ms);
    ChannelHandle::iterateAll();
    manager()->iterate(time.value);
  }
  void dispatch(int call, void *data, void *connection = nullptr,
                uint8_t version = 29) {
    if (!connection) connection = &transport;
    EXPECT_CALL(wire, srpc_getdata(connection, _, 0))
        .WillOnce([&](void *, TsrpcReceivedData *rd, unsigned int) {
          *rd = {};
          rd->call_id = call;
          if (call == SUPLA_SD_CALL_REMOTE_CHANNEL_STATE)
            rd->data.sd_remote_channel_state =
                static_cast<TSD_SuplaRemoteChannelState *>(data);
          else
            rd->data.sd_ensure_suplan_resource_access_result =
                static_cast<TSD_SuplaEnsureResourceAccessResult *>(data);
          return SUPLA_RESULT_TRUE;
        });
    Supla::messageReceived(connection, 19, 0, &protocol, version);
  }
  void reply(uint8_t status = SUPLA_SUPLAN_ACCESS_STATUS_GRANTED) {
    TSD_SuplaEnsureResourceAccessResult result = {};
    result.Result = SUPLA_SUPLAN_RESULT_OK;
    result.AccessStatus = status;
    result.DeliveryMode = requests.back().DeliveryMode;
    dispatch(SUPLA_SD_CALL_ENSURE_SUPLAN_RESOURCE_ACCESS_RESULT, &result);
  }
  TSD_SuplaRemoteChannelState snapshot(int id = 1001, double value = 21) {
    TSD_SuplaRemoteChannelState state = {};
    state.ChannelId = id;
    state.Channel.Number = 0xff;
    state.Channel.Type = SUPLA_CHANNELTYPE_THERMOMETER;
    state.Channel.Default = SUPLA_CHANNELFNC_THERMOMETER;
    state.Channel.ValueValidityTimeSec = 1;
    memcpy(state.Channel.value, &value, sizeof(value));
    return state;
  }
  void publish(TSD_SuplaRemoteChannelState state) {
    dispatch(SUPLA_SD_CALL_REMOTE_CHANNEL_STATE, &state);
  }
  SimpleTime time;
  StreamConfig storage;
  testing::NiceMock<SrpcMock> wire;
  StreamDevice device;
  StreamSrpc protocol;
  int transport = 0;
  testing::NiceMock<NetworkClientMock> *client;
  std::unique_ptr<Channel> local;
  std::unique_ptr<Stack> stack;
  Protocol::SupLan *lan = nullptr;
  std::vector<TDS_SuplaEnsureResourceAccess> requests;
};
TEST_F(ServerStream, ExplicitReadOnlySharedSnapshotWithoutUdpOrPeer) {
  EXPECT_EQ(protocol.serverStreamAllocatedBytes(), 0U);
  ServerRemoteChannel a(1001), b(1001);
  tick();
  ASSERT_EQ(requests.size(), 1U);
  EXPECT_EQ(requests[0].DeliveryMode, SUPLA_RESOURCE_DELIVERY_SERVER_STREAM);
  EXPECT_EQ(requests[0].Permissions, SUPLA_SUPLAN_PERMISSION_READ);
  EXPECT_EQ(requests[0].Flags, SUPLA_SUPLAN_ENSURE_ACCESS_FLAG_ALLOW_APPROVAL);
  EXPECT_FALSE(a.canRead());
  reply();
  EXPECT_FALSE(a.state().available());
  publish(snapshot());
  EXPECT_TRUE(a.canRead());
  EXPECT_DOUBLE_EQ(a.state().temperature(), 21);
  EXPECT_DOUBLE_EQ(b.state().temperature(), 21);
  EXPECT_EQ(manager()->resourceCount(), 1);
  EXPECT_EQ(manager()->consumerCount(), 2);
  EXPECT_FALSE(a.canControl());
  uint8_t value[8] = {};
  EXPECT_FALSE(a.setValue(value));
  EXPECT_FALSE(a.executeAction(ACTION_TOGGLE));
  EXPECT_FALSE(a.configure(1001, CONTROL));
  tick(60000);
  EXPECT_TRUE(a.state().available());  // Server, not Device, expires the value.
  EXPECT_EQ(requests.size(), 1U);
  auto offline = snapshot();
  offline.Channel.Offline = 1;
  publish(offline);
  EXPECT_FALSE(a.state().available());
  publish(snapshot(1001, 23));
  EXPECT_DOUBLE_EQ(a.state().temperature(), 23);
}
TEST_F(ServerStream, NonTlsTransportCannotSendStreamEnsure) {
  ServerRemoteChannel channel(1001);
  ChannelHandle::iterateAll();
  ASSERT_EQ(manager()->consumerCount(), 1);
  ON_CALL(*client, isSecure()).WillByDefault(Return(false));
  tick();
  EXPECT_TRUE(requests.empty());
  EXPECT_FALSE(channel.canRead());
  EXPECT_FALSE(channel.state().available());
  ON_CALL(*client, isSecure()).WillByDefault(Return(true));
  tick(1000);  // Existing failed-ENSURE backoff, no immediate retry loop.
  EXPECT_EQ(requests.size(), 1u);
}
TEST_F(ServerStream, SnapshotBeforeResultIsHiddenAndPendingActivatesByPush) {
  ServerRemoteChannel a(1001);
  tick();
  publish(snapshot());
  EXPECT_FALSE(a.canRead());
  EXPECT_FALSE(a.state().available());
  reply(SUPLA_SUPLAN_ACCESS_STATUS_PENDING_APPROVAL);
  EXPECT_EQ(a.accessStatus(), ChannelAccessStatus::PENDING_APPROVAL);
  EXPECT_FALSE(a.state().available());
  publish(snapshot(1001, 22));
  EXPECT_TRUE(a.canRead());
  EXPECT_DOUBLE_EQ(a.state().temperature(), 22);
  EXPECT_EQ(requests.size(), 1U);
}
TEST_F(ServerStream, InitialSnapshotAdjacentToGrantedResult) {
  ServerRemoteChannel a(1001);
  tick();
  publish(snapshot());
  reply();
  EXPECT_DOUBLE_EQ(a.state().temperature(), 21);
}
TEST_F(ServerStream, RejectedUnsolicitedAndMalformedCannotExposeState) {
  publish(snapshot());
  EXPECT_EQ(protocol.serverStreamAllocatedBytes(), 0U);
  ServerRemoteChannel a(1001);
  tick();
  reply(SUPLA_SUPLAN_ACCESS_STATUS_REJECTED);
  publish(snapshot());
  EXPECT_FALSE(a.canRead());
  EXPECT_EQ(a.accessStatus(), ChannelAccessStatus::REJECTED);
  EXPECT_FALSE(protocol.onRemoteChannelState(nullptr));
  a.configure(1002, READ);
  tick();
  reply();
  publish(snapshot());  // Old ChannelId.
  auto malformed = snapshot(1002);
  malformed.Channel.Number = 1;
  EXPECT_FALSE(protocol.onRemoteChannelState(&malformed));
  malformed.Channel.Number = 0xff;
  malformed.Channel.Offline = 255;
  EXPECT_FALSE(protocol.onRemoteChannelState(&malformed));
  malformed.Channel.Offline = 0;
  malformed.Channel.Type = 0;
  EXPECT_FALSE(protocol.onRemoteChannelState(&malformed));
  EXPECT_FALSE(a.state().available());
}
TEST_F(ServerStream, DynamicLifetimeFreesCacheAndFencesBorrowedView) {
  size_t emptyBytes = 0;
  for (int i = 0; i < 200; ++i) {
    auto a = new ServerRemoteChannel(1001);
    tick(i ? 1000 : 0);
    reply();
    publish(snapshot());
    const auto borrowed = a->state();
    ASSERT_TRUE(borrowed.available());
    delete a;
    EXPECT_FALSE(borrowed.available());
    EXPECT_EQ(manager()->resourceCount(), 0);
    if (!i) emptyBytes = protocol.serverStreamAllocatedBytes();
    EXPECT_EQ(protocol.serverStreamAllocatedBytes(), emptyBytes);
  }
}
TEST_F(ServerStream, MovedAndCopiedViewsNeverAcceptRecycledStreamNodes) {
  for (int i = 0; i < 64; ++i) {
    std::unique_ptr<ServerRemoteChannel> handle(new ServerRemoteChannel(1001));
    tick(i ? 1000 : 0);
    reply();
    publish(snapshot());
    auto original = handle->state();
    auto moved = std::move(original);
    ChannelState assigned;
    assigned = std::move(moved);
    ASSERT_TRUE(assigned.available());
    handle.reset();
    EXPECT_FALSE(original.available());
    EXPECT_FALSE(moved.available());
    EXPECT_FALSE(assigned.available());
    ServerRemoteChannel replacement(1001);
    tick(1000);
    reply();
    publish(snapshot(1001, 24));
    EXPECT_TRUE(replacement.state().available());
    EXPECT_FALSE(assigned.available());
    EXPECT_EQ(assigned.type(), 0U);
  }
}
TEST_F(ServerStream, ManagerDestructionDetachesBackendAndLiveHandles) {
  ServerRemoteChannel handle(1001);
  tick();
  reply();
  publish(snapshot());
  const auto borrowed = handle.state();
  ASSERT_TRUE(borrowed.available());
  stack.reset();
  EXPECT_FALSE(handle.state().available());
  // Borrowed views require their manager to outlive method calls. Their
  // destruction itself performs no lookup and is safe after manager teardown.
  // SRPC's detached backend and the handle also remain safely destructible.
}
TEST_F(ServerStream, CapabilityVersionAndIdentityAreMandatory) {
  EXPECT_TRUE(protocol.serverStreamReady());
  protocol.setVersion(28);
  EXPECT_FALSE(protocol.serverStreamReady());
  ServerRemoteChannel a(1001);
  tick();
  EXPECT_TRUE(requests.empty());
  protocol.setVersion(29);
  bootstrap();
  manager()->serverReconnected();
  tick();
  ASSERT_EQ(requests.size(), 1U);
  reply();
  publish(snapshot());
  EXPECT_TRUE(a.canRead());
  protocol.serverIdentity().disconnected();
  EXPECT_FALSE(a.state().available());
}
TEST_F(ServerStream, DisconnectFencesSnapshotAndReconstructsAfterSync) {
  ServerRemoteChannel a(1001);
  tick();
  reply();
  publish(snapshot());
  auto borrowed = a.state();
  protocol.lost();
  EXPECT_FALSE(borrowed.available());
  EXPECT_FALSE(a.state().available());
  auto oldState = snapshot();
  EXPECT_FALSE(protocol.onRemoteChannelState(&oldState));
  int old = 1;
  EXPECT_CALL(wire, srpc_getdata(&old, _, _)).Times(0);
  Supla::messageReceived(&old, 19, 0, &protocol, 29);
  protocol.connected(&transport);
  bootstrap();
  manager()->serverReconnected();
  tick(1000);
  ASSERT_EQ(requests.size(), 2U);
  reply();
  EXPECT_FALSE(a.state().available());
  publish(snapshot(1001, 24));
  EXPECT_DOUBLE_EQ(a.state().temperature(), 24);
  EXPECT_FALSE(borrowed.available());
}

TEST_F(ServerStream, BoundedCapacityReleasesAndReadmitsWithoutEviction) {
  const int capacity = SUPLA_SERVER_STREAM_MAX_CHANNELS;
  std::vector<std::unique_ptr<ServerRemoteChannel>> handles;
  for (int i = 0; i < capacity; ++i) {
    handles.emplace_back(new ServerRemoteChannel(1001 + i));
    tick();
    ASSERT_EQ(requests.size(), static_cast<size_t>(i + 1));
    reply();
    publish(snapshot(1001 + i, i + 1));
  }
  EXPECT_EQ(manager()->resourceCount(), capacity);
  ServerRemoteChannel overflow(1001 + capacity);
  tick();
  EXPECT_EQ(overflow.accessStatus(), ChannelAccessStatus::RESOURCE_LIMIT);
  EXPECT_EQ(requests.size(), static_cast<size_t>(capacity));
  for (int i = 0; i < capacity; ++i)
    EXPECT_DOUBLE_EQ(handles[i]->state().temperature(), i + 1);
  auto stale = handles[capacity / 2]->state();
  handles[capacity / 2].reset();
  EXPECT_FALSE(stale.available());
  tick();
  ASSERT_EQ(requests.size(), static_cast<size_t>(capacity + 1));
  reply();
  publish(snapshot(1001 + capacity, 42));
  EXPECT_DOUBLE_EQ(overflow.state().temperature(), 42);
}
TEST_F(ServerStream, SameChannelHasIndependentD2dAndStreamFailureDomains) {
  using namespace Supla::SupLan;  // NOLINT(build/namespaces)
  TSDS_SuplaSetSuplanDestinationAssociation request = {};
  request.PeerContext = {1, 0, 1, 101, 1, 202, 7, 1};
  request.AclRevision = 1;
  request.PeerKeySize = 32;
  memset(request.PeerKey, 0x42, 32);
  request.ResourceCount = 1;
  request.Resources[0] = {1, 1001, 1};
  TDS_SuplaSetSuplanDestinationAssociationResult result = {};
  ASSERT_TRUE(lan->setSuplanDestinationAssociation(request, &result));
  ASSERT_EQ(result.Result, SUPLA_SUPLAN_RESULT_OK);
  ResourceId id = {kResourceTypeChannel, 1001};
  ASSERT_TRUE(manager()->consume({1, id, kPermissionRead}));
  auto state = snapshot(1001, 17);
  manager()->receive(0, id, reinterpret_cast<uint8_t *>(&state.Channel),
                     sizeof(state.Channel), 0);
  ServerRemoteChannel streamed(1001);
  tick();
  reply();
  publish(snapshot(1001, 25));
  EXPECT_DOUBLE_EQ(manager()->state(id, 0).temperature(), 17);
  EXPECT_DOUBLE_EQ(streamed.state().temperature(), 25);
  protocol.lost();
  EXPECT_FALSE(streamed.state().available());
  EXPECT_DOUBLE_EQ(manager()->state(id, 0).temperature(), 17);
}
TEST_F(ServerStream, UdpUnavailableDoesNotBlockOrCreatePeerDemand) {
  ServerRemoteChannel a(1001);
  ChannelHandle::iterateAll();
  lan->iterate(0);  // No network address/UDP socket in this harness.
  ASSERT_EQ(requests.size(), 1U);
  reply();
  publish(snapshot());
  EXPECT_TRUE(a.state().available());
}
TEST_F(ServerStream, UnflaggedDestinationCannotRequestOrAcceptStream) {
  stack.reset();
  lan = nullptr;
  EXPECT_FALSE(protocol.serverStreamReady());
  TDS_SuplaEnsureResourceAccess request = {};
  request.Resource = {SUPLA_SUPLAN_RESOURCE_TYPE_CHANNEL, 1001};
  request.Permissions = SUPLA_SUPLAN_PERMISSION_READ;
  request.DeliveryMode = SUPLA_RESOURCE_DELIVERY_SERVER_STREAM;
  EXPECT_FALSE(protocol.ensureResourceAccess(request));
  auto state = snapshot();
  EXPECT_FALSE(protocol.onRemoteChannelState(&state));
  EXPECT_TRUE(requests.empty());
}
TEST_F(ServerStream, NoFakeEventsOnOfflineReconnectOrFirstSnapshot) {
  class Counter : public ActionHandler {
   public:
    void handleAction(int, int) override { ++count; }
    int count = 0;
  } counter;
  ServerRemoteChannel a(1001);
  a.addAction(1, counter, ON_CHANGE);
  tick();
  reply();
  publish(snapshot());
  ChannelHandle::iterateAll();
  EXPECT_EQ(counter.count, 0);
  publish(snapshot(1001, 22));
  ChannelHandle::iterateAll();
  EXPECT_EQ(counter.count, 1);
  auto unavailable = snapshot(1001, 0);
  unavailable.Channel.Offline = 1;
  publish(unavailable);
  ChannelHandle::iterateAll();
  EXPECT_EQ(counter.count, 1);
  publish(snapshot(1001, 23));
  ChannelHandle::iterateAll();
  EXPECT_EQ(counter.count, 1);
  ON_CALL(*client, connected()).WillByDefault(Return(0));
  auto view = a.state();
  EXPECT_FALSE(view.available());
  ChannelHandle::iterateAll();
  EXPECT_EQ(counter.count, 1);
}
TEST_F(ServerStream, LostConnectionInvalidatesAnAlreadyBorrowedView) {
  ServerRemoteChannel a(1001);
  tick();
  reply();
  publish(snapshot());
  auto view = a.state();
  ASSERT_TRUE(view.available());
  ON_CALL(*client, connected()).WillByDefault(Return(0));
  EXPECT_FALSE(view.available());
  EXPECT_FALSE(a.canRead());
}

TEST_F(ServerStream, ReconcileOnlyDemandNeverRequestsUserApproval) {
  ASSERT_TRUE(manager()->consume({1, {SupLan::kResourceTypeChannel, 1001},
      SupLan::kPermissionRead, false, RemoteDelivery::ServerStream}));
  manager()->iterate(0);
  ASSERT_EQ(requests.size(), 1U);
  EXPECT_EQ(requests.back().Flags, 0);
  reply(SUPLA_SUPLAN_ACCESS_STATUS_PENDING_APPROVAL);
  auto shared = static_cast<Device::RemoteChannelManager *>(manager());
  EXPECT_FALSE(shared->state(
      {1001, RemoteDelivery::ServerStream}, 0).available());
  manager()->remove(1);
}
TEST_F(ServerStream, CancelledAnonymousReplyCannotApproveAnotherChannel) {
  ServerRemoteChannel a(1001);
  tick();
  ASSERT_EQ(requests.size(), 1U);
  ASSERT_TRUE(a.configure(1002, READ));
  tick(1);
  ASSERT_EQ(requests.size(), 1U);  // Old SRPC reply has not drained.
  reply(SUPLA_SUPLAN_ACCESS_STATUS_REJECTED);
  EXPECT_NE(a.accessStatus(), ChannelAccessStatus::REJECTED);
  publish(snapshot(1001));
  EXPECT_FALSE(a.state().available());
  tick(1000);
  ASSERT_EQ(requests.size(), 2U);
  EXPECT_EQ(requests.back().Resource.ResourceId, 1002);
  reply();
  publish(snapshot(1002));
  EXPECT_TRUE(a.state().available());
}
TEST_F(ServerStream, TimeoutReconnectsBeforeReusingAnonymousWireSlot) {
  ServerRemoteChannel a(1001);
  tick();
  ASSERT_EQ(requests.size(), 1U);
  time.advance(10000);
  EXPECT_CALL(wire, srpc_free(&transport)).Times(1);
  EXPECT_FALSE(protocol.iterate(time.value));
  EXPECT_FALSE(protocol.acceptEnsureResult());
  EXPECT_FALSE(a.state().available());
  EXPECT_FALSE(protocol.onRemoteChannelState(nullptr));
  EXPECT_FALSE(protocol.ensureResourceAccess(requests.back()));
  protocol.connected(&transport);
  bootstrap();
  manager()->serverReconnected();
  tick(1);
  ASSERT_EQ(requests.size(), 2U);
  reply();
  publish(snapshot());
  EXPECT_TRUE(a.state().available());
}
TEST_F(ServerStream, SameLocalChannelIdStillRequiresServerAuthorization) {
  ServerRemoteChannel a(601);
  EXPECT_EQ(a.localChannel(), nullptr);
  tick();
  ASSERT_EQ(requests.size(), 1U);
  EXPECT_FALSE(a.canRead());
  reply();
  publish(snapshot(601));
  EXPECT_TRUE(a.canRead());
  EXPECT_EQ(a.localChannel(), nullptr);
}
TEST_F(ServerStream, FunctionNoneAlsoInvalidatesBorrowedValue) {
  ServerRemoteChannel a(1001);
  tick();
  reply();
  publish(snapshot());
  auto old = a.state();
  ASSERT_TRUE(old.available());
  auto none = snapshot();
  none.Channel.Default = SUPLA_CHANNELFNC_NONE;
  publish(none);
  EXPECT_FALSE(old.available());
  EXPECT_FALSE(a.state().available());
}

TEST_F(ServerStream, ManyConsumersShareOneBoundedStreamAndReadmit) {
  const int capacity = SUPLA_SERVER_STREAM_MAX_CONSUMERS;
  for (int i = 1; i <= capacity; ++i)
    ASSERT_TRUE(manager()->consume({static_cast<uint32_t>(i),
        {SupLan::kResourceTypeChannel, 1001}, SupLan::kPermissionRead, false,
        RemoteDelivery::ServerStream}));
  EXPECT_EQ(manager()->resourceCount(), 1);
  EXPECT_EQ(manager()->consumerCount(), capacity);
  EXPECT_FALSE(manager()->consume({static_cast<uint32_t>(capacity + 1),
      {SupLan::kResourceTypeChannel, 1001}, SupLan::kPermissionRead, false,
      RemoteDelivery::ServerStream}));
  manager()->iterate(0);
  ASSERT_EQ(requests.size(), 1U);
  reply();
  publish(snapshot());
  manager()->remove(capacity / 2);
  EXPECT_TRUE(manager()->consume({static_cast<uint32_t>(capacity + 1),
      {SupLan::kResourceTypeChannel, 1001}, SupLan::kPermissionRead, false,
      RemoteDelivery::ServerStream}));
  manager()->iterate(1);
  EXPECT_EQ(requests.size(), 1U);
  RecordProperty("shared_stream_heap_payload_bytes",
                 static_cast<int>(protocol.serverStreamAllocatedBytes()));
  for (int i = 1; i <= capacity + 1; ++i) manager()->remove(i);
  EXPECT_EQ(manager()->resourceCount(), 0);
  EXPECT_EQ(manager()->consumerCount(), 0);
  RecordProperty("empty_stream_backend_bytes",
                 static_cast<int>(protocol.serverStreamAllocatedBytes()));
}
TEST_F(ServerStream, InvalidSnapshotBetweenOnlineStatesResetsBaseline) {
  class Counter : public ActionHandler {
   public:
    void handleAction(int, int) override { ++count; }
    int count = 0;
  } counter;
  ServerRemoteChannel a(1001);
  a.addAction(1, counter, ON_CHANGE);
  tick();
  reply();
  publish(snapshot());
  ChannelHandle::iterateAll();
  auto none = snapshot();
  none.Channel.Default = SUPLA_CHANNELFNC_NONE;
  publish(none);
  // The ordinary loop need not observe the intermediate unavailable state.
  publish(snapshot(1001, 30));
  ChannelHandle::iterateAll();
  EXPECT_EQ(counter.count, 0);
}

TEST_F(ServerStream, Version28StateCannotEnterAnEligibleV29Connection) {
  ServerRemoteChannel a(1001);
  tick();
  reply();
  auto state = snapshot();
  dispatch(SUPLA_SD_CALL_REMOTE_CHANNEL_STATE, &state, nullptr, 28);
  EXPECT_FALSE(a.state().available());
  publish(state);
  EXPECT_TRUE(a.state().available());
}

TEST_F(ServerStream, ReconfigureInCallbackReleasesOldCacheSafely) {
  ServerRemoteChannel a(1001);
  class Switch : public ActionHandler {
   public:
    explicit Switch(ChannelHandle *source) : source(source) {}
    void handleAction(int, int) override {
      EXPECT_TRUE(source->configure(1002, READ));
      ++count;
    }
    ChannelHandle *source;
    int count = 0;
  } change(&a);
  class Counter : public ActionHandler {
   public:
    void handleAction(int, int) override { ++count; }
    int count = 0;
  } counter;
  a.addAction(1, change, ON_CHANGE);
  a.addAction(2, counter, ON_TURN_ON);
  tick();
  reply();
  auto binary = snapshot();
  binary.Channel.Type = SUPLA_CHANNELTYPE_BINARYSENSOR;
  binary.Channel.Default = SUPLA_CHANNELFNC_BINARY_SENSOR;
  memset(binary.Channel.value, 0, sizeof(binary.Channel.value));
  publish(binary);
  ChannelHandle::iterateAll();
  const auto borrowed = a.state();
  binary.Channel.value[0] = 1;
  publish(binary);
  ChannelHandle::iterateAll();
  EXPECT_EQ(change.count, 1);
  EXPECT_EQ(counter.count, 0);
  EXPECT_FALSE(borrowed.available());
  EXPECT_FALSE(a.state().available());
  tick();
  ASSERT_EQ(requests.size(), 2U);
  reply();
  publish(binary);  // Late update for abandoned A cannot populate B.
  EXPECT_FALSE(a.state().available());
  binary.ChannelId = 1002;
  publish(binary);
  ChannelHandle::iterateAll();
  EXPECT_EQ(change.count, 1);
  EXPECT_EQ(counter.count, 0);
}

TEST_F(ServerStream, PersistedIdentityAdmitsAfterProductionSyncOrdering) {
  // The accepted map survives disconnect, so resolution can already be REMOTE
  // while registration and DEFAULT config synchronization are still pending.
  protocol.lost();
  ASSERT_TRUE(protocol.serverIdentity().identityAvailable());
  protocol.connected(&transport);
  ASSERT_TRUE(protocol.serverIdentity().registrationStarted());
  ServerRemoteChannel channel(1001);
  tick();
  EXPECT_EQ(protocol.serverStreamAllocatedBytes(), 0U);
  EXPECT_TRUE(requests.empty());

  TSD_SuplaRegisterDeviceResult registered = {};
  registered.result_code = SUPLA_RESULTCODE_TRUE;
  registered.version = 29;
  registered.version_min = 1;
  registered.activity_timeout = 30;
  protocol.onRegisterResult(&registered);
  tick();  // Defer again after the registration notification, before SYNC.
  TSD_SuplaDeviceIdentities ids = {};
  ids.DeviceId = 202;
  ids.ChannelCount = 1;
  ids.ChannelId[0] = 601;
  protocol.onDeviceIdentities(&ids);
  tick();
  EXPECT_FALSE(protocol.serverIdentity().identityTransition());
  EXPECT_FALSE(protocol.serverStreamReady());
  EXPECT_EQ(manager()->consumerCount(), 0);
  EXPECT_EQ(protocol.serverStreamAllocatedBytes(), 0U);
  EXPECT_TRUE(requests.empty());

  class ConfigBarrier : public Element {
   public:
    bool isChannelConfigDurable() const override { return durable; }
    bool durable = false;
  } config;
  const auto deferredGeneration = manager()->admissionGeneration();
  protocol.onDeviceSyncDone();
  tick();
  EXPECT_EQ(manager()->admissionGeneration(), deferredGeneration);
  EXPECT_FALSE(protocol.serverStreamReady());
  EXPECT_TRUE(requests.empty());

  config.durable = true;
  protocol.onDeviceSyncDone();
  ASSERT_TRUE(protocol.serverStreamReady());
  tick();
  ASSERT_EQ(requests.size(), 1U);
  EXPECT_EQ(requests.back().DeliveryMode,
            SUPLA_RESOURCE_DELIVERY_SERVER_STREAM);
  reply();
  publish(snapshot());
  EXPECT_DOUBLE_EQ(channel.state().temperature(), 21);

  const auto admittedGeneration = manager()->admissionGeneration();
  protocol.onDeviceSyncDone();  // Duplicate sync must not reset active demand.
  tick();
  EXPECT_EQ(manager()->admissionGeneration(), admittedGeneration);
  EXPECT_EQ(requests.size(), 1U);
  EXPECT_TRUE(channel.state().available());
}
}  // namespace

TEST_F(ServerStream, DeviceIdMigrationInvalidatesBorrowedViewAndOldDemand) {
  ServerRemoteChannel channel(1001);
  tick();
  reply();
  publish(snapshot());
  auto borrowed = channel.state();
  ASSERT_TRUE(borrowed.available());
  const auto epoch = protocol.serverIdentity().rootEpoch();
  auto &identity = protocol.serverIdentity();
  ASSERT_TRUE(identity.registrationStarted());
  identity.registrationSucceeded();
  TSD_SuplaDeviceIdentities changed = {};
  changed.DeviceId = 303;
  changed.ChannelCount = 1;
  changed.ChannelId[0] = 701;
  ASSERT_EQ(identity.accept(changed).Result, SUPLA_SUPLAN_RESULT_OK);
  EXPECT_FALSE(borrowed.available());
  EXPECT_FALSE(channel.state().available());
  EXPECT_EQ(manager()->consumerCount(), 0);
  auto stale = snapshot();
  EXPECT_FALSE(protocol.onRemoteChannelState(&stale));
  identity.syncDone();
  tick(1000);
  EXPECT_EQ(requests.size(), 1u);  // No automatic approval in the new scope.
  EXPECT_EQ(identity.rootEpoch(), epoch);
}

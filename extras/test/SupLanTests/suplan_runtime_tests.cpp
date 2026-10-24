// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright 2026 AC SOFTWARE SP. Z O.O.

#include <supla-common/proto.h>
#include <suplan/suplan_runtime.h>
#include <suplan/suplan_crypto.h>
#include <suplan/suplan_wire.h>

#include <gtest/gtest.h>
#include <simple_time.h>
#include <supla/control/virtual_relay.h>
#include <supla/suplan/remote_resource_manager.h>
#include <supla/protocol/suplan_protocol.h>
#include <supla/suplan/suplan_server_associations.h>
#include <crypto_test_hooks.h>

#include <array>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>

#include "suplan_crypto_openssl.h"

// The 6 KiB gate belongs to the constrained profile. Larger profiles reserve
// additional peers/sessions/interests; retain an independent bound for each.
#if SUPLAN_PROFILE == SUPLAN_PROFILE_CONSTRAINED
constexpr size_t kCoreWorkspaceBudget = 6 * 1024;
#elif SUPLAN_PROFILE == SUPLAN_PROFILE_STANDARD
constexpr size_t kCoreWorkspaceBudget = 12 * 1024;
#else
constexpr size_t kCoreWorkspaceBudget = 32 * 1024;
#endif
static_assert(sizeof(Supla::SupLan::Runtime) +
                  sizeof(Supla::SupLan::PeerTable) <= kCoreWorkspaceBudget,
              "SupLAN core workspace exceeds its profile budget");

namespace {

using Supla::SupLan::Endpoint;
using Supla::SupLan::NodeAddress;
using Supla::SupLan::ResourceId;


struct Packet {
  Endpoint source;
  Endpoint destination;
  bool multicast;
  std::vector<uint8_t> bytes;
};

struct FakeNetwork {
  std::vector<Packet> packets;
  std::vector<Packet> history;
  uint32_t now;
  bool failMulticast = false;
  bool dropNextFullData = false;
  uint32_t multicastAttempts = 0;

  FakeNetwork() : packets(), history(), now(0) {}

  bool send(const Endpoint &source, const Endpoint &destination,
            bool multicast, const uint8_t *data, size_t length) {
    if (data == nullptr || length == 0 ||
        length > Supla::SupLan::kMaxDatagramPayload) {
      return false;
    }
    if (multicast) {
      ++multicastAttempts;
      if (failMulticast) return false;
    }
    Packet packet = {};
    packet.source = source;
    packet.destination = destination;
    packet.multicast = multicast;
    packet.bytes.assign(data, data + length);
    history.push_back(packet);
    if (dropNextFullData && !multicast && length > 3 &&
        data[0] == Supla::SupLan::kAdaptationFull &&
        data[1] == Supla::SupLan::kVersion &&
        data[2] == Supla::SupLan::kFrameData) {
      dropNextFullData = false;
      return true;
    }
    packets.push_back(packet);
    return true;
  }
};

class FakeDatagramPort : public Supla::SupLan::DatagramPort {
 public:
  FakeDatagramPort(FakeNetwork *network, const Endpoint &self)
      : available(true), network_(network), self_(self) {}

  bool sendUnicast(const Endpoint &endpoint, const uint8_t *data,
                   size_t length) override {
    if (!available) return false;
    return network_->send(self_, endpoint, false, data, length);
  }

  bool sendLocateMulticast(const uint8_t *data, size_t length) override {
    if (!available) return false;
    Endpoint group = {0x06C9FFEF, 2016};
    return network_->send(self_, group, true, data, length);
  }

  int pollReceive(uint8_t *buffer, size_t capacity,
                  Endpoint *endpoint) override {
    if (!available) return 0;
    for (size_t i = 0; i < network_->packets.size(); ++i) {
      const Packet &packet = network_->packets[i];
      const bool matches = packet.multicast ||
          (packet.destination.address == self_.address &&
           packet.destination.port == self_.port);
      if (!matches ||
          (packet.source.address == self_.address &&
           packet.source.port == self_.port) ||
          packet.bytes.size() > capacity) {
        continue;
      }
      std::memcpy(buffer, packet.bytes.data(), packet.bytes.size());
      *endpoint = packet.source;
      const int result = static_cast<int>(packet.bytes.size());
      network_->packets.erase(network_->packets.begin() + i);
      return result;
    }
    return 0;
  }

  size_t maxDatagramPayload() const override {
    return Supla::SupLan::kMaxDatagramPayload;
  }

  uint32_t nowMs() const override { return network_->now; }

  bool available;

 private:
  FakeNetwork *network_;
  Endpoint self_;
};


class FakeApplication : public Supla::SupLan::ApplicationPort {
 public:
  void diagnostic(Supla::SupLan::DiagnosticEvent event, uint8_t,
      const ResourceId &, uint32_t) override {
    ++diagnosticEvents[static_cast<unsigned>(event)];
  }
  uint32_t diagnosticEvents[32] = {};
  FakeApplication() : value(0), controlCalls(0), stateCalls(0), actionCalls(0),
      acknowledged(0), lastAckResult(0), lastStateType(0), lastStateLength(0),
      runtime(nullptr) {}

  Supla::SupLan::BindingRetry *bindingRetry() override {
    return bindingPoolEnabled ? &largeRetry : nullptr;
  }
  uint8_t receiveBindings(uint8_t, const uint8_t *body,
                          size_t length) override {
    ++bindingCalls;
    lastBindings.assign(body, body + length);
    return bindingResult;
  }
  Supla::SupLan::BindingRetry largeRetry;
  bool bindingSessionNeeded(uint8_t) override { return bindingPoolEnabled; }
  bool bindingPoolEnabled = false;
  int bindingCalls = 0;
  uint8_t bindingResult = 3;
  std::vector<uint8_t> lastBindings;

  bool readResource(const ResourceId &resource, bool *eventOnly,
                    uint8_t *payload,
                    size_t capacity, size_t *payloadLength) override {
    if (resource.id == 50002) {
      *eventOnly = true;
      *payloadLength = 0;
      return false;
    }
    if (unavailableResource && resource.id == unavailableResource) return false;
    ++readCalls;
    if (fullSnapshots && stateAvailable && capacity >=
        sizeof(TDS_SuplaDeviceChannel_E)) {
      TDS_SuplaDeviceChannel_E snapshot = {};
      snapshot.Number = 0xff;
      snapshot.Type = SUPLA_CHANNELTYPE_RELAY;
      snapshot.Default = SUPLA_CHANNELFNC_POWERSWITCH;
      snapshot.ValueValidityTimeSec = 60;
      snapshot.value[0] = value;
      memcpy(payload, &snapshot, sizeof(snapshot));
      *eventOnly = false;
      *payloadLength = sizeof(snapshot);
      return true;
    }
    if (!stateAvailable || capacity < 14) {
      *eventOnly = false;
      *payloadLength = 0;
      return false;
    }
    *eventOnly = false;
    std::memset(payload, 0, 14);
    payload[0] = 0xFF;
    payload[1] = 0;
    payload[5] = 60;
    payload[6] = value;
    *payloadLength = 14;
    return true;
  }

  uint8_t dispatchControl(const ResourceId &, uint32_t,
                          const uint8_t *payload,
                          size_t payloadLength) override {
    ++controlCalls;
    if (payloadLength == 17) {
      value = payload[9];
    }
    if (runtime != nullptr) {
      uint8_t state[14] = {};
      state[0] = 0xFF;
      state[6] = value;
      const ResourceId changed = {Supla::SupLan::kResourceTypeChannel, 50001};
      (void)runtime->publishState(
          0, changed, Supla::SupLan::kSuplaCallDeviceChannelValueChangedC,
          state, sizeof(state));
    }
    return 3;
  }

  uint8_t dispatchControlAction(const ResourceId &, uint32_t actionId,
                                const uint8_t *params, size_t length) override {
    if (nativeElement) {
      ++nativeControlCalls;
      return nativeElement->handleRemoteAction(actionId, params, length);
    }
    if (actionId != ACTION_TOGGLE && actionId != ACTION_SET_RELAY_PARAMETERS)
      return 33;
    ++nativeControlCalls;
    nativeParams.assign(params, params + length);
    value = !value;
    return 3;
  }
  uint32_t nativeControlCalls = 0;
  std::vector<uint8_t> nativeParams;

  void receiveState(uint8_t peerIndex, const ResourceId &resource,
                    uint32_t messageType,
                    const uint8_t *payload, size_t payloadLength) override {
    ++stateCalls;
    if (remoteResources)
      remoteResources->receive(peerIndex, resource, payload, payloadLength,
                                runtime ? runtime->nowMs() : 0);
    lastStateType = messageType;
    lastStateLength = payloadLength;
    if (payloadLength == 14) {
      value = payload[6];
    } else if (payloadLength == sizeof(TDS_SuplaDeviceChannel_E)) {
      TDS_SuplaDeviceChannel_E snapshot;
      memcpy(&snapshot, payload, sizeof(snapshot));
      value = snapshot.value[0];
    }
  }

  void receiveAction(uint8_t, const ResourceId &, uint32_t,
                     const uint8_t *payload, size_t payloadLength) override {
    ++actionCalls;
    if (payload != nullptr && payloadLength == 15) {
      lastActionId = static_cast<uint32_t>(payload[1]) |
          (static_cast<uint32_t>(payload[2]) << 8) |
          (static_cast<uint32_t>(payload[3]) << 16) |
          (static_cast<uint32_t>(payload[4]) << 24);
    }
  }

  void operationAcknowledged(uint8_t, const ResourceId &, uint32_t,
                             uint8_t result) override {
    ++acknowledged;
    lastAckResult = result;
  }

  bool allowPresence(uint8_t index, uint32_t now) override {
    return remoteResources ? remoteResources->allowPresence(index, now)
                           : presenceAllowed;
  }
  void peerAwake(uint8_t index, uint16_t window, bool reset,
                 const uint8_t *nonce) override {
    ++awakeHints;
    if (remoteResources) {
      remoteResources->peerAwake(index, window, reset, nonce,
                                 runtime ? runtime->nowMs() : 0);
    } else if (runtime) {
      runtime->refreshPeer(index, window, reset);
    }
  }
  bool presenceAllowed = false;
  int awakeHints = 0;
  void sessionChanged(uint8_t peerIndex) override {
    if (remoteResources) remoteResources->sessionChanged(peerIndex);
  }
  bool fullChannelSnapshots() const override { return fullSnapshots; }
  bool fullSnapshots = false;
  uint32_t readCalls = 0;
  Supla::Element *nativeElement = nullptr;
  Supla::Device::RemoteResourceManager *remoteResources = nullptr;
  bool stateAvailable = true;
  uint32_t unavailableResource = 0;
  uint8_t value;
  uint32_t controlCalls;
  uint32_t stateCalls;
  uint32_t actionCalls;
  uint32_t acknowledged;
  uint8_t lastAckResult;
  uint32_t lastActionId = 0;
  uint32_t lastStateType;
  size_t lastStateLength;
  Supla::SupLan::Runtime *runtime;
};

struct RuntimePair {
  ScopedCryptoTestState rng{5};
  Supla::SupLan::OpenSslCryptoPort crypto;
  FakeNetwork network;
  Endpoint endpointA;
  Endpoint endpointB;
  FakeDatagramPort datagramsA;
  FakeDatagramPort datagramsB;
  FakeApplication appA;
  FakeApplication appB;
  Supla::SupLan::PeerTable peersA;
  Supla::SupLan::PeerTable peersB;
  Supla::SupLan::Runtime runtimeA;
  Supla::SupLan::Runtime runtimeB;
  ResourceId resource;
  uint8_t peerA;
  uint8_t peerB;
  uint8_t actionPeerA;
  uint8_t actionPeerB;

  static NodeAddress nodeAddress(uint32_t nodeId) {
    NodeAddress node = {Supla::SupLan::kNodeIdDevice, nodeId};
    return node;
  }

  explicit RuntimePair(bool sameHost = false)
      : crypto(), network(),
        endpointA({0x0100007F, static_cast<uint16_t>(sameHost ? 32171 : 2016)}),
        endpointB({sameHost ? 0x0100007FU : 0x0200007FU,
                   static_cast<uint16_t>(sameHost ? 32172 : 2016)}),
        datagramsA(&network, endpointA), datagramsB(&network, endpointB),
        appA(), appB(), peersA(), peersB(),
        runtimeA(&crypto, &datagramsA, &appA, &peersA,
                 nodeAddress(1001), 27),
        runtimeB(&crypto, &datagramsB, &appB, &peersB,
                 nodeAddress(1002), 27),
        resource({Supla::SupLan::kResourceTypeChannel, 50001}), peerA(0),
        peerB(0), actionPeerA(0), actionPeerB(0) {
    Supla::SupLan::PeerContext context = {};
    context.authorityType = Supla::SupLan::kAuthorityServer;
    context.source.nameSpace = Supla::SupLan::kNodeIdDevice;
    context.source.nodeId = 1001;
    context.destination.nameSpace = Supla::SupLan::kNodeIdDevice;
    context.destination.nodeId = 1002;
    context.rootEpoch = 1;
    context.peerGeneration = 1;
    const uint8_t rootKey[32] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
        0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F};
    Supla::SupLan::PeerContext actionContext = context;
    actionContext.source.nodeId = 1002;
    actionContext.destination.nodeId = 1001;
    Supla::SupLan::AclEntry acl = {};
    acl.resource = resource;
    acl.permissions = Supla::SupLan::kPermissionRead |
        Supla::SupLan::kPermissionControl;
    Supla::SupLan::PeerMaterial material = {};
    EXPECT_TRUE(Supla::SupLan::derivePeerMaterial(
        &context, rootKey, &material));
    EXPECT_TRUE(peersA.addPeerFromRoot(&context, rootKey, 1, &acl, 1,
                                       &peerA));
    EXPECT_TRUE(peersB.addPeer(&context, material.peerKey, 1, &acl, 1,
                               &peerB, true));
    const uint8_t reversePeerKey[32] = {
        0x70, 0xD1, 0x3F, 0x17, 0x8A, 0x1F, 0x93, 0xE3,
        0x4E, 0x0C, 0x8A, 0x0B, 0x84, 0x4C, 0x4E, 0xAB,
        0x04, 0xEC, 0xF8, 0x62, 0x90, 0x42, 0xD9, 0xC0,
        0xFA, 0x42, 0xB9, 0xC1, 0xC4, 0x62, 0x47, 0xCC};
    Supla::SupLan::AclEntry actionAcl = {};
    actionAcl.resource.type = Supla::SupLan::kResourceTypeChannel;
    actionAcl.resource.id = 50002;
    actionAcl.permissions = Supla::SupLan::kPermissionActionTrigger;
    EXPECT_TRUE(peersA.addPeer(&actionContext, reversePeerKey, 1,
                               &actionAcl, 1, &actionPeerA, true));
    EXPECT_TRUE(peersB.addPeerFromRoot(&actionContext, rootKey, 1,
                                       &actionAcl, 1, &actionPeerB));
    rng.resetCalls();
    appA.runtime = &runtimeA;
  }

  void pump(uint8_t count) {
    for (uint8_t i = 0; i < count; ++i) {
      network.now += 10;
      runtimeA.iterate();
      runtimeB.iterate();
    }
  }

  void pumpMs(uint32_t durationMs) {
    for (uint32_t elapsed = 0; elapsed < durationMs; elapsed += 10) {
      network.now += 10;
      runtimeA.iterate();
      runtimeB.iterate();
    }
  }
};

std::vector<std::vector<uint8_t>> protectedDataFromB(
    const RuntimePair &pair, size_t firstHistoryIndex) {
  std::vector<std::vector<uint8_t>> frames;
  for (size_t i = firstHistoryIndex; i < pair.network.history.size(); ++i) {
    const Packet &packet = pair.network.history[i];
    if (!packet.multicast &&
        packet.source.address == pair.endpointB.address &&
        packet.source.port == pair.endpointB.port &&
        packet.destination.address == pair.endpointA.address &&
        packet.destination.port == pair.endpointA.port &&
        packet.bytes.size() > 2 &&
        packet.bytes[0] == Supla::SupLan::kAdaptationFull &&
        packet.bytes[1] == Supla::SupLan::kVersion &&
        packet.bytes[2] == Supla::SupLan::kFrameData) {
      frames.push_back(packet.bytes);
    }
  }
  return frames;
}

TEST(SupLanRuntime, IterateWithoutPeerTableIsNoOp) {
  Supla::SupLan::OpenSslCryptoPort crypto;
  FakeNetwork network;
  FakeDatagramPort datagrams(&network, {0x0100007F, 2017});
  FakeApplication application;
  Supla::SupLan::Runtime runtime(
      &crypto, &datagrams, &application, nullptr,
      {Supla::SupLan::kNodeIdDevice, 101}, 29);

  runtime.iterate();
  runtime.iterate();

  EXPECT_TRUE(network.packets.empty());
  EXPECT_EQ(network.multicastAttempts, 0U);
  EXPECT_EQ(application.controlCalls, 0U);
  EXPECT_EQ(application.stateCalls, 0U);
}

TEST(SupLanRuntime, RandomFailureDoesNotTransmitLocate) {
  RuntimePair pair;
  pair.rng.failRandom = true;
  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
  pair.runtimeB.iterate();
  EXPECT_EQ(pair.runtimeB.diagnostics().locateTx, 0U);
  EXPECT_TRUE(pair.network.packets.empty());
}

TEST(SupLanRuntime, LocateKdfRunsOnceAndNotAgainWhileLocateIsOutstanding) {
  RuntimePair pair;

  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
  pair.runtimeB.iterate();
  EXPECT_EQ(pair.runtimeB.diagnostics().locateTx, 1U);
  EXPECT_EQ(pair.rng.sha256Calls, 1U);
  EXPECT_EQ(pair.rng.hmacSha256Calls, 6U);

  pair.network.now += 5;
  pair.runtimeB.iterate();
  EXPECT_EQ(pair.runtimeB.diagnostics().locateTx, 1U);
  EXPECT_EQ(pair.rng.sha256Calls, 1U);
  EXPECT_EQ(pair.rng.hmacSha256Calls, 6U);

  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
  EXPECT_EQ(pair.runtimeB.diagnostics().locateTx, 1U);
  EXPECT_EQ(pair.rng.sha256Calls, 1U);
  EXPECT_EQ(pair.rng.hmacSha256Calls, 6U);
}

TEST(SupLanRuntime, LocateSessionReadControlRetryAndDuplicateSuppression) {
  RuntimePair pair;
  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
  pair.pump(20);
  EXPECT_EQ(pair.runtimeA.diagnostics().locateRx, 1U);
  EXPECT_EQ(pair.runtimeB.diagnostics().locateReplyRx, 1U);
  EXPECT_EQ(pair.runtimeA.diagnostics().sessionEstablished, 1U);
  EXPECT_EQ(pair.runtimeB.diagnostics().sessionEstablished, 1U);
  EXPECT_EQ(pair.runtimeA.diagnostics().readDispatched, 1U);
  EXPECT_EQ(pair.runtimeB.diagnostics().stateNotificationRx, 1U);
  EXPECT_EQ(pair.appB.value, 0U);
  EXPECT_EQ(pair.runtimeA.poolDiagnostics().interests.used, 1U);

  pair.runtimeA.testHooks()->dropNextAckTx = 1;
  const size_t historyBeforeControl = pair.network.history.size();
  uint8_t control[17] = {};
  control[4] = 0xFF;
  control[9] = 1;
  ASSERT_TRUE(pair.runtimeB.sendControl(pair.peerB, pair.resource, control,
                                        sizeof(control)));
  pair.pump(5);
  EXPECT_EQ(pair.appA.controlCalls, 1U);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().retries.used, 1U);
  pair.network.now += Supla::SupLan::kAckRetryMs + 1;
  pair.runtimeB.iterate();
  pair.pump(8);
  EXPECT_EQ(pair.appA.controlCalls, 1U);
  EXPECT_EQ(pair.runtimeA.diagnostics().controlDispatched, 1U);
  EXPECT_EQ(pair.runtimeA.diagnostics().controlDuplicateSuppressed, 1U);
  EXPECT_EQ(pair.runtimeA.diagnostics().ackTx, 3U);
  EXPECT_EQ(pair.runtimeB.diagnostics().retryTx, 1U);
  EXPECT_EQ(pair.appB.acknowledged, 2U);
  EXPECT_EQ(pair.appB.lastAckResult, 3U);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().retries.used, 0U);
  EXPECT_EQ(pair.appB.value, 1U);
  std::vector<std::vector<uint8_t> > controlTransmissions;
  for (size_t i = historyBeforeControl; i < pair.network.history.size(); ++i) {
    const Packet &packet = pair.network.history[i];
    if (!packet.multicast &&
        packet.source.address == pair.endpointB.address &&
        packet.destination.address == pair.endpointA.address &&
        packet.bytes.size() > 2 &&
        packet.bytes[0] == Supla::SupLan::kAdaptationFull &&
        packet.bytes[1] == Supla::SupLan::kVersion &&
        packet.bytes[2] == Supla::SupLan::kFrameData) {
      controlTransmissions.push_back(packet.bytes);
    }
  }
  ASSERT_EQ(controlTransmissions.size(), 2U);
  EXPECT_EQ(controlTransmissions[0], controlTransmissions[1]);

  pair.runtimeA.testHooks()->maxDatagramPayload = 96;
  const uint8_t extendedPrefix[6] = {0xFF, 100, 100, 0, 0, 0};
  std::array<uint8_t, 100> extendedValue = {};
  for (size_t i = 0; i < extendedValue.size(); ++i) {
    extendedValue[i] = static_cast<uint8_t>(i ^ 0x5A);
  }
  ASSERT_TRUE(pair.runtimeA.publishStateParts(
      pair.peerA, pair.resource,
      Supla::SupLan::kSuplaCallDeviceChannelExtendedValueChanged,
      extendedPrefix, sizeof(extendedPrefix), extendedValue.data(),
      extendedValue.size()));
  pair.pump(8);
  EXPECT_GE(pair.runtimeA.diagnostics().fragmentTx, 2U);
  EXPECT_EQ(pair.runtimeB.diagnostics().reassemblyCompleted, 1U);
  EXPECT_EQ(pair.appB.lastStateType,
            Supla::SupLan::kSuplaCallDeviceChannelExtendedValueChanged);
  EXPECT_EQ(pair.appB.lastStateLength, 106U);

  const uint32_t stateCallsBeforeLoss = pair.appB.stateCalls;
  pair.runtimeA.testHooks()->dropFragmentNumber = 2;
  ASSERT_TRUE(pair.runtimeA.publishStateParts(
      pair.peerA, pair.resource,
      Supla::SupLan::kSuplaCallDeviceChannelExtendedValueChanged,
      extendedPrefix, sizeof(extendedPrefix), extendedValue.data(),
      extendedValue.size()));
  pair.pump(4);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().reassembly.used, 1U);
  pair.network.now += Supla::SupLan::kReassemblyTimeoutMs + 1;
  pair.runtimeB.iterate();
  EXPECT_EQ(pair.runtimeB.diagnostics().reassemblyExpired, 1U);
  EXPECT_EQ(pair.appB.stateCalls, stateCallsBeforeLoss);

  EXPECT_TRUE(pair.runtimeA.forgetSession(pair.peerA));
  EXPECT_EQ(pair.runtimeA.poolDiagnostics().interests.used, 1U);
}

TEST(SupLanRuntime,
     LostReadAckRetriesIdenticalFrameAndSuppressesDuplicateRead) {
  RuntimePair pair;
  pair.runtimeA.testHooks()->dropNextAckTx = 1;
  const size_t historyStart = pair.network.history.size();
  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
  pair.pump(20);

  EXPECT_EQ(pair.runtimeA.diagnostics().readDispatched, 1U);
  EXPECT_EQ(pair.runtimeA.diagnostics().dataDuplicate, 1U);
  EXPECT_EQ(pair.runtimeA.diagnostics().ackTx, 2U);
  EXPECT_EQ(pair.runtimeB.diagnostics().ackRx, 1U);
  EXPECT_EQ(pair.runtimeB.diagnostics().retryTx, 1U);
  EXPECT_EQ(pair.appB.stateCalls, 1U);
  EXPECT_EQ(pair.appB.acknowledged, 1U);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().retries.used, 0U);

  std::vector<std::vector<uint8_t> > readTransmissions;
  for (size_t i = historyStart; i < pair.network.history.size(); ++i) {
    const Packet &packet = pair.network.history[i];
    if (!packet.multicast &&
        packet.source.address == pair.endpointB.address &&
        packet.destination.address == pair.endpointA.address &&
        packet.source.port == pair.endpointB.port &&
        packet.destination.port == pair.endpointA.port &&
        packet.bytes.size() > 2 &&
        packet.bytes[0] == Supla::SupLan::kAdaptationFull &&
        packet.bytes[1] == Supla::SupLan::kVersion &&
        packet.bytes[2] == Supla::SupLan::kFrameData) {
      readTransmissions.push_back(packet.bytes);
    }
  }
  ASSERT_EQ(readTransmissions.size(), 2U);
  EXPECT_EQ(readTransmissions[0], readTransmissions[1]);
}

TEST(SupLanRuntime, StatefulReadWithoutStateResponseIssuesNewRead) {
  RuntimePair pair;
  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
  pair.pump(20);
  const uint32_t readsBefore = pair.runtimeA.diagnostics().readDispatched;
  const uint32_t statesBefore = pair.appB.stateCalls;
  const uint32_t acksBefore = pair.runtimeB.diagnostics().ackRx;

  pair.runtimeA.testHooks()->dropNextDataTx = 1;
  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
  pair.pump(8);
  EXPECT_EQ(pair.runtimeB.diagnostics().ackRx, acksBefore + 1);
  EXPECT_EQ(pair.appB.stateCalls, statesBefore);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().retries.used, 1U);

  pair.network.now += Supla::SupLan::kReadStateResponseTimeoutMs + 1;
  pair.runtimeB.iterate();
  pair.pump(12);
  EXPECT_EQ(pair.runtimeA.diagnostics().readDispatched, readsBefore + 2);
  EXPECT_EQ(pair.appB.stateCalls, statesBefore + 1);
  EXPECT_EQ(pair.runtimeB.diagnostics().ackRx, acksBefore + 2);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().retries.used, 0U);
}

TEST(SupLanRuntime, InvalidProtectedDataNeverReachesApplication) {
  RuntimePair pair;
  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
  pair.pump(20);
  ASSERT_EQ(pair.runtimeA.diagnostics().sessionEstablished, 1U);
  ASSERT_EQ(pair.runtimeB.diagnostics().sessionEstablished, 1U);

  uint8_t control[17] = {};
  control[4] = 0xFF;
  control[9] = 1;
  pair.runtimeB.testHooks()->corruptNextDataTagTx = 1;
  ASSERT_TRUE(pair.runtimeB.sendControl(pair.peerB, pair.resource, control,
                                        sizeof(control)));
  pair.pump(5);
  EXPECT_EQ(pair.runtimeA.diagnostics().dataAuthFail, 1U);
  EXPECT_EQ(pair.appA.controlCalls, 0U);

  control[9] = 0;
  ASSERT_TRUE(pair.runtimeB.sendControl(pair.peerB, pair.resource, control,
                                        sizeof(control)));
  pair.pump(5);
  EXPECT_EQ(pair.runtimeA.diagnostics().controlDispatched, 1U);
  EXPECT_EQ(pair.appA.controlCalls, 1U);
}

TEST(SupLanRuntime,
     AdmissionRejectUsesPendingEndpointAfterAuthenticatedDataOnly) {
  RuntimePair pair;
  pair.runtimeA.clearEndpoint(pair.peerA);
  pair.runtimeB.clearEndpoint(pair.peerB);
  pair.runtimeA.testHooks()->failNextSessionAllocation = 1;
  pair.runtimeB.testHooks()->corruptNextDataTagTx = 1;

  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
  pair.pump(20);

  EXPECT_EQ(pair.runtimeA.diagnostics().dataAuthFail, 2U);
  EXPECT_EQ(pair.runtimeA.diagnostics().sessionRejectTx, 0U);
  EXPECT_EQ(pair.runtimeB.diagnostics().sessionRejectRx, 0U);
  EXPECT_EQ(pair.runtimeA.diagnostics().readDispatched, 0U);
  EXPECT_EQ(pair.runtimeA.poolDiagnostics().sessions.used, 0U);
  EXPECT_EQ(pair.runtimeA.poolDiagnostics().pending.used, 1U);
  ASSERT_NE(pair.peersA.get(pair.peerA), nullptr);
  EXPECT_EQ(pair.peersA.get(pair.peerA)->endpointState,
            Supla::SupLan::kPeerEndpointNone);

  // The failed AEAD did not consume the admission failure or promote the
  // pending endpoint. A subsequent authenticated READ reaches the admission
  // check and must receive SESSION_REJECT through that pending endpoint.
  ASSERT_TRUE(pair.runtimeB.forgetSession(pair.peerB));
  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
  for (unsigned i = 0; i < 10 &&
       pair.runtimeB.diagnostics().sessionRejectRx == 0; ++i) {
    pair.pump(1);
  }

  EXPECT_EQ(pair.runtimeA.diagnostics().sessionRejectTx, 1U);
  EXPECT_EQ(pair.runtimeB.diagnostics().sessionRejectRx, 1U);
  EXPECT_EQ(pair.runtimeA.diagnostics().readDispatched, 0U);
  EXPECT_EQ(pair.runtimeA.poolDiagnostics().sessions.used, 0U);
  EXPECT_EQ(pair.runtimeA.poolDiagnostics().pending.used, 0U);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().sessions.used, 0U);
  EXPECT_EQ(pair.runtimeA.diagnostics().poolReject, 1U);
  EXPECT_EQ(pair.peersA.get(pair.peerA)->endpointState,
            Supla::SupLan::kPeerEndpointNone);
}

TEST(SupLanRuntime, ClearEndpointRetainsInterestAndReestablishesSession) {
  RuntimePair pair;
  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
  pair.pump(20);
  ASSERT_EQ(pair.runtimeA.poolDiagnostics().sessions.used, 1U);
  ASSERT_EQ(pair.runtimeB.poolDiagnostics().sessions.used, 1U);
  ASSERT_EQ(pair.runtimeA.poolDiagnostics().interests.used, 1U);
  const uint32_t stateCalls = pair.appB.stateCalls;

  pair.runtimeB.clearEndpoint(pair.peerB);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().sessions.used, 0U);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().interests.used, 0U);
  EXPECT_EQ(pair.runtimeA.poolDiagnostics().interests.used, 1U);
  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
  pair.pump(24);

  EXPECT_EQ(pair.runtimeA.diagnostics().readDispatched, 2U);
  EXPECT_EQ(pair.runtimeA.poolDiagnostics().interests.used, 1U);
  EXPECT_EQ(pair.runtimeA.poolDiagnostics().sessions.used, 1U);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().sessions.used, 1U);
  EXPECT_GT(pair.appB.stateCalls, stateCalls);
  EXPECT_EQ(pair.runtimeB.diagnostics().locateTx, 2U);
  EXPECT_EQ(pair.runtimeA.diagnostics().locateRx, 2U);
}

TEST(SupLanRuntime, ActionOnlyReadRefreshesInterestAndActionsAreAcknowledged) {
  RuntimePair pair;
  const ResourceId actionResource = {
      Supla::SupLan::kResourceTypeChannel, 50002};
  ASSERT_TRUE(pair.runtimeA.requestRead(pair.actionPeerA, actionResource));
  pair.pump(20);
  EXPECT_EQ(pair.runtimeB.diagnostics().readDispatched, 1U);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().interests.used, 1U);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().sessions.used, 1U);

  uint8_t payload[15] = {};
  payload[0] = 0xFF;
  payload[1] = 0x34;
  payload[2] = 0x12;
  const uint32_t ackBefore = pair.runtimeB.diagnostics().ackRx;
  ASSERT_TRUE(pair.runtimeB.publishAction(pair.actionPeerB, actionResource,
                                           payload, sizeof(payload)));
  pair.pump(5);
  EXPECT_EQ(pair.appA.actionCalls, 1U);
  EXPECT_EQ(pair.appA.lastActionId, 0x1234U);
  EXPECT_EQ(pair.runtimeB.diagnostics().actionTx, 1U);
  EXPECT_EQ(pair.runtimeA.diagnostics().actionRx, 1U);
  EXPECT_EQ(pair.runtimeB.diagnostics().ackRx, ackBefore + 1U);
  EXPECT_EQ(pair.runtimeA.poolDiagnostics().retries.used, 0U);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().retries.used, 0U);
}

TEST(SupLanRuntime, DroppedActionDataRetriesAndDroppedAckReacksDuplicate) {
  RuntimePair pair;
  const ResourceId actionResource = {
      Supla::SupLan::kResourceTypeChannel, 50002};
  ASSERT_TRUE(pair.runtimeA.requestRead(pair.actionPeerA, actionResource));
  pair.pump(20);

  uint8_t payload[15] = {};
  payload[0] = 0xFF;
  payload[1] = 0x78;
  payload[2] = 0x56;
  uint32_t actionCalls = pair.appA.actionCalls;
  uint32_t retries = pair.runtimeB.diagnostics().retryTx;
  const size_t droppedDataHistoryStart = pair.network.history.size();
  pair.network.dropNextFullData = true;
  ASSERT_TRUE(pair.runtimeB.publishAction(pair.actionPeerB, actionResource,
                                           payload, sizeof(payload)));
  pair.pumpMs(140);
  EXPECT_EQ(pair.appA.actionCalls, actionCalls);
  EXPECT_EQ(pair.runtimeB.diagnostics().retryTx, retries);
  pair.pumpMs(20);
  EXPECT_EQ(pair.appA.actionCalls, actionCalls + 1U);
  EXPECT_EQ(pair.appA.lastActionId, 0x5678U);
  EXPECT_EQ(pair.runtimeB.diagnostics().retryTx, retries + 1U);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().retries.used, 0U);
  std::vector<std::vector<uint8_t>> recoveredActionFrames =
      protectedDataFromB(pair, droppedDataHistoryStart);
  ASSERT_EQ(recoveredActionFrames.size(), 2U);
  EXPECT_EQ(recoveredActionFrames[0], recoveredActionFrames[1]);

  payload[1] = 0xBC;
  payload[2] = 0x9A;
  actionCalls = pair.appA.actionCalls;
  retries = pair.runtimeB.diagnostics().retryTx;
  const uint32_t ackRx = pair.runtimeB.diagnostics().ackRx;
  const uint32_t duplicateBefore = pair.runtimeA.diagnostics().dataDuplicate;
  const size_t historyStart = pair.network.history.size();
  pair.runtimeA.testHooks()->dropNextAckTx = 1;
  ASSERT_TRUE(pair.runtimeB.publishAction(pair.actionPeerB, actionResource,
                                           payload, sizeof(payload)));
  pair.pumpMs(140);
  EXPECT_EQ(pair.appA.actionCalls, actionCalls + 1U);
  EXPECT_EQ(pair.runtimeB.diagnostics().retryTx, retries);
  pair.pumpMs(20);

  EXPECT_EQ(pair.appA.actionCalls, actionCalls + 1U);
  EXPECT_EQ(pair.runtimeB.diagnostics().retryTx, retries + 1U);
  EXPECT_EQ(pair.runtimeB.diagnostics().ackRx, ackRx + 1U);
  EXPECT_EQ(pair.runtimeA.diagnostics().dataDuplicate, duplicateBefore + 1U);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().retries.used, 0U);
  const std::vector<std::vector<uint8_t>> actionFrames =
      protectedDataFromB(pair, historyStart);
  ASSERT_EQ(actionFrames.size(), 2U);
  EXPECT_EQ(actionFrames[0], actionFrames[1]);
}

TEST(SupLanRuntime, ActionTriggerUsesThreeAttemptsAndLocalFailureWaits) {
  RuntimePair pair;
  const ResourceId actionResource = {
      Supla::SupLan::kResourceTypeChannel, 50002};
  ASSERT_TRUE(pair.runtimeA.requestRead(pair.actionPeerA, actionResource));
  pair.pump(20);

  uint8_t payload[15] = {};
  payload[0] = 0xFF;
  payload[1] = 0x01;
  uint32_t actionCalls = pair.appA.actionCalls;
  uint32_t retries = pair.runtimeB.diagnostics().retryTx;
  const size_t historyStart = pair.network.history.size();
  pair.runtimeA.testHooks()->dropNextAckTx = 10;
  ASSERT_TRUE(pair.runtimeB.publishAction(pair.actionPeerB, actionResource,
                                           payload, sizeof(payload)));
  pair.pumpMs(600);
  EXPECT_EQ(pair.appA.actionCalls, actionCalls + 1U);
  EXPECT_EQ(pair.runtimeB.diagnostics().retryTx, retries + 2U);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().retries.used, 0U);
  std::vector<std::vector<uint8_t>> frames =
      protectedDataFromB(pair, historyStart);
  ASSERT_EQ(frames.size(), 3U);
  EXPECT_EQ(frames[0], frames[1]);
  EXPECT_EQ(frames[1], frames[2]);
  pair.runtimeA.testHooks()->dropNextAckTx = 0;

  payload[1] = 0x02;
  actionCalls = pair.appA.actionCalls;
  retries = pair.runtimeB.diagnostics().retryTx;
  const uint32_t dataTx = pair.runtimeB.diagnostics().dataTx;
  pair.runtimeB.testHooks()->failNextDataTx = 1;
  ASSERT_TRUE(pair.runtimeB.publishAction(pair.actionPeerB, actionResource,
                                           payload, sizeof(payload)));
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().deferredEvents.used, 0U);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().retries.used, 1U);
  EXPECT_EQ(pair.runtimeB.diagnostics().dataTx, dataTx);
  pair.pumpMs(140);
  EXPECT_EQ(pair.runtimeB.diagnostics().retryTx, retries);
  EXPECT_EQ(pair.runtimeB.diagnostics().dataTx, dataTx);
  pair.pumpMs(20);
  EXPECT_EQ(pair.appA.actionCalls, actionCalls + 1U);
  EXPECT_EQ(pair.runtimeB.diagnostics().retryTx, retries + 1U);
  EXPECT_EQ(pair.runtimeB.diagnostics().dataTx, dataTx + 1U);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().retries.used, 0U);
}

TEST(SupLanRuntime, ActionTriggerCanEstablishSessionAndExpiresWithoutReplay) {
  RuntimePair pair;
  const ResourceId actionResource = {
      Supla::SupLan::kResourceTypeChannel, 50002};
  ASSERT_TRUE(pair.runtimeA.requestRead(pair.actionPeerA, actionResource));
  pair.pump(20);
  ASSERT_EQ(pair.runtimeB.poolDiagnostics().sessions.used, 1U);

  EXPECT_TRUE(pair.runtimeB.forgetSession(pair.actionPeerB));
  uint8_t payload[15] = {};
  payload[0] = 0xFF;
  payload[1] = 0x11;
  ASSERT_TRUE(pair.runtimeB.publishAction(pair.actionPeerB, actionResource,
                                           payload, sizeof(payload)));
  pair.pump(30);
  EXPECT_EQ(pair.appA.actionCalls, 1U);
  EXPECT_EQ(pair.appA.lastActionId, 0x11U);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().sessions.used, 1U);

  EXPECT_TRUE(pair.runtimeB.forgetSession(pair.actionPeerB));
  pair.runtimeB.clearEndpoint(pair.actionPeerB);
  pair.datagramsA.available = false;
  const uint32_t actionTx = pair.runtimeB.diagnostics().actionTx;
  ASSERT_TRUE(pair.runtimeB.publishAction(pair.actionPeerB, actionResource,
                                           payload, sizeof(payload)));
  pair.pumpMs(3100);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().deferredEvents.used, 0U);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().locates.used, 0U);
  EXPECT_EQ(pair.runtimeB.diagnostics().actionTx, actionTx);

  pair.datagramsA.available = true;
  const uint32_t sessionInitTx = pair.runtimeB.diagnostics().sessionInitTx;
  pair.pumpMs(500);
  EXPECT_EQ(pair.appA.actionCalls, 1U);
  EXPECT_EQ(pair.runtimeB.diagnostics().actionTx, actionTx);
  EXPECT_EQ(pair.runtimeB.diagnostics().sessionInitTx, sessionInitTx);
}

TEST(SupLanRuntime, ActionTriggerCanUseLocateWithoutAnUnrelatedRead) {
  RuntimePair pair;
  const ResourceId actionResource = {
      Supla::SupLan::kResourceTypeChannel, 50002};
  ASSERT_TRUE(pair.runtimeA.requestRead(pair.actionPeerA, actionResource));
  pair.pump(20);
  ASSERT_EQ(pair.runtimeB.poolDiagnostics().interests.used, 1U);
  ASSERT_EQ(pair.runtimeB.poolDiagnostics().sessions.used, 1U);

  pair.runtimeB.clearEndpoint(pair.actionPeerB);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().sessions.used, 0U);
  const uint32_t locateTx = pair.runtimeB.diagnostics().locateTx;
  const uint32_t locateRx = pair.runtimeA.diagnostics().locateRx;
  const uint32_t actionCalls = pair.appA.actionCalls;
  const uint32_t ackRx = pair.runtimeB.diagnostics().ackRx;
  uint8_t payload[15] = {};
  payload[0] = 0xFF;
  payload[1] = 0xCA;
  payload[2] = 0xFE;

  ASSERT_TRUE(pair.runtimeB.publishAction(pair.actionPeerB, actionResource,
                                           payload, sizeof(payload)));
  pair.pump(40);

  EXPECT_EQ(pair.runtimeB.diagnostics().locateTx, locateTx + 1U);
  EXPECT_EQ(pair.runtimeA.diagnostics().locateRx, locateRx + 1U);
  EXPECT_EQ(pair.appA.actionCalls, actionCalls + 1U);
  EXPECT_EQ(pair.appA.lastActionId, 0xFECAU);
  EXPECT_EQ(pair.runtimeB.diagnostics().ackRx, ackRx + 1U);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().sessions.used, 1U);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().retries.used, 0U);
}

TEST(SupLanRuntime, ActionTriggerIsNotReissuedAcrossReplacementSession) {
  RuntimePair pair;
  const ResourceId actionResource = {
      Supla::SupLan::kResourceTypeChannel, 50002};
  ASSERT_TRUE(pair.runtimeA.requestRead(pair.actionPeerA, actionResource));
  pair.pump(20);
  uint8_t payload[15] = {};
  payload[0] = 0xFF;
  payload[1] = 0xA5;
  pair.runtimeA.testHooks()->dropNextAckTx = 1;
  ASSERT_TRUE(pair.runtimeB.publishAction(pair.actionPeerB, actionResource,
                                           payload, sizeof(payload)));
  pair.pump(2);
  ASSERT_EQ(pair.appA.actionCalls, 1U);
  ASSERT_EQ(pair.runtimeB.poolDiagnostics().retries.used, 1U);

  EXPECT_TRUE(pair.runtimeA.forgetSession(pair.actionPeerA));
  EXPECT_TRUE(pair.runtimeB.forgetSession(pair.actionPeerB));
  ASSERT_TRUE(pair.runtimeA.requestRead(pair.actionPeerA, actionResource));
  pair.pump(30);
  EXPECT_EQ(pair.runtimeA.poolDiagnostics().sessions.used, 1U);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().sessions.used, 1U);
  EXPECT_EQ(pair.appA.actionCalls, 1U);
  pair.pumpMs(500);
  EXPECT_EQ(pair.appA.actionCalls, 1U);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().retries.used, 0U);
}

TEST(SupLanRuntime, ReplayedSessionInitDoesNotMoveActiveEndpoint) {
  RuntimePair pair;
  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
  pair.pump(20);
  ASSERT_EQ(pair.runtimeA.diagnostics().sessionEstablished, 1U);
  ASSERT_EQ(pair.peersA.get(pair.peerA)->endpoint.port,
            pair.endpointB.port);

  Packet replay = {};
  bool foundInit = false;
  for (size_t i = 0; i < pair.network.history.size(); ++i) {
    const Packet &packet = pair.network.history[i];
    if (!packet.multicast && packet.bytes.size() ==
            Supla::SupLan::kSessionInitSize &&
        packet.bytes[0] == Supla::SupLan::kVersion &&
        packet.bytes[1] == Supla::SupLan::kFrameSessionInit) {
      replay = packet;
      foundInit = true;
      break;
    }
  }
  ASSERT_TRUE(foundInit);
  replay.source = {0x0300007F, 9999};
  replay.destination = pair.endpointA;
  pair.network.packets.push_back(replay);
  pair.pump(2);

  EXPECT_EQ(pair.peersA.get(pair.peerA)->endpoint.address,
            pair.endpointB.address);
  EXPECT_EQ(pair.peersA.get(pair.peerA)->endpoint.port,
            pair.endpointB.port);
  uint8_t control[17] = {};
  control[4] = 0xFF;
  control[9] = 1;
  ASSERT_TRUE(pair.runtimeB.sendControl(pair.peerB, pair.resource, control,
                                        sizeof(control)));
  pair.pump(8);
  EXPECT_EQ(pair.appA.controlCalls, 1U);
}

TEST(SupLanRuntime, TransmittedControlNeverReplaysInReplacementSession) {
  RuntimePair pair;
  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
  pair.pump(20);
  ASSERT_TRUE(pair.runtimeA.forgetSession(pair.peerA));

  uint8_t control[17] = {};
  control[4] = 0xFF;
  control[9] = 1;
  ASSERT_TRUE(pair.runtimeB.sendControl(pair.peerB, pair.resource, control,
                                        sizeof(control)));
  pair.pump(60);
  EXPECT_EQ(pair.appA.controlCalls, 0U);
  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
  pair.pump(30);
  EXPECT_EQ(pair.runtimeB.diagnostics().sessionInitTx, 2U);
  EXPECT_EQ(pair.runtimeB.diagnostics().sessionEstablished, 2U);
  EXPECT_EQ(pair.runtimeA.diagnostics().sessionEstablished, 2U);
  EXPECT_EQ(pair.appA.controlCalls, 0U);
  EXPECT_EQ(pair.appB.acknowledged, 2U);
  EXPECT_EQ(pair.runtimeA.diagnostics().readDispatched, 2U);
  EXPECT_EQ(pair.runtimeA.poolDiagnostics().sessions.used, 1U);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().sessions.used, 1U);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().retries.used, 0U);
}

TEST(SupLanRuntime, DeferredControlWaitsForRetrySlot) {
  RuntimePair pair;
  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
  pair.pump(20);
  pair.runtimeA.testHooks()->dropNextAckTx = 2;
  uint8_t control[17] = {};
  control[4] = 0xFF;
  control[9] = 1;
  for (uint8_t i = 0; i < 3; ++i) {
    ASSERT_TRUE(pair.runtimeB.sendControl(pair.peerB, pair.resource, control,
                                          sizeof(control)));
  }

  pair.pump(5);
  EXPECT_EQ(pair.appA.controlCalls, 2U);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().retries.used, 2U);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().deferredEvents.used, 1U);

  pair.network.now += Supla::SupLan::kAckRetryMs + 1;
  pair.runtimeB.iterate();
  pair.pump(20);

  EXPECT_EQ(pair.appA.controlCalls, 3U);
  EXPECT_EQ(pair.runtimeA.diagnostics().controlDuplicateSuppressed, 2U);
  EXPECT_EQ(pair.appB.acknowledged, 4U);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().retries.used, 0U);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().deferredEvents.used, 0U);
}

TEST(SupLanRuntime,
     ActionTriggerPoolExhaustionPreservesControlRetriesAndDoesNotReplay) {
  RuntimePair pair;
  const ResourceId actionResource = {
      Supla::SupLan::kResourceTypeChannel, 50002};
  ASSERT_TRUE(pair.runtimeA.requestRead(pair.actionPeerA, actionResource));
  pair.pump(20);
  ASSERT_EQ(pair.runtimeB.poolDiagnostics().sessions.used, 1U);

  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
  pair.pump(20);
  ASSERT_EQ(pair.runtimeB.poolDiagnostics().sessions.used, 2U);

  // Keep both shared retry slots occupied by acknowledged CONTROL operations.
  pair.runtimeA.testHooks()->dropNextAckTx = 2;
  const uint32_t acknowledgedBeforeControls = pair.appB.acknowledged;
  uint8_t control[17] = {};
  control[4] = 0xFF;
  control[9] = 1;
  ASSERT_TRUE(pair.runtimeB.sendControl(pair.peerB, pair.resource, control,
                                        sizeof(control)));
  control[9] = 0;
  ASSERT_TRUE(pair.runtimeB.sendControl(pair.peerB, pair.resource, control,
                                        sizeof(control)));
  ASSERT_EQ(pair.runtimeB.poolDiagnostics().retries.used, 2U);

  uint8_t action[15] = {};
  action[0] = 0xFF;
  action[1] = 0x34;
  ASSERT_TRUE(pair.runtimeB.publishAction(pair.actionPeerB, actionResource,
                                          action, sizeof(action)));
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().retries.used, 2U);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().deferredEvents.used, 0U);
  EXPECT_EQ(pair.runtimeB.diagnostics().actionTx, 0U);
  EXPECT_EQ(pair.appA.actionCalls, 0U);
  EXPECT_EQ(pair.runtimeB.diagnostics().poolReject, 1U);

  pair.pump(5);
  EXPECT_EQ(pair.appA.controlCalls, 2U);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().retries.used, 2U);

  pair.network.now += Supla::SupLan::kAckRetryMs + 1;
  pair.pump(20);
  EXPECT_EQ(pair.appA.controlCalls, 2U);
  EXPECT_EQ(pair.appB.acknowledged, acknowledgedBeforeControls + 2U);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().retries.used, 0U);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().deferredEvents.used, 0U);
  EXPECT_EQ(pair.appA.actionCalls, 0U);
  EXPECT_EQ(pair.runtimeB.diagnostics().actionTx, 0U);
}

TEST(SupLanRuntime, TamperedSessionAcceptIsRejectedThenRetryRecovers) {
  RuntimePair pair;
  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
  pair.pump(20);
  ASSERT_EQ(pair.runtimeB.diagnostics().sessionEstablished, 1U);
  ASSERT_EQ(pair.appB.stateCalls, 1U);

  ASSERT_TRUE(pair.runtimeB.forgetSession(pair.peerB));
  pair.runtimeB.testHooks()->pauseSessionInitRetries = 1;
  pair.runtimeA.testHooks()->corruptNextSessionMacTx = 1;
  const uint32_t establishedBefore =
      pair.runtimeB.diagnostics().sessionEstablished;
  const uint32_t readsBefore = pair.runtimeA.diagnostics().readDispatched;
  const uint32_t statesBefore = pair.appB.stateCalls;
  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
  pair.pump(5);

  EXPECT_EQ(pair.runtimeB.diagnostics().invalidSessionDrop, 1U);
  EXPECT_EQ(pair.runtimeB.diagnostics().sessionEstablished,
            establishedBefore);
  EXPECT_EQ(pair.runtimeA.diagnostics().readDispatched, readsBefore);
  EXPECT_EQ(pair.appB.stateCalls, statesBefore);

  pair.network.now += Supla::SupLan::kSessionInitRetryMs + 1;
  const uint32_t initTransmitsBeforeRetry =
      pair.runtimeB.diagnostics().sessionInitTx;
  pair.pump(2);
  EXPECT_EQ(pair.runtimeB.diagnostics().sessionInitTx,
            initTransmitsBeforeRetry);
  pair.runtimeB.testHooks()->pauseSessionInitRetries = 0;
  pair.pump(8);
  EXPECT_EQ(pair.runtimeB.diagnostics().sessionEstablished,
            establishedBefore + 1);
  EXPECT_EQ(pair.runtimeA.diagnostics().readDispatched, readsBefore + 1);
  EXPECT_EQ(pair.appB.stateCalls, statesBefore + 1);
}


// ADR-008 regression: old characterization expectations are forbidden.
TEST(SupLanDiscoveryAudit, MissingPeerStopsAfterThreeLocateAttempts) {
  RuntimePair pair;
  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
  pair.runtimeB.iterate();
  for (uint32_t now = 1; now <= 5000; ++now) {
    pair.network.packets.clear();
    pair.network.now = now;
    pair.runtimeB.iterate();
    EXPECT_LE(pair.runtimeB.diagnostics().locateTx, 3U);
  }
  EXPECT_EQ(pair.runtimeB.diagnostics().locateTx, 3U);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().deferredEvents.used, 0U);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().retries.used, 0U);
  EXPECT_TRUE(pair.runtimeB.readNeedsRefresh(pair.peerB, pair.resource));
  EXPECT_FALSE(pair.runtimeB.recoveryStatus(pair.peerB).active);
  EXPECT_EQ(pair.runtimeB.diagnostics().sessionInitTx, 0U);
}

TEST(SupLanDiscoveryAudit, LostQueryAndLateJoiningPeerRecoverOnNextAttempt) {
  RuntimePair pair;
  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
  pair.runtimeB.iterate();
  for (int i = 0; i < 2; ++i) {
    pair.network.packets.clear();
    pair.network.now += Supla::SupLan::kLocateReplyWindowMs;
    pair.runtimeB.iterate();
  }
  pair.pump(10);
  EXPECT_EQ(pair.runtimeB.diagnostics().locateTx, 3U);
  EXPECT_EQ(pair.runtimeA.diagnostics().readDispatched, 1U);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().deferredEvents.used, 0U);
}

TEST(SupLanDiscoveryAudit, LostReplyRecoversWithFreshQuery) {
  RuntimePair pair;
  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
  pair.runtimeB.iterate();
  pair.runtimeA.iterate();
  ASSERT_EQ(pair.network.packets.size(), 1U);
  ASSERT_EQ(pair.network.packets[0].bytes.size(), 34U);
  pair.network.packets.clear();
  pair.network.now = Supla::SupLan::kLocateReplyWindowMs;
  pair.runtimeB.iterate();
  pair.pump(10);
  EXPECT_EQ(pair.runtimeB.diagnostics().locateTx, 2U);
  EXPECT_EQ(pair.runtimeA.diagnostics().readDispatched, 1U);
}

TEST(SupLanDiscoveryAudit, ReplyJustBeforeDeadlineIsAccepted) {
  RuntimePair pair;
  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
  pair.runtimeB.iterate();
  pair.runtimeA.iterate();
  pair.network.now = Supla::SupLan::kLocateReplyWindowMs - 1;
  pair.runtimeB.iterate();
  EXPECT_EQ(pair.runtimeB.diagnostics().locateReplyRx, 1U);
  pair.pump(10);
  EXPECT_EQ(pair.runtimeA.diagnostics().readDispatched, 1U);
}

TEST(SupLanDiscoveryAudit, ReplyAtDeadlineIsRejectedEvenIfAlreadyQueued) {
  RuntimePair pair;
  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
  pair.runtimeB.iterate();
  pair.runtimeA.iterate();
  pair.network.now = Supla::SupLan::kLocateReplyWindowMs;
  pair.runtimeB.iterate();
  EXPECT_EQ(pair.runtimeB.diagnostics().locateReplyRx, 0U);
  EXPECT_EQ(pair.runtimeB.diagnostics().invalidLocateDrop, 1U);
  EXPECT_EQ(pair.runtimeB.diagnostics().locateTx, 2U);
}

TEST(SupLanDiscoveryAudit, DuplicateQueryIsRateLimitedAndReplyConsumedOnce) {
  RuntimePair pair;
  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
  pair.runtimeB.iterate();
  pair.network.packets.push_back(pair.network.packets.front());
  pair.runtimeA.iterate();
  EXPECT_EQ(pair.runtimeA.diagnostics().locateRx, 2U);
  EXPECT_EQ(pair.runtimeA.diagnostics().locateReplyTx, 1U);
  ASSERT_EQ(pair.network.packets.size(), 1U);
  pair.network.packets.push_back(pair.network.packets.front());
  pair.runtimeB.iterate();
  EXPECT_EQ(pair.runtimeB.diagnostics().locateReplyRx, 1U);
  EXPECT_EQ(pair.runtimeB.diagnostics().invalidLocateDrop, 1U);
  EXPECT_EQ(pair.runtimeB.diagnostics().sessionInitTx, 1U);
  pair.pump(10);
  EXPECT_EQ(pair.runtimeA.diagnostics().readDispatched, 1U);
}

TEST(SupLanDiscoveryAudit, ReorderedOldReplyCannotReplaceNewCandidate) {
  RuntimePair pair;
  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
  pair.runtimeB.iterate();
  pair.runtimeA.iterate();
  ASSERT_EQ(pair.network.packets.size(), 1U);
  const Packet oldReply = pair.network.packets.front();
  pair.network.packets.clear();
  pair.network.now = Supla::SupLan::kLocateReplyWindowMs;
  pair.runtimeB.iterate();
  pair.runtimeA.iterate();
  pair.network.packets.push_back(oldReply);
  pair.runtimeB.iterate();
  EXPECT_EQ(pair.runtimeB.diagnostics().locateReplyRx, 1U);
  EXPECT_EQ(pair.runtimeB.diagnostics().invalidLocateDrop, 1U);
  pair.pump(10);
  EXPECT_EQ(pair.runtimeA.diagnostics().readDispatched, 1U);
}

TEST(SupLanDiscoveryAudit,
     ConcurrentReadRequestsCoalesceAndShareControlRecovery) {
  RuntimePair pair;
  uint8_t control[17] = {};
  control[4] = 0xFF;
  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
  pair.runtimeB.iterate();
  ASSERT_TRUE(pair.runtimeB.sendControl(pair.peerB, pair.resource,
                                        control, sizeof(control)));
  for (unsigned i = 2; i < SUPLAN_MAX_DEFERRED_APP_EVENTS; ++i) {
    ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
  }
  EXPECT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
  EXPECT_EQ(pair.runtimeB.diagnostics().locateTx, 1U);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().locates.used, 1U);
  pair.pump(20);
  EXPECT_EQ(pair.appA.controlCalls, 1U);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().deferredEvents.used, 0U);
  EXPECT_EQ(pair.runtimeA.diagnostics().readDispatched, 1U);
}

TEST(SupLanDiscoveryAudit, UnresponsiveCandidateIsRetiredWithinCycle) {
  RuntimePair pair;
  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
  pair.runtimeB.iterate();
  pair.runtimeA.iterate();
  pair.runtimeB.iterate();
  for (unsigned i = 0; i < 15; ++i) {
    pair.network.packets.clear();
    pair.network.now += 250;
    pair.runtimeB.iterate();
  }
  EXPECT_EQ(pair.runtimeB.diagnostics().sessionInitTx, 3U);
  EXPECT_EQ(pair.runtimeB.diagnostics().locateTx, 2U);
  EXPECT_EQ(pair.peersB.get(pair.peerB)->endpointState,
            Supla::SupLan::kPeerEndpointNone);
  // Fresh discovery succeeds before the original 5 s deadline.
  pair.pump(20);
  EXPECT_EQ(pair.runtimeA.diagnostics().readDispatched, 1U);
  EXPECT_FALSE(pair.runtimeB.readNeedsRefresh(pair.peerB, pair.resource));
}

TEST(SupLanDiscoveryAudit,
     ReadRetriesStaleSessionAndRecoversAtRetainedEndpoint) {
  RuntimePair pair;
  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
  pair.runtimeB.iterate();
  pair.pump(20);
  ASSERT_TRUE(pair.runtimeA.forgetSession(pair.peerA));
  const uint32_t readsBefore = pair.runtimeA.diagnostics().readDispatched;
  const uint32_t statesBefore = pair.appB.stateCalls;
  const uint32_t locateTxBefore = pair.runtimeB.diagnostics().locateTx;
  const size_t historyStart = pair.network.history.size();
  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
  pair.pump(2);
  for (uint8_t i = 0; i < Supla::SupLan::kAckMaxAttempts; ++i) {
    pair.network.now += Supla::SupLan::kAckRetryMs + 1;
    pair.runtimeB.iterate();
    pair.runtimeA.iterate();
  }

  std::vector<std::vector<uint8_t> > staleReadTransmissions;
  for (size_t i = historyStart; i < pair.network.history.size(); ++i) {
    const Packet &packet = pair.network.history[i];
    if (!packet.multicast &&
        packet.source.port == pair.endpointB.port &&
        packet.destination.port == pair.endpointA.port &&
        packet.bytes.size() > 2 &&
        packet.bytes[0] == Supla::SupLan::kAdaptationFull &&
        packet.bytes[1] == Supla::SupLan::kVersion &&
        packet.bytes[2] == Supla::SupLan::kFrameData) {
      staleReadTransmissions.push_back(packet.bytes);
    }
  }
  ASSERT_EQ(staleReadTransmissions.size(), 3U);
  EXPECT_EQ(staleReadTransmissions[0], staleReadTransmissions[1]);
  EXPECT_EQ(staleReadTransmissions[1], staleReadTransmissions[2]);
  EXPECT_EQ(pair.runtimeB.diagnostics().locateTx, locateTxBefore);
  EXPECT_EQ(pair.runtimeB.diagnostics().sessionInitTx, 2U);
  const size_t historyAfterStaleRetries = pair.network.history.size();
  pair.pump(20);

  EXPECT_EQ(pair.runtimeA.diagnostics().readDispatched, readsBefore + 1);
  EXPECT_EQ(pair.appB.stateCalls, statesBefore + 1);
  EXPECT_EQ(pair.appB.acknowledged, 2U);
  EXPECT_EQ(pair.runtimeB.diagnostics().sessionEstablished, 2U);
  EXPECT_EQ(pair.runtimeA.diagnostics().sessionEstablished, 2U);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().sessions.used, 1U);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().retries.used, 0U);
  bool foundNewRead = false;
  for (size_t i = historyAfterStaleRetries; i < pair.network.history.size();
       ++i) {
    const Packet &packet = pair.network.history[i];
    if (!packet.multicast &&
        packet.source.port == pair.endpointB.port &&
        packet.destination.port == pair.endpointA.port &&
        packet.bytes.size() > 2 &&
        packet.bytes[0] == Supla::SupLan::kAdaptationFull &&
        packet.bytes[1] == Supla::SupLan::kVersion &&
        packet.bytes[2] == Supla::SupLan::kFrameData) {
      foundNewRead = true;
      EXPECT_NE(packet.bytes, staleReadTransmissions[0]);
    }
  }
  EXPECT_TRUE(foundNewRead);
}

TEST(SupLanDiscoveryAudit,
     ReadRecoveryFallsBackToAuthenticatedLocateWhenDirectSessionFails) {
  RuntimePair pair;
  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
  pair.runtimeB.iterate();
  pair.pump(20);
  ASSERT_TRUE(pair.runtimeA.forgetSession(pair.peerA));
  pair.runtimeA.testHooks()->dropNextSessionAcceptTx = 0xFF;
  const uint32_t locateTxBefore = pair.runtimeB.diagnostics().locateTx;
  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));

  for (uint8_t i = 0; i < 32 &&
       pair.runtimeB.diagnostics().locateTx == locateTxBefore; ++i) {
    pair.network.now += Supla::SupLan::kSessionInitRetryMs;
    pair.runtimeB.iterate();
    pair.runtimeA.iterate();
  }
  ASSERT_GT(pair.runtimeB.diagnostics().locateTx, locateTxBefore);
  pair.runtimeA.testHooks()->dropNextSessionAcceptTx = 0;
  pair.pump(20);

  EXPECT_EQ(pair.runtimeB.diagnostics().sessionEstablished, 2U);
  EXPECT_EQ(pair.runtimeA.diagnostics().readDispatched, 2U);
  EXPECT_EQ(pair.appB.stateCalls, 2U);
  EXPECT_EQ(pair.runtimeB.diagnostics().locateReplyRx, 2U);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().retries.used, 0U);
}


TEST(SupLanDiscoveryAudit, SameIpDistinctPortsPreservedAcrossEntireExchange) {
  RuntimePair pair(true);
  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
  pair.runtimeB.iterate();
  ASSERT_EQ(pair.network.packets.size(), 1U);
  const Packet query = pair.network.packets.front();
  EXPECT_EQ(query.destination.port, 2016U);
  EXPECT_EQ(query.source.port, pair.endpointB.port);
  pair.runtimeA.iterate();
  ASSERT_EQ(pair.network.packets.size(), 1U);
  const Packet reply = pair.network.packets.front();
  EXPECT_EQ(reply.source.port, pair.endpointA.port);
  EXPECT_EQ(reply.destination.port, pair.endpointB.port);
  pair.pump(10);
  EXPECT_EQ(pair.runtimeA.diagnostics().readDispatched, 1U);
  EXPECT_EQ(pair.appB.stateCalls, 1U);
  for (const Packet &packet : pair.network.history) {
    if (packet.multicast) continue;
    EXPECT_EQ(packet.destination.address, pair.endpointA.address);
    EXPECT_EQ(packet.destination.port,
              packet.source.port == pair.endpointA.port
                  ? pair.endpointB.port : pair.endpointA.port);
  }
}

TEST(SupLanDiscoveryAudit, FailedMulticastSendConsumesSpacedAttempts) {
  RuntimePair pair;
  pair.network.failMulticast = true;
  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
  pair.runtimeB.iterate();
  for (unsigned i = 0; i < 50; ++i) pair.runtimeB.iterate();
  EXPECT_EQ(pair.network.multicastAttempts, 1U);
  pair.network.now = 249;
  pair.runtimeB.iterate();
  EXPECT_EQ(pair.network.multicastAttempts, 1U);
  pair.network.now = 250;
  pair.runtimeB.iterate();
  EXPECT_EQ(pair.network.multicastAttempts, 2U);
  pair.network.now = 500;
  pair.runtimeB.iterate();
  EXPECT_EQ(pair.network.multicastAttempts, 3U);
  pair.network.now = 750;
  pair.runtimeB.iterate();
  EXPECT_EQ(pair.network.multicastAttempts, 3U);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().locates.used, 0U);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().deferredEvents.used, 0U);
  EXPECT_TRUE(pair.runtimeB.readNeedsRefresh(pair.peerB, pair.resource));
}


TEST(SupLanDiscoveryAudit, SourceLocatesInterestedDestinationAfterIpLoss) {
  RuntimePair pair;
  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
  pair.runtimeB.iterate();
  pair.pump(10);
  pair.runtimeA.clearEndpoint(pair.peerA);
  ASSERT_EQ(pair.runtimeA.poolDiagnostics().interests.used, 1U);
  const uint32_t statesBefore = pair.appB.stateCalls;
  const uint32_t locateTxBefore = pair.runtimeA.diagnostics().locateTx;
  const uint32_t locateReplyTxBefore =
      pair.runtimeB.diagnostics().locateReplyTx;
  uint8_t state[14] = {};
  state[0] = 0xFF;
  ASSERT_TRUE(pair.runtimeA.publishState(
      pair.peerA, pair.resource,
      Supla::SupLan::kSuplaCallDeviceChannelValueChangedC,
      state, sizeof(state)));
  pair.pump(60);
  EXPECT_EQ(pair.runtimeA.diagnostics().locateTx, locateTxBefore + 1U);
  EXPECT_EQ(pair.runtimeB.diagnostics().locateReplyTx,
            locateReplyTxBefore + 1U);
  EXPECT_EQ(pair.appB.stateCalls, statesBefore + 1U);
  EXPECT_EQ(pair.runtimeA.poolDiagnostics().deferredEvents.used, 0U);
}

TEST(SupLanRuntime, BindingReceiveEnvelopeExceedsLocalOutboundCapacity) {
  using namespace Supla::SupLan;  // NOLINT(build/namespaces)
  RuntimePair pair;
  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
  pair.pumpMs(200);
  PeerMaterial material;
  ASSERT_TRUE(pair.peersA.materialFor(pair.peerA, &material));
  std::vector<uint8_t> initFrame, acceptFrame;
  for (const auto &packet : pair.network.history) {
    if (packet.bytes.size() == kSessionInitSize)
      initFrame = packet.bytes;
    if (packet.bytes.size() == kSessionAcceptSize)
      acceptFrame = packet.bytes;
  }
  ASSERT_EQ(initFrame.size(), kSessionInitSize);
  ASSERT_EQ(acceptFrame.size(), kSessionAcceptSize);
  SessionInit init;
  SessionAccept accept;
  ASSERT_TRUE(decodeSessionInit(material.initMacKey, initFrame.data(),
                                initFrame.size(), &init));
  ASSERT_TRUE(decodeSessionAccept(material.acceptMacKey, initFrame.data(),
      &init, acceptFrame.data(), acceptFrame.size(), &accept));
  SessionKeys keys;
  uint8_t transcriptHash[32];
  ASSERT_TRUE(deriveSessionKeys(material.peerKey, material.contextHash,
      init.ni, accept.nr, initFrame.data(), initFrame.size(),
      acceptFrame.data(), acceptFrame.size(), accept.sessionId,
      &keys, transcriptHash));
  // Eighty-two entries fit the public 1024-byte RX envelope, independently
  // of this Device's smaller local outbound/persistent binding capacity.
  std::vector<uint8_t> body(1 + 82 * 12, 0);
  body[0] = 82;
  std::vector<uint8_t> application(6 + body.size(), 0);
  application[0] = kMessageClassNative;
  putUint32(application.data() + 1, 8);
  application[5] = kAckRequired;
  std::memcpy(application.data() + 6, body.data(), body.size());
  EXPECT_GT(application.size(), SUPLAN_MAX_APPLICATION_BYTES);
  std::vector<uint8_t> frame(application.size() + 32);
  const auto length = frame.size();
  frame[0] = kVersion;
  frame[1] = kFrameData;
  putUint64(frame.data() + 2, accept.sessionId);
  putUint32(frame.data() + 10, 100);
  putUint16(frame.data() + 14, application.size());
  uint8_t nonce[kAeadNonceSize];
  std::memcpy(nonce, keys.responderToInitiator.noncePrefix, kNoncePrefixSize);
  putUint32(nonce + kNoncePrefixSize, 100);
  ASSERT_TRUE(pair.crypto.aes128CcmEncrypt(keys.responderToInitiator.trafficKey,
      nonce, frame.data(), kProtectedHeaderSize, application.data(),
      application.size(), frame.data() + kProtectedHeaderSize,
      frame.data() + kProtectedHeaderSize + application.size()));
  FragmentSender sender;
  const auto transmit = [](void *context, const uint8_t *data, size_t size) {
    auto pair = static_cast<RuntimePair *>(context);
    return pair->network.send(pair->endpointA, pair->endpointB, false,
                               data, size);
  };
  ASSERT_TRUE(sender.send(frame.data(), length, 250, 100,
                          transmit, &pair));
  pair.pumpMs(100);
  EXPECT_EQ(pair.appB.bindingCalls, 1);
  EXPECT_EQ(pair.appB.lastBindings, body);
}

TEST(SupLanRuntime, SourceFirstBindingDiscoveryUsesBoundedRecoveryBackoff) {
  RuntimePair pair;
  pair.appA.bindingPoolEnabled = true;
  pair.datagramsB.available = false;
  uint8_t body[13] = {1, 1, 1};
  body[7] = 1;
  Supla::SupLan::putUint32(body + 3, pair.resource.id);
  Supla::SupLan::putUint32(body + 8, 60001);
  for (int i = 0; i < 1200; ++i) {
    EXPECT_FALSE(pair.runtimeA.sendBindings(pair.peerA, body, sizeof(body)));
    ++pair.network.now;
    pair.runtimeA.iterate();
    pair.runtimeB.iterate();
  }
  EXPECT_LE(pair.network.multicastAttempts, 3u);
  EXPECT_FALSE(pair.runtimeA.recoveryStatus(pair.peerA).active);
  const auto attempts = pair.network.multicastAttempts;
  for (int i = 0; i < 4000; ++i) {
    EXPECT_FALSE(pair.runtimeA.sendBindings(pair.peerA, body, sizeof(body)));
    ++pair.network.now;
    pair.runtimeA.iterate();
    pair.runtimeB.iterate();
  }
  EXPECT_EQ(pair.network.multicastAttempts, attempts);
  pair.datagramsB.available = true;
  for (int i = 0; i < 7000 && !pair.runtimeA.bindingsInFlight(); ++i) {
    pair.runtimeA.sendBindings(pair.peerA, body, sizeof(body));
    ++pair.network.now;
    pair.runtimeA.iterate();
    pair.runtimeB.iterate();
  }
  EXPECT_GT(pair.network.multicastAttempts, attempts);
  pair.pumpMs(300);
  EXPECT_GE(pair.appB.bindingCalls, 1);
}

TEST(SupLanRuntime, DelayedOlderBindingsCannotRestoreRemovedTopology) {
  RuntimePair pair;
  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
  pair.pumpMs(200);
  pair.appA.bindingPoolEnabled = true;
  uint8_t body[13] = {1, 1, 1};
  Supla::SupLan::putUint32(body + 3, pair.resource.id);
  body[7] = 1;
  Supla::SupLan::putUint32(body + 8, 60001);
  ASSERT_TRUE(pair.runtimeA.sendBindings(pair.peerA, body, sizeof(body)));
  ASSERT_FALSE(pair.network.packets.empty());
  const Packet delayed = pair.network.packets.back();
  pair.network.packets.clear();
  // Let all protected retries expire before the first packet reaches B.
  for (int i = 0; i < 60; ++i) {
    pair.network.now += 10;
    pair.runtimeA.iterate();
    pair.network.packets.clear();
  }
  ASSERT_FALSE(pair.runtimeA.bindingsInFlight());
  const uint8_t empty = 0;
  ASSERT_TRUE(pair.runtimeA.sendBindings(pair.peerA, &empty, 1));
  pair.pumpMs(50);
  ASSERT_EQ(pair.appB.lastBindings, std::vector<uint8_t>{0});
  const int calls = pair.appB.bindingCalls;
  pair.network.packets.push_back(delayed);
  pair.pumpMs(50);
  EXPECT_EQ(pair.appB.bindingCalls, calls);
  EXPECT_EQ(pair.appB.lastBindings, std::vector<uint8_t>{0});
}

TEST(SupLanRuntime, InterleavedStateDoesNotSupersedeBinding) {
  RuntimePair pair;
  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
  pair.pumpMs(200);
  pair.appA.bindingPoolEnabled = true;
  uint8_t body[13] = {1, 1, 1};
  Supla::SupLan::putUint32(body + 3, pair.resource.id);
  body[7] = 1;
  Supla::SupLan::putUint32(body + 8, 60001);
  ASSERT_TRUE(pair.runtimeA.sendBindings(pair.peerA, body, sizeof(body)));
  ASSERT_FALSE(pair.network.packets.empty());
  const Packet delayed = pair.network.packets.back();
  pair.network.packets.clear();
  uint8_t state[14] = {0xff};
  ASSERT_TRUE(pair.runtimeA.publishState(pair.peerA, pair.resource,
      Supla::SupLan::kSuplaCallDeviceChannelValueChangedC,
      state, sizeof(state)));
  pair.pumpMs(50);
  const int states = pair.appB.stateCalls;
  EXPECT_GT(states, 0);
  EXPECT_EQ(pair.appB.bindingCalls, 0);
  pair.network.packets.push_back(delayed);
  pair.pumpMs(50);
  EXPECT_EQ(pair.appB.bindingCalls, 1);
  EXPECT_FALSE(pair.runtimeA.bindingsInFlight());
}

TEST(SupLanRuntime, BindingPersistenceFailureNeedsNewOperation) {
  RuntimePair pair;
  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
  pair.pumpMs(200);
  pair.appA.bindingPoolEnabled = true;
  pair.appB.bindingResult = 0;
  const uint8_t empty = 0;
  ASSERT_TRUE(pair.runtimeA.sendBindings(pair.peerA, &empty, 1));
  pair.pumpMs(50);
  ASSERT_EQ(pair.appB.bindingCalls, 1);
  EXPECT_TRUE(pair.runtimeA.bindingsInFlight());
  pair.appB.bindingResult = 3;
  pair.pumpMs(500);
  // Frozen replay semantics never redeliver even an unacknowledged duplicate.
  EXPECT_EQ(pair.appB.bindingCalls, 1);
  EXPECT_FALSE(pair.runtimeA.bindingsInFlight());
  ASSERT_TRUE(pair.runtimeA.sendBindings(pair.peerA, &empty, 1));
  pair.pumpMs(50);
  EXPECT_EQ(pair.appB.bindingCalls, 2);
  EXPECT_FALSE(pair.runtimeA.bindingsInFlight());
  EXPECT_EQ(pair.appA.lastAckResult, 3);
}

TEST(SupLanRuntime, ResourceBindingsReserveLargeRetryAndRetransmitExactBytes) {
  RuntimePair pair;
  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
  pair.pumpMs(200);
  std::vector<uint8_t> body(1 + 12 * 8, 0);
  body[0] = 8;
  for (int i = 0; i < 8; ++i) {
    auto entry = body.data() + 1 + 12 * i;
    entry[0] = entry[1] = entry[6] = 1;
    Supla::SupLan::putUint32(entry + 2, pair.resource.id);
    Supla::SupLan::putUint32(entry + 7, 60001 + i);
  }
  const auto before = pair.network.history.size();
  EXPECT_FALSE(
      pair.runtimeA.sendBindings(pair.peerA, body.data(), body.size()));
  EXPECT_EQ(pair.network.history.size(), before);
  pair.appA.bindingPoolEnabled = true;
  pair.runtimeB.testHooks()->dropNextAckTx = 1;
  ASSERT_TRUE(pair.runtimeA.sendBindings(pair.peerA, body.data(), body.size()));
  EXPECT_TRUE(pair.appA.largeRetry.length > SUPLAN_MAX_RETRY_FRAME_BYTES);
  EXPECT_FALSE(
      pair.runtimeA.sendBindings(pair.peerA, body.data(), body.size()));
  pair.pumpMs(400);
  EXPECT_EQ(pair.appB.bindingCalls, 1);
  EXPECT_EQ(pair.appB.lastBindings, body);
  EXPECT_FALSE(pair.runtimeA.bindingsInFlight());
  std::vector<std::vector<uint8_t>> sent;
  for (size_t i = before; i < pair.network.history.size(); ++i) {
    const auto &packet = pair.network.history[i];
    if (packet.source.address == pair.endpointA.address &&
        packet.bytes.size() > 100 &&
        packet.bytes[0] == Supla::SupLan::kAdaptationFull)
      sent.push_back(packet.bytes);
  }
  ASSERT_GE(sent.size(), 2u);
  EXPECT_EQ(sent[0], sent[1]);
  const auto after = pair.network.history.size();
  std::vector<uint8_t> overflow(1 + 12 * (SUPLAN_MAX_BINDINGS_PER_PEER + 1), 0);
  overflow[0] = SUPLAN_MAX_BINDINGS_PER_PEER + 1;
  EXPECT_FALSE(pair.runtimeA.sendBindings(pair.peerA, overflow.data(),
                                         overflow.size()));
  EXPECT_EQ(pair.network.history.size(), after);
  ASSERT_TRUE(pair.runtimeA.sendBindings(pair.peerA, body.data(), body.size()));
  pair.runtimeA.authorizationChanged(pair.peerA);
  EXPECT_FALSE(pair.runtimeA.bindingsInFlight());
  // Session replacement drops protected retry; no replay into a new SESSION.
  ASSERT_TRUE(pair.runtimeA.sendBindings(pair.peerA, body.data(), body.size()));
  ASSERT_TRUE(pair.runtimeA.forgetSession(pair.peerA));
  pair.runtimeA.iterate();
  EXPECT_FALSE(pair.runtimeA.bindingsInFlight());
}

}  // namespace

TEST(SupLanRuntime, ExpiredControlCannotExecuteWhenAbsentPeerReturns) {
  RuntimePair pair;
  pair.datagramsA.available = false;
  uint8_t control[17] = {};
  control[4] = 0xFF;
  control[9] = 1;
  ASSERT_TRUE(pair.runtimeB.sendControl(pair.peerB, pair.resource, control,
                                       sizeof(control)));
  pair.pumpMs(740);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().deferredEvents.used, 1U);
  pair.pumpMs(10);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().deferredEvents.used, 0U);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().locates.used, 0U);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().pending.used, 0U);
  pair.datagramsA.available = true;
  pair.pumpMs(1500);
  EXPECT_EQ(pair.appA.controlCalls, 0U);
  ASSERT_TRUE(pair.runtimeB.sendControl(pair.peerB, pair.resource, control,
                                       sizeof(control)));
  pair.pumpMs(500);
  EXPECT_EQ(pair.appA.controlCalls, 1U);
}

TEST(SupLanRuntime, ControlExpiryHandlesClockWrapAndLocalTransportFailure) {
  RuntimePair pair;
  pair.network.now = UINT32_MAX - 749;
  pair.datagramsB.available = false;
  uint8_t control[17] = {};
  control[4] = 0xFF;
  ASSERT_TRUE(pair.runtimeB.sendControl(pair.peerB, pair.resource, control,
                                       sizeof(control)));
  pair.pumpMs(740);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().deferredEvents.used, 1U);
  pair.pumpMs(10);  // Deadline is exactly zero, not an unset sentinel.
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().deferredEvents.used, 0U);
  pair.datagramsB.available = true;
  pair.pumpMs(500);
  EXPECT_EQ(pair.appA.controlCalls, 0U);
}

TEST(SupLanRuntime, ControlDeadlineSurvivesSessionRecovery) {
  RuntimePair pair;
  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
  pair.pumpMs(100);
  ASSERT_EQ(pair.runtimeB.poolDiagnostics().sessions.used, 1U);
  pair.datagramsA.available = false;
  uint8_t control[17] = {};
  control[4] = 0xFF;
  control[9] = 1;
  ASSERT_TRUE(pair.runtimeB.sendControl(pair.peerB, pair.resource, control,
                                       sizeof(control)));
  pair.pumpMs(4990);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().retries.used, 0U);
  pair.pumpMs(10);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().retries.used, 0U);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().pending.used, 0U);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().locates.used, 0U);
  // A real outage loses datagrams; do not deliver the old wire CONTROL.
  pair.network.packets.clear();
  pair.datagramsA.available = true;
  pair.pumpMs(1000);
  EXPECT_EQ(pair.appA.controlCalls, 0U);
}

TEST(SupLanRuntime, ControlExecutesIfPeerReturnsBeforeDeadline) {
  RuntimePair pair;
  pair.datagramsA.available = false;
  uint8_t control[17] = {};
  control[4] = 0xFF;
  control[9] = 1;
  ASSERT_TRUE(pair.runtimeB.sendControl(pair.peerB, pair.resource, control,
                                       sizeof(control)));
  pair.pumpMs(400);
  pair.network.packets.clear();
  pair.datagramsA.available = true;
  pair.pumpMs(500);
  EXPECT_EQ(pair.appA.controlCalls, 1U);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().retries.used, 0U);
}

TEST(SupLanRuntime, DeferredControlKeepsOriginalDeadlineAfterFirstData) {
  RuntimePair pair;
  pair.datagramsA.available = false;
  pair.peersB.get(pair.peerB)->endpoint = pair.endpointA;
  pair.peersB.get(pair.peerB)->endpointState =
      Supla::SupLan::kPeerEndpointLocateCandidate;
  uint8_t control[17] = {};
  control[4] = 0xFF;
  ASSERT_TRUE(pair.runtimeB.sendControl(pair.peerB, pair.resource, control,
                                       sizeof(control)));
  pair.pumpMs(4100);
  pair.network.packets.clear();
  pair.datagramsA.available = true;
  pair.runtimeA.testHooks()->dropNextAckTx = 10;
  pair.pumpMs(200);
  ASSERT_EQ(pair.appA.controlCalls, 1U);
  ASSERT_EQ(pair.runtimeB.poolDiagnostics().retries.used, 1U);
  pair.datagramsA.available = false;
  pair.pumpMs(700);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().retries.used, 0U);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().deferredEvents.used, 0U);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().pending.used, 0U);
}

TEST(SupLanRuntime, ExpiredControlDoesNotCancelConcurrentReadRecovery) {
  RuntimePair pair;
  pair.datagramsA.available = false;
  uint8_t control[17] = {};
  control[4] = 0xFF;
  ASSERT_TRUE(pair.runtimeB.sendControl(pair.peerB, pair.resource, control,
                                       sizeof(control)));
  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
  pair.pumpMs(5000);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().deferredEvents.used, 0U);
  EXPECT_TRUE(pair.runtimeB.readNeedsRefresh(pair.peerB, pair.resource));
  pair.network.packets.clear();
  pair.datagramsA.available = true;
  pair.pumpMs(2000);
  EXPECT_EQ(pair.appA.controlCalls, 0U);
  EXPECT_EQ(pair.runtimeB.diagnostics().stateNotificationRx, 1U);
}

TEST(SupLanRecovery, BackgroundStagesHaveHardMinimumAndAdditiveJitter) {
  RuntimePair pair;
  pair.network.failMulticast = true;
  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
  pair.runtimeB.iterate();
  for (uint32_t now = 250; now <= 750; now += 250) {
    pair.network.now = now;
    pair.runtimeB.iterate();
  }
  const uint32_t delays[] = {5000, 15000, 60000, 300000, 300000, 300000};
  for (const uint32_t base : delays) {
    const uint32_t failedAt = pair.network.now;
    const auto status = pair.runtimeB.recoveryStatus(pair.peerB);
    const uint32_t delay = status.nextRefreshMs - failedAt;
    EXPECT_GE(delay, base);
    EXPECT_LE(delay, base + base / 5);
    EXPECT_FALSE(status.active);
    EXPECT_EQ(pair.runtimeB.poolDiagnostics().deferredEvents.used, 0U);
    const uint32_t attempts = pair.network.multicastAttempts;
    pair.network.now = status.nextRefreshMs - 1;
    pair.runtimeB.iterate();
    EXPECT_EQ(pair.network.multicastAttempts, attempts);
    pair.network.now = status.nextRefreshMs;
    pair.runtimeB.iterate();
    EXPECT_EQ(pair.network.multicastAttempts, attempts + 1);
    for (uint32_t elapsed = 250; elapsed <= 750; elapsed += 250) {
      pair.network.now = status.nextRefreshMs + elapsed;
      pair.runtimeB.iterate();
    }
    EXPECT_EQ(pair.network.multicastAttempts, attempts + 3);
  }
}

TEST(SupLanRecovery, BackgroundSuccessClearsDependencyAndResetsBackoff) {
  RuntimePair pair;
  pair.datagramsA.available = false;
  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
  pair.pumpMs(800);
  ASSERT_TRUE(pair.runtimeB.readNeedsRefresh(pair.peerB, pair.resource));
  pair.network.packets.clear();
  pair.datagramsA.available = true;
  pair.network.now = pair.runtimeB.recoveryStatus(pair.peerB).nextRefreshMs;
  pair.pump(20);
  EXPECT_EQ(pair.appB.stateCalls, 1U);
  EXPECT_FALSE(pair.runtimeB.readNeedsRefresh(pair.peerB, pair.resource));
  EXPECT_EQ(pair.runtimeB.recoveryStatus(pair.peerB).backgroundStage, 0U);
  EXPECT_FALSE(pair.runtimeB.recoveryStatus(pair.peerB).active);
  const auto locates = pair.runtimeB.diagnostics().locateTx;
  pair.network.now += 600000;
  pair.pump(20);
  EXPECT_EQ(pair.runtimeB.diagnostics().locateTx, locates);
}

TEST(SupLanRecovery, ResourcesAndExplicitTriggersShareOneCycle) {
  RuntimePair pair;
  const ResourceId second = {Supla::SupLan::kResourceTypeChannel, 50003};
  Supla::SupLan::AclEntry acl[2] = {
      {pair.resource, Supla::SupLan::kPermissionRead},
      {second, Supla::SupLan::kPermissionRead}};
  ASSERT_TRUE(pair.peersA.replaceAcl(pair.peerA, 2, acl, 2));
  ASSERT_TRUE(pair.peersB.replaceAcl(pair.peerB, 2, acl, 2));
  pair.datagramsA.available = false;
  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
  const uint32_t deadline = pair.runtimeB.recoveryStatus(pair.peerB).deadlineMs;
  for (unsigned i = 0; i < 100; ++i) {
    ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
    ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, second));
  }
  // Accepted logical demand does not reserve deferred frames before iterate.
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().deferredEvents.used, 0U);
  EXPECT_TRUE(pair.runtimeB.readNeedsRefresh(pair.peerB, pair.resource));
  EXPECT_TRUE(pair.runtimeB.readNeedsRefresh(pair.peerB, second));
  EXPECT_EQ(pair.runtimeB.recoveryStatus(pair.peerB).deadlineMs, deadline);
  pair.pumpMs(800);
  EXPECT_EQ(pair.runtimeB.diagnostics().locateTx, 3U);
  EXPECT_TRUE(pair.runtimeB.readNeedsRefresh(pair.peerB, pair.resource));
  EXPECT_TRUE(pair.runtimeB.readNeedsRefresh(pair.peerB, second));
  pair.network.packets.clear();
  pair.datagramsA.available = true;
  // Explicit READ bypasses the background wait and coalesces both resources.
  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, second));
  pair.pump(20);
  EXPECT_EQ(pair.runtimeB.diagnostics().locateTx, 4U);
  EXPECT_EQ(pair.runtimeA.diagnostics().readDispatched, 2U);
  EXPECT_FALSE(pair.runtimeB.readNeedsRefresh(pair.peerB, pair.resource));
  EXPECT_FALSE(pair.runtimeB.readNeedsRefresh(pair.peerB, second));
}

TEST(SupLanRecovery, SleepSuspendsBackgroundButExplicitReadCanWakeRecovery) {
  RuntimePair pair;
  pair.datagramsA.available = false;
  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
  pair.pumpMs(800);
  pair.runtimeB.setPeerSleeping(pair.peerB, true);
  pair.network.now += 600000;
  pair.runtimeB.iterate();
  EXPECT_EQ(pair.runtimeB.diagnostics().locateTx, 3U);
  EXPECT_FALSE(pair.runtimeB.recoveryStatus(pair.peerB).active);
  pair.network.packets.clear();
  pair.datagramsA.available = true;
  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
  pair.pump(20);
  EXPECT_EQ(pair.appB.stateCalls, 1U);
}

TEST(SupLanRecovery, HardDeadlineStopsHandshakeEvenAcrossClockWrap) {
  RuntimePair pair;
  pair.network.now = UINT32_MAX - 4999;
  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
  pair.runtimeA.iterate();
  pair.runtimeB.iterate();
  pair.runtimeB.testHooks()->pauseSessionInitRetries = true;
  pair.network.packets.clear();
  pair.network.now += 4999;
  pair.runtimeB.iterate();
  pair.network.packets.clear();
  pair.network.now += 1;
  pair.runtimeB.iterate();
  EXPECT_FALSE(pair.runtimeB.recoveryStatus(pair.peerB).active);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().pending.used, 0U);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().locates.used, 0U);
  EXPECT_TRUE(pair.runtimeB.readNeedsRefresh(pair.peerB, pair.resource));
}

TEST(SupLanRecovery, MissedStateIsCoalescedBoundedAndNeverReplayed) {
  RuntimePair pair;
  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
  pair.pump(20);
  const uint32_t before = pair.appB.stateCalls;
  pair.runtimeA.clearEndpoint(pair.peerA);
  pair.datagramsB.available = false;
  pair.network.packets.clear();
  uint8_t state[14] = {};
  state[0] = 0xFF;
  for (uint8_t value = 1; value <= 20; ++value) {
    state[6] = value;
    ASSERT_TRUE(pair.runtimeA.publishState(pair.peerA, pair.resource,
        Supla::SupLan::kSuplaCallDeviceChannelValueChangedC,
        state, sizeof(state)));
  }
  EXPECT_EQ(pair.runtimeA.poolDiagnostics().deferredEvents.used, 1U);
  // A STATE never consumes the entire queue, so reverse READ is accepted.
  ASSERT_TRUE(pair.runtimeA.requestRead(pair.actionPeerA,
      ResourceId{Supla::SupLan::kResourceTypeChannel, 50002}));
  pair.pumpMs(6000);
  EXPECT_EQ(pair.runtimeA.poolDiagnostics().deferredEvents.used, 0U);
  pair.network.now += 301000;
  pair.runtimeA.iterate();
  EXPECT_EQ(pair.runtimeA.poolDiagnostics().interests.used, 0U);
  pair.network.packets.clear();
  pair.datagramsB.available = true;
  pair.pump(20);
  EXPECT_EQ(pair.appB.stateCalls, before);
}

TEST(SupLanRecovery, PendingStateIsDroppedWhenInterestExpires) {
  RuntimePair pair;
  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
  pair.pump(20);
  pair.runtimeA.clearEndpoint(pair.peerA);
  pair.network.packets.clear();
  pair.network.now = 300000;
  uint8_t state[14] = {};
  state[0] = 0xFF;
  ASSERT_TRUE(pair.runtimeA.publishState(pair.peerA, pair.resource,
      Supla::SupLan::kSuplaCallDeviceChannelValueChangedC,
      state, sizeof(state)));
  pair.network.now += 100;
  pair.network.packets.clear();
  pair.runtimeA.iterate();
  EXPECT_EQ(pair.runtimeA.poolDiagnostics().interests.used, 0U);
  EXPECT_EQ(pair.runtimeA.poolDiagnostics().deferredEvents.used, 0U);
}

TEST(SupLanRuntime, DataFailureSkipsAckAndOccursBeforeFragmentation) {
  RuntimePair pair;
  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
  ASSERT_TRUE(pair.runtimeA.requestRead(pair.actionPeerA,
      ResourceId{Supla::SupLan::kResourceTypeChannel, 50002}));
  pair.pump(20);
  pair.runtimeA.testHooks()->failNextDataTx = 1;
  uint8_t action[15] = {};
  action[0] = 0xFF;
  ASSERT_TRUE(pair.runtimeB.publishAction(pair.actionPeerB,
      ResourceId{Supla::SupLan::kResourceTypeChannel, 50002},
      action, sizeof(action)));
  pair.pump(10);
  EXPECT_EQ(pair.runtimeA.testHooks()->failNextDataTx, 1U);
  EXPECT_EQ(pair.appA.actionCalls, 1U);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().retries.used, 0U);
  pair.runtimeA.testHooks()->maxDatagramPayload = 96;
  const auto fragments = pair.runtimeA.diagnostics().fragmentTx;
  uint8_t extended[206] = {};
  extended[0] = 0xFF;
  extended[2] = 200;
  EXPECT_FALSE(pair.runtimeA.publishState(pair.peerA, pair.resource,
      Supla::SupLan::kSuplaCallDeviceChannelExtendedValueChanged,
      extended, sizeof(extended)));
  EXPECT_EQ(pair.runtimeA.testHooks()->failNextDataTx, 0U);
  EXPECT_EQ(pair.runtimeA.diagnostics().fragmentTx, fragments);
  ASSERT_TRUE(pair.runtimeA.publishState(pair.peerA, pair.resource,
      Supla::SupLan::kSuplaCallDeviceChannelExtendedValueChanged,
      extended, sizeof(extended)));
  pair.pump(10);
  EXPECT_EQ(pair.appB.lastStateLength, sizeof(extended));
  EXPECT_GT(pair.runtimeA.diagnostics().fragmentTx, fragments);
}

TEST(SupLanRuntime, FailedControlSendRetriesIdenticalFrameAt150Ms) {
  RuntimePair pair;
  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
  pair.pump(20);
  struct Observation {
    RuntimePair *pair;
    std::vector<std::vector<uint8_t>> frames;
    std::vector<uint32_t> times;
  } observation = {&pair, {}, {}};
  pair.runtimeB.setDataTransmitObserver(
      [](void *context, const Endpoint &, const uint8_t *frame, size_t length) {
        auto *seen = static_cast<Observation *>(context);
        seen->frames.emplace_back(frame, frame + length);
        seen->times.push_back(seen->pair->network.now);
      }, &observation);
  pair.runtimeB.testHooks()->failNextDataTx = 1;
  uint8_t control[17] = {};
  control[4] = 0xFF;
  control[9] = 1;
  ASSERT_TRUE(pair.runtimeB.sendControl(pair.peerB, pair.resource,
                                       control, sizeof(control)));
  ASSERT_EQ(observation.frames.size(), 1U);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().retries.used, 1U);
  EXPECT_EQ(pair.appA.controlCalls, 0U);
  pair.network.now += 149;
  pair.runtimeB.iterate();
  EXPECT_EQ(observation.frames.size(), 1U);
  pair.network.now += 1;
  pair.runtimeB.iterate();
  ASSERT_EQ(observation.frames.size(), 2U);
  EXPECT_EQ(observation.frames[0], observation.frames[1]);
  EXPECT_EQ(observation.times[1] - observation.times[0], 150U);
  pair.pump(10);
  EXPECT_EQ(pair.appA.controlCalls, 1U);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().retries.used, 0U);
}

TEST(SupLanRecovery, SourcePublicationsWithSessionDoNotCreateRecoveryCycles) {
  using E = Supla::SupLan::DiagnosticEvent;
  for (bool full : {false, true}) {
    SCOPED_TRACE(full);
    RuntimePair pair;
    pair.appA.fullSnapshots = full;
    // Match the SDK Source callback: grants require a session, not a READ.
    pair.appA.bindingPoolEnabled = true;
    ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
    pair.pump(20);
    ASSERT_TRUE(pair.runtimeA.hasSession(pair.peerA));
    EXPECT_FALSE(pair.runtimeA.recoveryStatus(pair.peerA).active);
    const auto before = pair.appB.stateCalls;
    for (int i = 0; i < 12; ++i) {
      pair.appA.value = i + 1;
      uint8_t payload[36] = {};
      size_t length = 0;
      bool eventOnly = false;
      ASSERT_TRUE(pair.appA.readResource(pair.resource, &eventOnly,
          payload, sizeof(payload), &length));
      ASSERT_TRUE(pair.runtimeA.publishStateParts(pair.peerA, pair.resource,
          full ? 6 : Supla::SupLan::kSuplaCallDeviceChannelValueChangedC,
          nullptr, 0, payload, length, full));
      pair.pumpMs(1000);
      EXPECT_FALSE(pair.runtimeA.recoveryStatus(pair.peerA).active);
    }
    EXPECT_EQ(pair.appB.stateCalls, before + 12);
    EXPECT_EQ(pair.appA.diagnosticEvents[
                  static_cast<unsigned>(E::RecoveryFailed)], 0U);
    EXPECT_EQ(pair.appA.diagnosticEvents[static_cast<unsigned>(E::ReadRefresh)],
              0U);
    EXPECT_EQ(pair.appA.diagnosticEvents[
                  static_cast<unsigned>(E::ReadTransmit)], 0U);
    EXPECT_EQ(pair.runtimeA.poolDiagnostics().deferredEvents.used, 0U);
  }
}

TEST(SupLanRecovery, SourcePublicationsRespectBackoffAndRecoverFreshState) {
  using E = Supla::SupLan::DiagnosticEvent;
  RuntimePair pair;
  pair.appA.bindingPoolEnabled = true;
  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
  pair.pump(20);
  const auto before = pair.appB.stateCalls;
  pair.runtimeA.clearEndpoint(pair.peerA);
  pair.datagramsB.available = false;
  pair.network.packets.clear();
  uint8_t state[14] = {};
  state[0] = 0xff;
  ASSERT_TRUE(pair.runtimeA.publishState(pair.peerA, pair.resource,
      Supla::SupLan::kSuplaCallDeviceChannelValueChangedC,
      state, sizeof(state)));
  pair.pumpMs(1000);
  ASSERT_FALSE(pair.runtimeA.recoveryStatus(pair.peerA).active);
  const auto deadline = pair.runtimeA.recoveryStatus(pair.peerA).nextRefreshMs;
  ASSERT_GT(deadline, pair.network.now);
  const auto attempts = pair.network.multicastAttempts;
  while (pair.network.now + 10 < deadline) {
    pair.network.now += 10;
    EXPECT_FALSE(pair.runtimeA.publishState(pair.peerA, pair.resource,
        Supla::SupLan::kSuplaCallDeviceChannelValueChangedC,
        state, sizeof(state)));
    pair.runtimeA.iterate();
    EXPECT_FALSE(pair.runtimeA.recoveryStatus(pair.peerA).active);
    EXPECT_EQ(pair.runtimeA.recoveryStatus(pair.peerA).nextRefreshMs, deadline);
  }
  EXPECT_EQ(pair.network.multicastAttempts, attempts);
  EXPECT_EQ(pair.appA.diagnosticEvents[static_cast<unsigned>(E::ReadRefresh)],
            0U);
  EXPECT_EQ(pair.runtimeA.poolDiagnostics().deferredEvents.used, 0U);
  pair.network.packets.clear();
  pair.datagramsB.available = true;
  pair.network.now = deadline;
  pair.pump(20);
  ASSERT_TRUE(pair.runtimeA.hasSession(pair.peerA));
  EXPECT_EQ(pair.appB.stateCalls, before);  // No old publication is replayed.
  state[6] = 42;
  ASSERT_TRUE(pair.runtimeA.publishState(pair.peerA, pair.resource,
      Supla::SupLan::kSuplaCallDeviceChannelValueChangedC,
      state, sizeof(state)));
  pair.pump(20);
  EXPECT_EQ(pair.appB.stateCalls, before + 1);
  EXPECT_FALSE(pair.runtimeA.recoveryStatus(pair.peerA).active);
}

TEST(SupLanRecovery, SourceSnapshotBuildFailureDoesNotLeaveRecoveryActive) {
  using E = Supla::SupLan::DiagnosticEvent;
  RuntimePair pair;
  pair.appA.fullSnapshots = true;
  pair.appA.bindingPoolEnabled = true;
  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
  pair.pump(20);
  const auto before = pair.appB.stateCalls;
  uint8_t payload[36] = {};
  pair.appA.stateAvailable = false;
  ASSERT_TRUE(pair.runtimeA.publishStateParts(pair.peerA, pair.resource, 6,
      nullptr, 0, payload, sizeof(payload), true));
  pair.pumpMs(6000);
  EXPECT_EQ(pair.appA.diagnosticEvents[
                static_cast<unsigned>(E::RecoveryFailed)], 0U);
  EXPECT_EQ(pair.appB.stateCalls, before);
  EXPECT_EQ(pair.runtimeA.poolDiagnostics().deferredEvents.used, 0U);
  EXPECT_FALSE(pair.runtimeA.recoveryStatus(pair.peerA).active);
}

TEST(SupLanRecovery, LocalStateSendFailureDoesNotBecomeRunnableRetry) {
  RuntimePair pair;
  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
  pair.pump(20);
  const uint32_t before = pair.appB.stateCalls;
  pair.runtimeA.testHooks()->failNextDataTx = 1;
  uint8_t state[14] = {};
  state[0] = 0xFF;
  ASSERT_TRUE(pair.runtimeA.publishState(pair.peerA, pair.resource,
      Supla::SupLan::kSuplaCallDeviceChannelValueChangedC,
      state, sizeof(state)));
  EXPECT_EQ(pair.runtimeA.poolDiagnostics().deferredEvents.used, 0U);
  pair.pumpMs(1000);
  EXPECT_EQ(pair.appB.stateCalls, before);
  EXPECT_EQ(pair.runtimeA.poolDiagnostics().retries.used, 0U);
}

TEST(SupLanDiscoveryAudit, LateLocateReplyPreservesConfirmedSessionEndpoint) {
  RuntimePair pair;
  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
  pair.runtimeB.iterate();
  pair.pump(20);
  pair.runtimeB.clearEndpoint(pair.peerB);
  ASSERT_TRUE(pair.runtimeA.forgetSession(pair.peerA));
  pair.network.packets.clear();
  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
  uint8_t state[14] = {};
  state[0] = 0xFF;
  ASSERT_TRUE(pair.runtimeA.publishState(pair.peerA, pair.resource, 103,
                                        state, sizeof(state)));
  // Source initiates directly while Destination's LOCATE is outstanding.
  pair.runtimeB.iterate();
  pair.runtimeA.iterate();
  std::vector<Packet> replies;
  for (size_t i = 0; i < pair.network.packets.size();) {
    const Packet &packet = pair.network.packets[i];
    if (packet.bytes.size() >= 2 &&
        packet.bytes[1] == Supla::SupLan::kFrameLocateReply) {
      replies.push_back(packet);
      pair.network.packets.erase(pair.network.packets.begin() + i);
    } else {
      ++i;
    }
  }
  ASSERT_EQ(replies.size(), 1U);
  pair.runtimeB.iterate();  // Protected STATE confirms the inbound SESSION.
  ASSERT_EQ(pair.runtimeB.poolDiagnostics().sessions.used, 1U);
  ASSERT_EQ(pair.peersB.get(pair.peerB)->endpointState,
            Supla::SupLan::kPeerEndpointAuthenticated);
  const Endpoint confirmed = pair.peersB.get(pair.peerB)->endpoint;
  const auto handshakes = pair.runtimeB.diagnostics().sessionInitTx;
  // Even a valid reply received from another endpoint is only discovery.
  replies[0].source.port += 1;
  pair.network.packets.push_back(replies[0]);
  pair.runtimeB.iterate();
  EXPECT_EQ(pair.peersB.get(pair.peerB)->endpointState,
            Supla::SupLan::kPeerEndpointAuthenticated);
  EXPECT_EQ(pair.peersB.get(pair.peerB)->endpoint.address, confirmed.address);
  EXPECT_EQ(pair.peersB.get(pair.peerB)->endpoint.port, confirmed.port);
  pair.pump(20);
  EXPECT_FALSE(pair.runtimeB.readNeedsRefresh(pair.peerB, pair.resource));
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().retries.used, 0U);
  EXPECT_EQ(pair.runtimeB.diagnostics().sessionInitTx, handshakes);
  uint8_t control[17] = {};
  control[4] = 0xFF;
  control[9] = 1;
  ASSERT_TRUE(pair.runtimeB.sendControl(pair.peerB, pair.resource, control,
                                       sizeof(control)));
  pair.pump(20);
  EXPECT_EQ(pair.appA.controlCalls, 1U);
  EXPECT_EQ(pair.appA.value, 1U);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().retries.used, 0U);
}


TEST(SupLanRecovery, LogicalAdmissionIsIndependentOfDeferredFrameCapacity) {
  RuntimePair pair;
  pair.datagramsA.available = false;
  uint8_t control[17] = {};
  control[4] = 0xff;
  for (unsigned i = 0; i < SUPLAN_MAX_DEFERRED_APP_EVENTS; ++i) {
    ASSERT_TRUE(pair.runtimeB.sendControl(pair.peerB, pair.resource, control,
                                         sizeof(control), 701));
  }
  ASSERT_EQ(pair.runtimeB.poolDiagnostics().deferredEvents.used,
            SUPLAN_MAX_DEFERRED_APP_EVENTS);
  const auto packets = pair.network.history.size();
  for (int i = 0; i < 5; ++i) {
    ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
  }
  EXPECT_EQ(pair.network.history.size(), packets);
  EXPECT_TRUE(pair.runtimeB.readNeedsRefresh(pair.peerB, pair.resource));
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().deferredEvents.used,
            SUPLAN_MAX_DEFERRED_APP_EVENTS);
  pair.runtimeB.cancelOwnedControls(701);
  pair.datagramsA.available = true;
  pair.pump(40);
  EXPECT_EQ(pair.runtimeA.diagnostics().readDispatched, 1U);
  EXPECT_EQ(pair.appB.stateCalls, 1U);
  EXPECT_EQ(pair.appA.controlCalls, 0U);
  EXPECT_FALSE(pair.runtimeB.readNeedsRefresh(pair.peerB, pair.resource));
}

TEST(SupLanRecovery, LogicalDemandPoolRejectsOverflowAndReclaimsLastOwner) {
  RuntimePair pair;
  const Supla::SupLan::AclEntry wildcard = {
      {Supla::SupLan::kResourceTypeDevice, 1001},
      Supla::SupLan::kPermissionRead};
  ASSERT_TRUE(pair.peersB.replaceAcl(pair.peerB, 2, &wildcard, 1));
  for (uint32_t i = 0; i < SUPLAN_MAX_READ_DEPENDENCIES; ++i) {
    const ResourceId resource = {
        Supla::SupLan::kResourceTypeChannel, 61000 + i};
    ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, resource)) << i;
    ASSERT_TRUE(pair.runtimeB.readNeedsRefresh(pair.peerB, resource));
  }
  const ResourceId extra = {Supla::SupLan::kResourceTypeChannel, 71000};
  EXPECT_FALSE(pair.runtimeB.requestRead(pair.peerB, extra));
  EXPECT_FALSE(pair.runtimeB.readNeedsRefresh(pair.peerB, extra));
  EXPECT_EQ(pair.runtimeB.diagnostics().poolReject, 1U);
  EXPECT_EQ(pair.runtimeB.diagnostics().locateTx, 0U);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().deferredEvents.used, 0U);
  pair.runtimeB.removeReadDependency(
      pair.peerB, {Supla::SupLan::kResourceTypeChannel, 61000});
  EXPECT_TRUE(pair.runtimeB.requestRead(pair.peerB, extra));
  EXPECT_FALSE(pair.runtimeB.requestRead(pair.peerB, wildcard.resource));
  EXPECT_FALSE(pair.runtimeB.requestRead(pair.peerB, {0xff, 1001}));
}

TEST(SupLanRecovery, DirectServerRoleMutationCannotDispatchRetainedRead) {
  for (bool connected : {false, true}) {
    RuntimePair pair;
    if (connected) {
      ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
      pair.pump(20);
    }
    pair.datagramsA.available = false;
    ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
    pair.runtimeB.iterate();
    const auto dataTx = pair.runtimeB.diagnostics().dataTx;
    pair.peersB.get(pair.peerB)->destination = false;
    pair.network.now += Supla::SupLan::kAckRetryMs + 1;
    pair.runtimeB.iterate();
    EXPECT_EQ(pair.runtimeB.diagnostics().dataTx, dataTx);
    EXPECT_FALSE(pair.runtimeB.readNeedsRefresh(pair.peerB, pair.resource));
    EXPECT_EQ(pair.runtimeB.poolDiagnostics().deferredEvents.used, 0U);
    EXPECT_EQ(pair.runtimeB.poolDiagnostics().retries.used, 0U);
    EXPECT_FALSE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
    pair.peersB.get(pair.peerB)->destination = true;
    pair.datagramsA.available = true;
    ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
    pair.pump(40);
    EXPECT_FALSE(pair.runtimeB.readNeedsRefresh(pair.peerB, pair.resource));
  }
}


TEST(SupLanRecovery, FairCursorDispatchesAllSlotsAndReusedSlotsOnce) {
  RuntimePair pair;
  const Supla::SupLan::AclEntry wildcard = {
      {Supla::SupLan::kResourceTypeDevice, 1001},
      Supla::SupLan::kPermissionRead};
  ASSERT_TRUE(pair.peersA.replaceAcl(pair.peerA, 2, &wildcard, 1));
  ASSERT_TRUE(pair.peersB.replaceAcl(pair.peerB, 2, &wildcard, 1));
  // Each normal Source has half as many interest slots as Destination
  // dependencies. Two Sources cover the full pool without overriding limits.
  const Endpoint endpointC = {0x0300007f, 2016};
  FakeDatagramPort portC(&pair.network, endpointC);
  FakeApplication appC;
  Supla::SupLan::PeerTable peersC;
  Supla::SupLan::Runtime runtimeC(&pair.crypto, &portC, &appC, &peersC,
                                 RuntimePair::nodeAddress(1003), 27);
  const Supla::SupLan::PeerContext context = {
      Supla::SupLan::kAuthorityServer, 0,
      RuntimePair::nodeAddress(1003), RuntimePair::nodeAddress(1002), 1, 1};
  const uint8_t root[32] = {42};
  Supla::SupLan::PeerMaterial material = {};
  ASSERT_TRUE(Supla::SupLan::derivePeerMaterial(&context, root, &material));
  const Supla::SupLan::AclEntry otherWildcard = {
      {Supla::SupLan::kResourceTypeDevice, 1003},
      Supla::SupLan::kPermissionRead};
  uint8_t sourceC = 0xff, destinationC = 0xff;
  ASSERT_TRUE(peersC.addPeerFromRoot(&context, root, 1, &otherWildcard, 1,
                                    &sourceC));
  ASSERT_TRUE(pair.peersB.addPeer(&context, material.peerKey, 1,
                                 &otherWildcard, 1, &destinationC, true));
  // The single-recipient fake multicast bus is not a broadcast simulation.
  // Known candidates exercise actual authenticated handshakes for both peers.
  pair.peersB.get(pair.peerB)->endpoint = pair.endpointA;
  pair.peersB.get(pair.peerB)->endpointState =
      Supla::SupLan::kPeerEndpointLocateCandidate;
  pair.peersB.get(destinationC)->endpoint = endpointC;
  pair.peersB.get(destinationC)->endpointState =
      Supla::SupLan::kPeerEndpointLocateCandidate;
  for (uint32_t i = 0; i < SUPLAN_MAX_READ_DEPENDENCIES; ++i) {
    ASSERT_TRUE(pair.runtimeB.requestRead(
        i % 2 ? destinationC : pair.peerB,
        {Supla::SupLan::kResourceTypeChannel, 61000 + i})) << i;
  }
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().deferredEvents.used, 0U);
  for (uint32_t i = 0; i < SUPLAN_MAX_READ_DEPENDENCIES + 40U; ++i) {
    ++pair.network.now;
    const auto sent = pair.runtimeB.diagnostics().dataTx;
    const auto retries = pair.runtimeB.diagnostics().retryTx;
    pair.runtimeB.iterate();
    EXPECT_LE(pair.runtimeB.diagnostics().dataTx - sent,
              2U + pair.runtimeB.diagnostics().retryTx - retries) << i;
    pair.runtimeA.iterate();
    runtimeC.iterate();
  }
  EXPECT_EQ(pair.appB.stateCalls, SUPLAN_MAX_READ_DEPENDENCIES);
  EXPECT_EQ(pair.runtimeA.diagnostics().readDispatched +
                runtimeC.diagnostics().readDispatched,
            SUPLAN_MAX_READ_DEPENDENCIES);
  for (uint32_t i = 0; i < SUPLAN_MAX_READ_DEPENDENCIES; ++i) {
    const ResourceId resource = {
        Supla::SupLan::kResourceTypeChannel, 61000 + i};
    ASSERT_FALSE(pair.runtimeB.readNeedsRefresh(
        i % 2 ? destinationC : pair.peerB, resource)) << i;
    if (i % 2 == 0) {
      pair.runtimeB.removeReadDependency(pair.peerB, resource);
      ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, resource));
    }
  }
  for (uint32_t i = 0; i < SUPLAN_MAX_READ_DEPENDENCIES + 40U; ++i) {
    pair.network.now += 10;
    pair.runtimeB.iterate();
    pair.runtimeA.iterate();
    runtimeC.iterate();
  }
  EXPECT_EQ(pair.appB.stateCalls, SUPLAN_MAX_READ_DEPENDENCIES +
                                (SUPLAN_MAX_READ_DEPENDENCIES + 1) / 2);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().retries.used, 0U);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().deferredEvents.used, 0U);
  // A known-session Source that stops replying may own one retry, but must
  // not fill the shared pools ahead of healthy C's waiting demand.
  const auto readsC = runtimeC.diagnostics().readDispatched;
  pair.datagramsA.available = false;
  const auto unavailableAt = pair.network.now;
  for (uint32_t i = 0; i < SUPLAN_MAX_READ_DEPENDENCIES; ++i) {
    ASSERT_TRUE(pair.runtimeB.requestRead(
        i % 2 ? destinationC : pair.peerB,
        {Supla::SupLan::kResourceTypeChannel, 61000 + i}));
  }
  for (uint32_t i = 0; i < SUPLAN_MAX_READ_DEPENDENCIES + 40U; ++i) {
    ++pair.network.now;
    pair.runtimeB.iterate();
    pair.runtimeA.iterate();
    runtimeC.iterate();
    bool waitingC = false;
    for (uint32_t id = 1; id < SUPLAN_MAX_READ_DEPENDENCIES; id += 2) {
      waitingC = waitingC || pair.runtimeB.readNeedsRefresh(
          destinationC, {Supla::SupLan::kResourceTypeChannel, 61000 + id});
    }
    if (!waitingC) break;
  }
  EXPECT_EQ(runtimeC.diagnostics().readDispatched,
            readsC + SUPLAN_MAX_READ_DEPENDENCIES / 2);
  // C completed before A's retry timer: A may now use both retry slots,
  // since it is the only remaining peer with demand, but C was not blocked.
  EXPECT_LE(pair.network.now - unavailableAt,
            SUPLAN_MAX_READ_DEPENDENCIES + 40U);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().retries.used, 2U);
  // Two admissions per turn can leave two additional A frames queued after
  // the final C response; C has already completed within the same bound.
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().deferredEvents.used, 2U);
  pair.datagramsA.available = true;
  pair.runtimeB.refreshPeer(pair.peerB, 40000, true);
  for (uint32_t i = 0; i < SUPLAN_MAX_READ_DEPENDENCIES + 80U; ++i) {
    ++pair.network.now;
    pair.runtimeB.iterate();
    pair.runtimeA.iterate();
    runtimeC.iterate();
  }
  for (uint32_t i = 0; i < SUPLAN_MAX_READ_DEPENDENCIES; ++i) {
    EXPECT_FALSE(pair.runtimeB.readNeedsRefresh(
        i % 2 ? destinationC : pair.peerB,
        {Supla::SupLan::kResourceTypeChannel, 61000 + i}));
  }
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().retries.used, 0U);
}


TEST(SupLanRecovery, MissingStateCannotBlockHealthyChannelOnSameSource) {
  RuntimePair pair;
  const ResourceId second = {Supla::SupLan::kResourceTypeChannel, 50003};
  const Supla::SupLan::AclEntry acl[] = {
      {pair.resource, Supla::SupLan::kPermissionRead},
      {second, Supla::SupLan::kPermissionRead}};
  ASSERT_TRUE(pair.peersA.replaceAcl(pair.peerA, 2, acl, 2));
  ASSERT_TRUE(pair.peersB.replaceAcl(pair.peerB, 2, acl, 2));
  pair.appA.unavailableResource = pair.resource.id;
  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, second));
  pair.pump(20);
  EXPECT_TRUE(pair.runtimeB.readNeedsRefresh(pair.peerB, pair.resource));
  EXPECT_FALSE(pair.runtimeB.readNeedsRefresh(pair.peerB, second));
  EXPECT_EQ(pair.appB.stateCalls, 1U);
  pair.appA.unavailableResource = 0;
  pair.pumpMs(1000);
  EXPECT_FALSE(pair.runtimeB.readNeedsRefresh(pair.peerB, pair.resource));
  EXPECT_EQ(pair.appB.stateCalls, 2U);
}

TEST(SupLanRecovery, NestedIterateAndRequestsCannotBypassFreshReadBudget) {
  RuntimePair pair;
  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
  pair.pump(20);
  const ResourceId second = {Supla::SupLan::kResourceTypeChannel, 50003};
  const ResourceId third = {Supla::SupLan::kResourceTypeChannel, 50004};
  const Supla::SupLan::AclEntry acl[] = {
      {pair.resource, Supla::SupLan::kPermissionRead},
      {second, Supla::SupLan::kPermissionRead},
      {third, Supla::SupLan::kPermissionRead}};
  ASSERT_TRUE(pair.peersA.replaceAcl(pair.peerA, 2, acl, 3));
  ASSERT_TRUE(pair.peersB.replaceAcl(pair.peerB, 2, acl, 3));
  struct Context {
    RuntimePair *pair; ResourceId second; ResourceId third; bool called;
  };
  Context context = {&pair, second, third, false};
  pair.runtimeB.setDataTransmitObserver(
      [](void *opaque, const Endpoint &, const uint8_t *, size_t) {
        auto &context = *static_cast<Context *>(opaque);
        if (context.called) return;
        context.called = true;
        EXPECT_TRUE(context.pair->runtimeB.requestRead(
            context.pair->peerB, context.second));
        EXPECT_TRUE(context.pair->runtimeB.requestRead(
            context.pair->peerB, context.third));
        context.pair->runtimeB.iterate();
      }, &context);
  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
  const auto sent = pair.runtimeB.diagnostics().dataTx;
  pair.runtimeB.iterate();
  EXPECT_TRUE(context.called);
  EXPECT_EQ(pair.runtimeB.diagnostics().dataTx, sent + 2);
  EXPECT_TRUE(pair.runtimeB.readNeedsRefresh(pair.peerB, third));
  pair.pump(30);
  EXPECT_EQ(pair.runtimeA.diagnostics().readDispatched, 4U);
  EXPECT_EQ(pair.appB.stateCalls, 4U);
  EXPECT_FALSE(pair.runtimeB.readNeedsRefresh(pair.peerB, second));
  EXPECT_FALSE(pair.runtimeB.readNeedsRefresh(pair.peerB, third));
}

TEST(SupLanRecovery, BeforeSendCallbackRevokesOrRecreatesReadFailClosed) {
  for (int variant = 0; variant < 3; ++variant) {
    RuntimePair pair;
    ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
    pair.pump(20);
    struct Context { RuntimePair *pair; int variant; bool called; };
    Context context = {&pair, variant, false};
    pair.runtimeB.setDataTransmitObserver(
        [](void *opaque, const Endpoint &, const uint8_t *, size_t) {
          auto &context = *static_cast<Context *>(opaque);
          if (context.called) return;
          context.called = true;
          auto &pair = *context.pair;
          if (context.variant == 0) {
            const Supla::SupLan::AclEntry other = {
                {Supla::SupLan::kResourceTypeChannel, 50003},
                Supla::SupLan::kPermissionRead};
            EXPECT_TRUE(pair.peersB.replaceAcl(pair.peerB, 2, &other, 1));
          } else {
            pair.runtimeB.removeReadDependency(pair.peerB, pair.resource);
            if (context.variant == 2) {
              EXPECT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
            }
          }
        }, &context);
    const auto sent = pair.runtimeB.diagnostics().dataTx;
    ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
    pair.runtimeB.iterate();
    EXPECT_TRUE(context.called);
    EXPECT_EQ(pair.runtimeB.diagnostics().dataTx,
              sent + (variant == 2 ? 1U : 0U));
    pair.pump(30);
    EXPECT_EQ(pair.runtimeA.diagnostics().readDispatched,
              variant == 2 ? 2U : 1U);
    EXPECT_EQ(pair.appB.stateCalls, variant == 2 ? 2U : 1U);
    EXPECT_FALSE(pair.runtimeB.readNeedsRefresh(pair.peerB, pair.resource));
  }
}

TEST(SupLanRecovery, QueuedReadRevocationKeepsUnrelatedControl) {
  RuntimePair pair;
  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
  pair.pump(20);
  const ResourceId second = {Supla::SupLan::kResourceTypeChannel, 50003};
  const Supla::SupLan::AclEntry acl[] = {
      {pair.resource, Supla::SupLan::kPermissionRead},
      {second, Supla::SupLan::kPermissionControl}};
  ASSERT_TRUE(pair.peersA.replaceAcl(pair.peerA, 2, acl, 2));
  ASSERT_TRUE(pair.peersB.replaceAcl(pair.peerB, 2, acl, 2));
  pair.datagramsA.available = false;
  uint8_t control[17] = {};
  control[4] = 0xff;
  for (unsigned i = 0; i < SUPLAN_MAX_RETRY_SLOTS; ++i) {
    ASSERT_TRUE(pair.runtimeB.sendControl(pair.peerB, second, control,
                                         sizeof(control)));
  }
  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
  pair.runtimeB.iterate();
  ASSERT_EQ(pair.runtimeB.poolDiagnostics().deferredEvents.used, 1U);
  ASSERT_TRUE(pair.peersB.replaceAcl(pair.peerB, 3, acl + 1, 1));
  // Synchronous drain occurs before an outer prune, with a queued READ.
  ASSERT_TRUE(pair.runtimeB.sendControl(pair.peerB, second, control,
                                       sizeof(control)));
  EXPECT_FALSE(pair.runtimeB.readNeedsRefresh(pair.peerB, pair.resource));
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().deferredEvents.used, 1U);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().retries.used,
            SUPLAN_MAX_RETRY_SLOTS);
  pair.datagramsA.available = true;
  pair.pump(40);
  EXPECT_EQ(pair.appA.controlCalls, SUPLAN_MAX_RETRY_SLOTS + 1U);
  EXPECT_EQ(pair.runtimeA.diagnostics().readDispatched, 1U);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().deferredEvents.used, 0U);
}

TEST(SupLanRecovery, CompletedReadDependenciesFollowCurrentAclCapacity) {
  RuntimePair pair;
  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
  pair.pump(20);
  for (uint32_t i = 0; i <= SUPLAN_MAX_TOTAL_ACL_ENTRIES; ++i) {
    const ResourceId resource = {
        Supla::SupLan::kResourceTypeChannel, 51000 + i};
    const Supla::SupLan::AclEntry acl[] = {
        {pair.resource, Supla::SupLan::kPermissionRead},
        {resource, Supla::SupLan::kPermissionRead}};
    ASSERT_TRUE(pair.peersA.replaceAcl(pair.peerA, 2 + i, acl, 2));
    ASSERT_TRUE(pair.peersB.replaceAcl(pair.peerB, 2 + i, acl, 2));
    // Allocation must reclaim old grants without requiring an iterate first.
    ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, resource)) << i;
    pair.pump(20);
    EXPECT_FALSE(pair.runtimeB.readNeedsRefresh(pair.peerB, resource));
    EXPECT_EQ(pair.appB.stateCalls, i + 2);
    // Expire Source interest to avoid exhausting its independent pool.
    pair.network.now += 300001;
    pair.runtimeA.iterate();
    pair.runtimeB.iterate();
  }
  EXPECT_EQ(pair.runtimeB.diagnostics().poolReject, 0U);
  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
  pair.pump(20);
  EXPECT_FALSE(pair.runtimeB.readNeedsRefresh(pair.peerB, pair.resource));
}

TEST(SupLanRecovery, AclRevocationCancelsQueuedAndRetryingReads) {
  for (bool established : {false, true}) {
    RuntimePair pair;
    if (established) {
      ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
      pair.pump(20);
    }
    pair.datagramsA.available = false;
    ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
    pair.runtimeB.iterate();
    ASSERT_TRUE(pair.runtimeB.readNeedsRefresh(pair.peerB, pair.resource));
    // Without a session, logical demand stays outside the deferred pool.
    EXPECT_EQ(pair.runtimeB.poolDiagnostics().retries.used,
              established ? 1U : 0U);
    EXPECT_EQ(pair.runtimeB.poolDiagnostics().deferredEvents.used, 0U);
    const Supla::SupLan::AclEntry controlOnly = {
        {Supla::SupLan::kResourceTypeChannel, 50003},
        Supla::SupLan::kPermissionControl};
    ASSERT_TRUE(pair.peersB.replaceAcl(pair.peerB, 2, &controlOnly, 1));
    pair.runtimeB.iterate();
    EXPECT_FALSE(pair.runtimeB.readNeedsRefresh(pair.peerB, pair.resource));
    EXPECT_EQ(pair.runtimeB.poolDiagnostics().deferredEvents.used, 0U);
    EXPECT_EQ(pair.runtimeB.poolDiagnostics().retries.used, 0U);
    EXPECT_EQ(pair.runtimeB.poolDiagnostics().locates.used, 0U);
    EXPECT_EQ(pair.runtimeB.poolDiagnostics().pending.used, 0U);
    EXPECT_FALSE(pair.runtimeB.recoveryStatus(pair.peerB).active);
    const auto locates = pair.runtimeB.diagnostics().locateTx;
    const auto retries = pair.runtimeB.diagnostics().retryTx;
    pair.pumpMs(15000);
    EXPECT_EQ(pair.runtimeB.diagnostics().locateTx, locates);
    EXPECT_EQ(pair.runtimeB.diagnostics().retryTx, retries);
  }
}

TEST(SupLanRecovery, AclRefreshPreservesStillAuthorizedPendingReads) {
  RuntimePair pair;
  pair.datagramsA.available = false;
  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
  const Supla::SupLan::AclEntry readOnly = {
      pair.resource, Supla::SupLan::kPermissionRead};
  ASSERT_TRUE(pair.peersB.replaceAcl(pair.peerB, 2, &readOnly, 1));
  pair.runtimeB.iterate();
  EXPECT_TRUE(pair.runtimeB.readNeedsRefresh(pair.peerB, pair.resource));
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().deferredEvents.used, 0U);
  pair.datagramsA.available = true;
  pair.pump(20);
  EXPECT_EQ(pair.appB.stateCalls, 1U);
  EXPECT_FALSE(pair.runtimeB.readNeedsRefresh(pair.peerB, pair.resource));
}

TEST(SupLanRecovery, RevokedReadDoesNotCancelOtherAuthorizedControl) {
  RuntimePair pair;
  const ResourceId second = {Supla::SupLan::kResourceTypeChannel, 50003};
  const Supla::SupLan::AclEntry acl[] = {
      {pair.resource, Supla::SupLan::kPermissionRead},
      {second, Supla::SupLan::kPermissionControl}};
  ASSERT_TRUE(pair.peersA.replaceAcl(pair.peerA, 2, acl, 2));
  ASSERT_TRUE(pair.peersB.replaceAcl(pair.peerB, 2, acl, 2));
  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
  uint8_t control[17] = {};
  control[4] = 0xFF;
  control[9] = 1;
  ASSERT_TRUE(pair.runtimeB.sendControl(pair.peerB, second, control,
                                       sizeof(control)));
  ASSERT_TRUE(pair.peersA.replaceAcl(pair.peerA, 3, acl + 1, 1));
  ASSERT_TRUE(pair.peersB.replaceAcl(pair.peerB, 3, acl + 1, 1));
  pair.pump(20);
  EXPECT_EQ(pair.runtimeA.diagnostics().readDispatched, 0U);
  EXPECT_EQ(pair.appA.controlCalls, 1U);
  EXPECT_EQ(pair.appA.value, 1U);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().retries.used, 0U);
  EXPECT_FALSE(pair.runtimeB.readNeedsRefresh(pair.peerB, pair.resource));
}

TEST(SupLanRuntime, QueuedActionRechecksSourceAclBeforeFirstTransmission) {
  for (bool keepRead : {false, true}) {
    RuntimePair pair;
    const ResourceId resource = {Supla::SupLan::kResourceTypeChannel, 50002};
    ASSERT_TRUE(pair.runtimeA.requestRead(pair.actionPeerA, resource));
    pair.pump(20);
    ASSERT_TRUE(pair.runtimeB.forgetSession(pair.actionPeerB));
    pair.datagramsA.available = false;
    uint8_t payload[15] = {};
    payload[0] = 0xFF;
    ASSERT_TRUE(pair.runtimeB.publishAction(pair.actionPeerB, resource, payload,
                                          sizeof(payload)));
    ASSERT_EQ(pair.runtimeB.poolDiagnostics().deferredEvents.used, 1U);
    const Supla::SupLan::AclEntry acl = {
        keepRead ? resource : ResourceId{
            Supla::SupLan::kResourceTypeChannel, 50003},
        keepRead ? Supla::SupLan::kPermissionRead
                 : Supla::SupLan::kPermissionActionTrigger};
    ASSERT_TRUE(pair.peersB.replaceAcl(pair.actionPeerB, 2, &acl, 1));
    pair.datagramsA.available = true;
    pair.pump(20);
    EXPECT_EQ(pair.appA.actionCalls, 0U);
    EXPECT_EQ(pair.runtimeB.diagnostics().actionTx, 0U);
    EXPECT_EQ(pair.runtimeB.poolDiagnostics().deferredEvents.used, 0U);
    EXPECT_EQ(pair.runtimeB.poolDiagnostics().retries.used, 0U);
    EXPECT_EQ(pair.runtimeB.poolDiagnostics().interests.used,
              keepRead ? 1U : 0U);
  }
}

TEST(SupLanRuntime, ActionRetryStopsAfterSourcePermissionRevocation) {
  RuntimePair pair;
  const ResourceId resource = {Supla::SupLan::kResourceTypeChannel, 50002};
  ASSERT_TRUE(pair.runtimeA.requestRead(pair.actionPeerA, resource));
  pair.pump(20);
  uint8_t payload[15] = {};
  payload[0] = 0xFF;
  pair.network.dropNextFullData = true;
  ASSERT_TRUE(pair.runtimeB.publishAction(pair.actionPeerB, resource, payload,
                                        sizeof(payload)));
  ASSERT_EQ(pair.runtimeB.poolDiagnostics().retries.used, 1U);
  const Supla::SupLan::AclEntry readOnly = {
      resource, Supla::SupLan::kPermissionRead};
  ASSERT_TRUE(pair.peersB.replaceAcl(pair.actionPeerB, 2, &readOnly, 1));
  const auto retries = pair.runtimeB.diagnostics().retryTx;
  pair.pumpMs(500);
  EXPECT_EQ(pair.appA.actionCalls, 0U);
  EXPECT_EQ(pair.runtimeB.diagnostics().retryTx, retries);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().retries.used, 0U);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().interests.used, 1U);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().sessions.used, 1U);
  ASSERT_TRUE(pair.runtimeA.requestRead(pair.actionPeerA, resource));
  pair.pump(20);
  EXPECT_EQ(pair.runtimeB.diagnostics().readDispatched, 2U);
}

TEST(SupLanRuntime, QueuedActionSurvivesUnrelatedAclReplacement) {
  RuntimePair pair;
  const ResourceId resource = {Supla::SupLan::kResourceTypeChannel, 50002};
  ASSERT_TRUE(pair.runtimeA.requestRead(pair.actionPeerA, resource));
  pair.pump(20);
  ASSERT_TRUE(pair.runtimeB.forgetSession(pair.actionPeerB));
  pair.datagramsA.available = false;
  uint8_t payload[15] = {};
  payload[0] = 0xFF;
  ASSERT_TRUE(pair.runtimeB.publishAction(pair.actionPeerB, resource, payload,
                                        sizeof(payload)));
  const Supla::SupLan::AclEntry acl[] = {
      {resource, Supla::SupLan::kPermissionActionTrigger},
      {{Supla::SupLan::kResourceTypeChannel, 50003},
       Supla::SupLan::kPermissionRead}};
  ASSERT_TRUE(pair.peersB.replaceAcl(pair.actionPeerB, 2, acl, 2));
  pair.datagramsA.available = true;
  pair.runtimeA.testHooks()->dropNextAckTx = 1;
  pair.pump(20);
  EXPECT_EQ(pair.appA.actionCalls, 1U);
  EXPECT_EQ(pair.runtimeB.diagnostics().retryTx, 1U);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().retries.used, 0U);
}

TEST(SupLanRuntime, RevokedInterestsDoNotExhaustPoolAcrossAclReplacements) {
  RuntimePair pair;
  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
  pair.pump(20);
  for (uint32_t i = 0; i <= SUPLAN_MAX_RUNTIME_INTERESTS; ++i) {
    const ResourceId resource = {
        Supla::SupLan::kResourceTypeChannel, 51000 + i};
    const Supla::SupLan::AclEntry acl[] = {
        {pair.resource, Supla::SupLan::kPermissionRead},
        {resource, Supla::SupLan::kPermissionRead}};
    ASSERT_TRUE(pair.peersA.replaceAcl(pair.peerA, 2 + i, acl, 2));
    ASSERT_TRUE(pair.peersB.replaceAcl(pair.peerB, 2 + i, acl, 2));
    ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, resource));
    pair.pump(20);
    EXPECT_EQ(pair.appB.stateCalls, i + 2);
    EXPECT_FALSE(pair.runtimeB.readNeedsRefresh(pair.peerB, resource));
    EXPECT_EQ(pair.runtimeA.poolDiagnostics().interests.used, 2U);
  }
  EXPECT_EQ(pair.runtimeA.diagnostics().poolReject, 0U);
  // The still-authorized original interest keeps receiving notifications.
  uint8_t payload[14] = {};
  payload[0] = 0xFF;
  const auto states = pair.appB.stateCalls;
  ASSERT_TRUE(pair.runtimeA.publishState(pair.peerA, pair.resource,
      Supla::SupLan::kSuplaCallDeviceChannelValueChangedC,
      payload, sizeof(payload)));
  pair.pump(20);
  EXPECT_EQ(pair.appB.stateCalls, states + 1);
}

TEST(SupLanRuntime, InterestRetainedWhileAnyReadControlOrActionGrantRemains) {
  RuntimePair pair;
  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
  pair.pump(20);
  uint32_t revision = 2;
  for (uint8_t permission : {Supla::SupLan::kPermissionRead,
                             Supla::SupLan::kPermissionControl,
                             Supla::SupLan::kPermissionActionTrigger}) {
    const Supla::SupLan::AclEntry acl = {pair.resource, permission};
    ASSERT_TRUE(pair.peersA.replaceAcl(pair.peerA, revision++, &acl, 1));
    pair.runtimeA.iterate();
    EXPECT_EQ(pair.runtimeA.poolDiagnostics().interests.used, 1U);
    EXPECT_EQ(pair.runtimeA.poolDiagnostics().sessions.used, 1U);
  }
  ASSERT_TRUE(pair.peersA.replaceAcl(pair.peerA, revision, nullptr, 0));
  pair.runtimeA.iterate();
  EXPECT_EQ(pair.runtimeA.poolDiagnostics().interests.used, 0U);
  EXPECT_EQ(pair.runtimeA.poolDiagnostics().sessions.used, 1U);
}

TEST(SupLanRuntime, ReusedAckSlotCannotCompleteAnUnacceptedRead) {
  RuntimePair pair;
  const Supla::SupLan::AclEntry readOnly = {
      pair.resource, Supla::SupLan::kPermissionRead};
  ASSERT_TRUE(pair.peersA.replaceAcl(pair.peerA, 2, &readOnly, 1));
  uint8_t control[17] = {};
  control[4] = 0xFF;
  // Sequence 0 leaves a negative CONTROL result in cache slot 0.
  ASSERT_TRUE(pair.runtimeB.sendControl(pair.peerB, pair.resource, control,
                                       sizeof(control)));
  pair.pump(20);
  ASSERT_EQ(pair.appB.lastAckResult, 24U);
  const Supla::SupLan::AclEntry all = {
      pair.resource, static_cast<uint8_t>(Supla::SupLan::kPermissionRead |
                                         Supla::SupLan::kPermissionControl)};
  ASSERT_TRUE(pair.peersA.replaceAcl(pair.peerA, 3, &all, 1));
  for (unsigned i = 0; i < 63; ++i) {
    ASSERT_TRUE(pair.runtimeB.sendControl(pair.peerB, pair.resource, control,
                                         sizeof(control)));
    pair.pump(2);
  }
  ASSERT_EQ(pair.appB.acknowledged, 64U);
  const auto acknowledgements = pair.appB.acknowledged;
  pair.appA.stateAvailable = false;
  // Sequence 64 must not inherit sequence 0's cached result on retransmission.
  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
  pair.pump(20);
  EXPECT_EQ(pair.runtimeA.diagnostics().resourceNotFound, 1U);
  EXPECT_EQ(pair.runtimeA.diagnostics().readDispatched, 0U);
  EXPECT_EQ(pair.appB.acknowledged, acknowledgements);
  EXPECT_TRUE(pair.runtimeB.readNeedsRefresh(pair.peerB, pair.resource));
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().retries.used, 1U);
  // READ still recovers automatically when the resource becomes available.
  pair.appA.stateAvailable = true;
  pair.pumpMs(1000);
  EXPECT_EQ(pair.appB.stateCalls, 1U);
  EXPECT_EQ(pair.runtimeA.diagnostics().readDispatched, 1U);
  EXPECT_FALSE(pair.runtimeB.readNeedsRefresh(pair.peerB, pair.resource));
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().retries.used, 0U);
}

TEST(SupLanRuntime, ReusedAckSlotRetainsNewControlResultForDuplicates) {
  RuntimePair pair;
  uint8_t control[17] = {};
  control[4] = 0xFF;
  for (unsigned i = 0; i < 64; ++i) {
    ASSERT_TRUE(pair.runtimeB.sendControl(pair.peerB, pair.resource, control,
                                         sizeof(control)));
    pair.pump(2);
  }
  ASSERT_EQ(pair.appA.controlCalls, 64U);
  pair.runtimeA.testHooks()->dropNextAckTx = 1;
  ASSERT_TRUE(pair.runtimeB.sendControl(pair.peerB, pair.resource, control,
                                       sizeof(control)));
  pair.pump(20);
  EXPECT_EQ(pair.appA.controlCalls, 65U);
  EXPECT_EQ(pair.appB.acknowledged, 65U);
  EXPECT_EQ(pair.appB.lastAckResult, 3U);
  EXPECT_EQ(pair.runtimeA.diagnostics().controlDuplicateSuppressed, 1U);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().retries.used, 0U);
}

TEST(SupLanRuntime, FailedAeadCannotInvalidateAnOlderCachedAck) {
  RuntimePair pair;
  uint8_t control[17] = {};
  control[4] = 0xFF;
  ASSERT_TRUE(pair.runtimeB.sendControl(pair.peerB, pair.resource, control,
                                       sizeof(control)));
  pair.pump(2);
  Packet first;
  bool found = false;
  for (const Packet &packet : pair.network.history) {
    if (packet.source.address == pair.endpointB.address &&
        packet.bytes.size() >= 3 &&
        packet.bytes[0] == Supla::SupLan::kAdaptationFull &&
        packet.bytes[2] == Supla::SupLan::kFrameData) {
      first = packet;
      found = true;
      break;
    }
  }
  ASSERT_TRUE(found);
  for (unsigned i = 0; i < 63; ++i) {
    ASSERT_TRUE(pair.runtimeB.sendControl(pair.peerB, pair.resource, control,
                                         sizeof(control)));
    pair.pump(2);
  }
  pair.runtimeB.testHooks()->corruptNextDataTagTx = 1;
  ASSERT_TRUE(pair.runtimeB.sendControl(pair.peerB, pair.resource, control,
                                       sizeof(control)));
  pair.pump(2);
  ASSERT_EQ(pair.runtimeA.diagnostics().dataAuthFail, 1U);
  const auto acks = pair.runtimeA.diagnostics().ackTx;
  pair.network.packets.push_back(first);
  pair.runtimeA.iterate();
  EXPECT_EQ(pair.runtimeA.diagnostics().ackTx, acks + 1);
  EXPECT_EQ(pair.runtimeA.diagnostics().controlDuplicateSuppressed, 1U);
  EXPECT_EQ(pair.appA.controlCalls, 64U);
}

TEST(SupLanSleepySource, AuthenticatedSleepWakeAndActualInterestLoss) {
  RuntimePair pair;
  pair.appB.runtime = &pair.runtimeB;
  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
  pair.pump(30);
  ASSERT_EQ(pair.runtimeA.poolDiagnostics().interests.used, 1u);
  ASSERT_EQ(pair.appB.stateCalls, 1u);
  ASSERT_TRUE(pair.runtimeA.announceSleep(pair.peerA, 30));
  pair.pump(3);
  EXPECT_TRUE(pair.runtimeB.recoveryStatus(pair.peerB).sleeping);
  EXPECT_EQ(pair.runtimeA.poolDiagnostics().interests.used, 1u);
  // Unsolicited hint without an interested consumer must have no effect.
  ASSERT_TRUE(pair.runtimeA.announcePresence(pair.peerA, 200, false));
  pair.pump(3);
  EXPECT_EQ(pair.appB.awakeHints, 0);
  EXPECT_TRUE(pair.runtimeB.recoveryStatus(pair.peerB).sleeping);
  pair.appB.presenceAllowed = true;
  ASSERT_TRUE(pair.runtimeA.announcePresence(pair.peerA, 200, false));
  pair.pump(3);
  EXPECT_EQ(pair.appB.awakeHints, 1);
  EXPECT_FALSE(pair.runtimeB.recoveryStatus(pair.peerB).sleeping);
  EXPECT_EQ(pair.runtimeA.poolDiagnostics().interests.used, 1u);
  pair.runtimeA.clearInterests();
  ASSERT_EQ(pair.runtimeA.poolDiagnostics().interests.used, 0u);
  ASSERT_TRUE(pair.runtimeA.announcePresence(pair.peerA, 0, true));
  pair.pump(30);
  EXPECT_EQ(pair.appB.awakeHints, 2);
  EXPECT_EQ(pair.runtimeA.poolDiagnostics().interests.used, 1u);
  EXPECT_EQ(pair.appB.stateCalls, 2u);
}

TEST(SupLanSleepySource, ForgedAwakeHintCannotStartRepair) {
  RuntimePair pair;
  pair.appB.presenceAllowed = true;
  ASSERT_TRUE(pair.runtimeA.announcePresence(pair.peerA, 200, false));
  ASSERT_FALSE(pair.network.packets.empty());
  pair.network.packets.back().bytes.back() ^= 1;
  pair.pump(3);
  EXPECT_EQ(pair.appB.awakeHints, 0);
  EXPECT_FALSE(pair.runtimeB.readNeedsRefresh(pair.peerB, pair.resource));
}

TEST(SupLanSleepySource, ForgedHintsDoNotConsumeAuthenticatedPresenceQuota) {
  RuntimePair pair;
  pair.peersB.get(pair.peerB)->destination = true;
  Supla::Device::RemoteResourceManager resources(
      &pair.peersB, &pair.runtimeB, nullptr);
  ASSERT_TRUE(resources.consume({1, pair.resource,
                                Supla::SupLan::kPermissionRead}));
  ASSERT_EQ(resources.effectivePermissions(pair.resource) &
                Supla::SupLan::kPermissionRead,
            Supla::SupLan::kPermissionRead);
  pair.appB.remoteResources = &resources;
  ASSERT_TRUE(pair.runtimeA.announcePresence(pair.peerA, 200, false));
  ASSERT_EQ(pair.network.packets.size(), 1U);
  const Packet awake = pair.network.packets.back();
  Packet forged = awake;
  forged.bytes.back() ^= 1;
  pair.network.packets.clear();
  pair.network.packets.push_back(forged);
  pair.network.packets.push_back(forged);
  pair.network.packets.push_back(awake);
  ASSERT_TRUE(pair.runtimeA.announcePresence(pair.peerA, 0, true));
  pair.runtimeB.iterate();
  EXPECT_EQ(pair.runtimeB.diagnostics().invalidLocateDrop, 2U);
  EXPECT_EQ(pair.appB.awakeHints, 2);
}

TEST(SupLanSleepySource, ForgedPresenceHasBoundedCryptoWorkAndWindowRecovery) {
  RuntimePair pair;
  pair.appB.presenceAllowed = true;
  ASSERT_TRUE(pair.runtimeA.announcePresence(pair.peerA, 200, false));
  const Packet valid = pair.network.packets.back();
  Packet forged = valid;
  forged.bytes.back() ^= 1;
  pair.network.packets.assign(4, forged);
  pair.rng.resetCalls();
  pair.runtimeB.iterate();
  const auto callsPerPacket = pair.rng.hmacSha256Calls / 4;
  ASSERT_GT(callsPerPacket, 0U);
  pair.network.packets.assign(1000, forged);
  for (int i = 0; i < 250; ++i) pair.runtimeB.iterate();
  EXPECT_EQ(pair.rng.hmacSha256Calls,
      (2 * SUPLAN_MAX_PERSISTENT_PEERS + 8) * callsPerPacket);
  EXPECT_EQ(pair.appB.awakeHints, 0);
  EXPECT_TRUE(pair.network.packets.empty());
  pair.network.now += 1000;
  pair.network.packets.push_back(valid);
  pair.runtimeB.iterate();
  EXPECT_EQ(pair.appB.awakeHints, 1);
}

TEST(SupLanSleepySource, DuplicateHintCannotSpendFreshWakeRepairOpportunity) {
  RuntimePair pair;
  pair.peersB.get(pair.peerB)->destination = true;
  pair.appB.runtime = &pair.runtimeB;
  Supla::Device::RemoteResourceManager resources(
      &pair.peersB, &pair.runtimeB, nullptr);
  ASSERT_TRUE(resources.consume({1, pair.resource,
                                 Supla::SupLan::kPermissionRead}));
  pair.appB.remoteResources = &resources;
  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
  pair.pump(30);
  ASSERT_TRUE(pair.runtimeA.announcePresence(pair.peerA, 200, false));
  const Packet first = pair.network.packets.front();
  pair.runtimeB.iterate();
  ASSERT_TRUE(pair.runtimeA.announceSleep(pair.peerA, 30));
  pair.runtimeB.iterate();
  ASSERT_TRUE(pair.runtimeB.recoveryStatus(pair.peerB).sleeping);
  pair.network.packets.push_back(first);
  pair.network.packets.push_back(first);
  ASSERT_TRUE(pair.runtimeA.announcePresence(pair.peerA, 200, false));
  pair.runtimeB.iterate();
  EXPECT_FALSE(pair.runtimeB.recoveryStatus(pair.peerB).sleeping);
  EXPECT_EQ(resources.consumerCount(), 1);
  EXPECT_EQ(resources.resourceCount(), 1);
}

TEST(SupLanRuntime, MixedNegativeFloodCannotDispatchOrStarveAdmittedControl) {
  RuntimePair pair;
  Supla::Control::VirtualRelay relay;
  relay.getChannel()->setDefault(SUPLA_CHANNELFNC_POWERSWITCH);
  pair.appA.nativeElement = &relay;
  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
  pair.pump(30);
  for (unsigned i = 0; i < 32; ++i) {
    ASSERT_TRUE(pair.runtimeB.sendControlAction(pair.peerB, pair.resource,
                                               ACTION_TOGGLE, nullptr, 0));
    ASSERT_EQ(pair.network.packets.size(), 1U);
    const Packet valid = pair.network.packets.front();
    Packet badTag = valid;
    badTag.bytes.back() ^= 1;
    Packet unknownSession = valid;
    unknownSession.bytes[3] ^= 1;
    Packet malformed = valid;
    malformed.bytes = {Supla::SupLan::kAdaptationFragment, 1, 2};
    pair.network.packets.clear();
    pair.network.packets.push_back(unknownSession);
    pair.network.packets.push_back(badTag);
    pair.network.packets.push_back(malformed);
    pair.runtimeA.iterate();
    EXPECT_EQ(pair.appA.nativeControlCalls, i);
    pair.network.packets.push_back(valid);
    pair.network.packets.push_back(valid);  // Authenticated duplicate ACK path.
    pair.pump(3);
    EXPECT_EQ(pair.appA.nativeControlCalls, i + 1);
    EXPECT_EQ(relay.isOn(), i % 2 == 0);
    EXPECT_EQ(pair.runtimeB.poolDiagnostics().retries.used, 0U);
  }
  EXPECT_EQ(pair.runtimeA.diagnostics().dataAuthFail, 32U);
  EXPECT_EQ(pair.runtimeA.diagnostics().controlDuplicateSuppressed, 32U);
}

TEST(SupLanSleepySource, LostResetHealthyReadRepairRevocationAndLocalActuator) {
  RuntimePair pair;
  pair.peersB.get(pair.peerB)->destination = true;
  pair.appB.runtime = &pair.runtimeB;
  pair.appA.fullSnapshots = true;
  Supla::Device::RemoteResourceManager manager(&pair.peersB, &pair.runtimeB,
                                               nullptr);
  pair.appB.remoteResources = &manager;
  Supla::Control::VirtualRelay local;
  local.getChannel()->setDefault(SUPLA_CHANNELFNC_POWERSWITCH);
  ASSERT_TRUE(manager.consume({1, pair.resource,
                               Supla::SupLan::kPermissionRead}));
  manager.iterate(pair.network.now);
  pair.pump(30);
  ASSERT_TRUE(manager.state(pair.resource, pair.network.now).available());
  ASSERT_EQ(pair.runtimeA.poolDiagnostics().interests.used, 1U);
  const auto sessions = pair.runtimeB.diagnostics().sessionEstablished;
  pair.runtimeA.clearInterests();
  ASSERT_TRUE(pair.runtimeA.announcePresence(pair.peerA, 0, true));
  pair.network.packets.clear();  // Lose only RESET, with a healthy transport.
  pair.network.now = 59999;
  manager.iterate(pair.network.now);
  EXPECT_EQ(pair.runtimeA.poolDiagnostics().interests.used, 0U);
  local.turnOn();
  EXPECT_TRUE(local.isOn());
  pair.network.now = 60001;
  manager.iterate(pair.network.now);
  pair.pump(30);
  EXPECT_EQ(pair.runtimeA.poolDiagnostics().interests.used, 1U);
  EXPECT_TRUE(manager.state(pair.resource, pair.network.now).available());
  EXPECT_EQ(pair.runtimeB.diagnostics().sessionEstablished, sessions);
  ASSERT_TRUE(pair.peersB.replaceAcl(pair.peerB, 2, nullptr, 0));
  manager.authorizationChanged();
  EXPECT_FALSE(manager.state(pair.resource, pair.network.now).available());
  EXPECT_EQ(manager.effectivePermissions(pair.resource), 0);
  local.turnOff();
  EXPECT_FALSE(local.isOn());
  EXPECT_EQ(manager.consumerCount(), 1);
}

TEST(SupLanSleepySource, LongIntentionalSleepRetainsAuthorizedReadRecipients) {
  RuntimePair pair;
  pair.appA.fullSnapshots = true;
  Supla::Device::ServerAssociations associations(&pair.peersA, &pair.runtimeA);
  Supla::Protocol::SupLan source(nullptr, &pair.peersA, nullptr, 0);
  source.attachRuntime(&pair.runtimeA);
  source.attachServerAssociations(&associations);
  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
  pair.pump(30);
  ASSERT_EQ(pair.runtimeA.poolDiagnostics().interests.used, 1U);
  for (int cycle = 0; cycle < 2; ++cycle) {
    source.announceSleep(360);
    pair.pump(3);
    pair.network.now += 360000;
    source.beginWake(200, true);
    const auto deadline = source.remoteResources()->presence().awakeUntilMs_;
    // Neither RESET nor AWAKE needs to reach the Destination for retained
    // Source recipients to survive the first runtime iteration after wake.
    source.iterate(pair.network.now);
    pair.network.packets.erase(std::remove_if(pair.network.packets.begin(),
        pair.network.packets.end(), [](const Packet &packet) {
          return (packet.bytes.size() == 50 || packet.bytes.size() == 52) &&
              packet.bytes[0] == Supla::SupLan::kVersion &&
              (packet.bytes[1] == 6 || packet.bytes[1] == 7);
        }), pair.network.packets.end());
    ASSERT_EQ(pair.runtimeA.poolDiagnostics().interests.used, 1U) << cycle;
    const auto states = pair.appB.stateCalls;
    pair.appA.value = cycle + 1;
    pair.runtimeA.publishCurrentInterests(pair.peerA);
    pair.pump(3);
    EXPECT_GT(pair.appB.stateCalls, states);
    EXPECT_EQ(pair.appB.value, cycle + 1);
    EXPECT_EQ(source.remoteResources()->presence().awakeUntilMs_, deadline);
    // A READ admitted just before the advertised deadline may complete, but
    // neither it nor its ACK may turn this fixed wake into an indefinite one.
    pair.network.now = deadline - 1;
    ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
    source.iterate(pair.network.now);
    pair.network.now = deadline + 1;
    pair.runtimeB.iterate();
    EXPECT_EQ(source.remoteResources()->presence().awakeUntilMs_, deadline);
    for (int i = 0; i <= SUPLAN_MAX_PERSISTENT_PEERS; ++i) {
      pair.network.now += 10;
      source.iterate(pair.network.now);
      pair.runtimeB.iterate();
    }
    EXPECT_TRUE(source.wakeWindowComplete());
    EXPECT_EQ(source.remoteResources()->presence().awakeUntilMs_, deadline);
  }
}

TEST(SupLanRuntime, NativeControlMaximumParamsUsesProtectedRetryAndDedupe) {
  RuntimePair pair;
  pair.runtimeA.testHooks()->dropNextAckTx = 1;
  uint8_t params[16] = {1};
  ASSERT_TRUE(pair.runtimeB.sendControlAction(pair.peerB, pair.resource,
      ACTION_SET_RELAY_PARAMETERS, params, sizeof(params)));
  pair.pumpMs(400);
  ASSERT_EQ(pair.appA.nativeControlCalls, 1U);
  EXPECT_EQ(pair.appA.nativeParams.size(), 16U);
  EXPECT_EQ(pair.appB.lastAckResult, 3);
  auto frames = protectedDataFromB(pair, 0);
  ASSERT_GE(frames.size(), 2U);
  EXPECT_EQ(frames[0], frames[1]);
  EXPECT_EQ(frames[0].size(), 65U);
  pair.network.send(pair.endpointB, pair.endpointA, false,
                     frames[0].data(), frames[0].size());
  pair.pump(5);
  EXPECT_EQ(pair.appA.nativeControlCalls, 1U);
  EXPECT_GT(pair.runtimeA.diagnostics().controlDuplicateSuppressed, 0U);
}
TEST(SupLanRuntime, NativeControlWithoutSessionExpiresAtOriginalDeadline) {
  RuntimePair pair;
  pair.datagramsA.available = false;
  ASSERT_TRUE(pair.runtimeB.sendControlAction(pair.peerB, pair.resource,
                                             ACTION_TOGGLE, nullptr, 0));
  pair.pumpMs(5000);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().deferredEvents.used, 0U);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().retries.used, 0U);
  pair.network.packets.clear();
  pair.datagramsA.available = true;
  pair.pumpMs(1000);
  EXPECT_EQ(pair.appA.nativeControlCalls, 0U);
}
TEST(SupLanRuntime, NativeControlUnsupportedHasExplicitAck) {
  RuntimePair pair;
  ASSERT_TRUE(pair.runtimeB.sendControlAction(pair.peerB, pair.resource,
                                             999999, nullptr, 0));
  pair.pump(20);
  EXPECT_EQ(pair.appB.lastAckResult, 33);
  EXPECT_EQ(pair.appA.nativeControlCalls, 0U);
}
TEST(SupLanRuntime, OwnedControlCancellationDoesNotCancelAnotherHandle) {
  RuntimePair pair;
  pair.datagramsA.available = false;
  ASSERT_TRUE(pair.runtimeB.sendControlAction(pair.peerB, pair.resource,
                                             ACTION_TOGGLE, nullptr, 0, 1));
  ASSERT_TRUE(pair.runtimeB.sendControlAction(pair.peerB, pair.resource,
                                             ACTION_TOGGLE, nullptr, 0, 2));
  pair.runtimeB.cancelOwnedControls(1);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().deferredEvents.used, 1U);
  pair.datagramsA.available = true;
  pair.pump(30);
  EXPECT_EQ(pair.appA.nativeControlCalls, 1U);
}
TEST(SupLanRuntime, ActionGrantPublishesWithoutAnyReadInterest) {
  RuntimePair pair;
  const ResourceId actionResource = {Supla::SupLan::kResourceTypeChannel,
                                      50002};
  uint8_t event[15] = {0xff, 4};
  ASSERT_TRUE(pair.runtimeB.publishAction(pair.actionPeerB, actionResource,
                                        event, sizeof(event)));
  pair.pump(30);
  EXPECT_EQ(pair.appA.actionCalls, 1U);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().interests.used, 0U);
  EXPECT_EQ(pair.runtimeA.poolDiagnostics().interests.used, 0U);
}

TEST(SupLanRuntime, NativeControlCodecGoldenEnvelopeAndRejectsLengths) {
  using namespace Supla::SupLan;  // NOLINT(build/namespaces)
  uint8_t payload[21] = {};
  size_t length = 0;
  uint8_t params[16] = {1};
  ASSERT_TRUE(encodeControlAction(237, params, 16, payload, sizeof(payload),
                                 &length));
  EXPECT_EQ(length, 21U);
  const uint8_t expected[5] = {0, 0, 0, 237, 16};
  EXPECT_EQ(memcmp(payload, expected, 5), 0);
  uint8_t body[26] = {};
  ASSERT_TRUE(encodeResourceId(kResourceTypeChannel, 0x01020304, body));
  memcpy(body + 5, payload, length);
  uint8_t application[32];
  ASSERT_TRUE(encodeApplicationData(kMessageClassNative, kNativeControlAction,
      kAckRequired, body, sizeof(body), application, sizeof(application),
      &length));
  EXPECT_EQ(length, 32U);
  const uint8_t header[16] = {2, 0, 0, 0, 9, 1, 1, 1, 2, 3, 4,
                             0, 0, 0, 237, 16};
  EXPECT_EQ(memcmp(application, header, sizeof(header)), 0);
  ControlActionView view = {};
  ASSERT_TRUE(decodeControlAction(payload, 21, &view));
  EXPECT_EQ(view.actionId, 237U);
  EXPECT_FALSE(decodeControlAction(payload, 20, &view));
  EXPECT_FALSE(encodeControlAction(237, params, 17, payload, sizeof(payload),
                                  &length));
  payload[4] = 17;
  EXPECT_FALSE(decodeControlAction(payload, 21, &view));
  ASSERT_TRUE(encodeControlAction(10, nullptr, 0, payload, sizeof(payload),
                                 &length));
  ASSERT_EQ(length, 5U);
  ASSERT_TRUE(encodeResourceId(kResourceTypeChannel, 0x01020304, body));
  memcpy(body + 5, payload, length);
  ASSERT_TRUE(encodeApplicationData(kMessageClassNative, kNativeControlAction,
      kAckRequired, body, 10, application, sizeof(application), &length));
  const uint8_t emptyParams[16] = {2, 0, 0, 0, 9, 1, 1, 1, 2, 3, 4,
                                 0, 0, 0, 10, 0};
  EXPECT_EQ(length, sizeof(emptyParams));
  EXPECT_EQ(memcmp(application, emptyParams, sizeof(emptyParams)), 0);
}

TEST(SupLanRuntime, NativeControlLostAckRetriesIdenticalCiphertextOnce) {
  RuntimePair pair;
  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
  pair.pump(20);
  pair.runtimeA.testHooks()->dropNextAckTx = 1;
  const auto start = pair.network.history.size();
  ASSERT_TRUE(pair.runtimeB.sendControlAction(pair.peerB, pair.resource,
                                             ACTION_TOGGLE, nullptr, 0));
  pair.pumpMs(400);
  EXPECT_EQ(pair.appA.nativeControlCalls, 1U);
  EXPECT_EQ(pair.appB.lastAckResult, 3);
  auto frames = protectedDataFromB(pair, start);
  ASSERT_GE(frames.size(), 2U);
  EXPECT_EQ(frames[0], frames[1]);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().retries.used, 0U);
}
TEST(SupLanRuntime, NativeControlSessionSwitchDropsAlreadyTransmittedCommand) {
  RuntimePair pair;
  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
  pair.pump(20);
  pair.runtimeA.testHooks()->dropNextAckTx = 10;
  ASSERT_TRUE(pair.runtimeB.sendControlAction(pair.peerB, pair.resource,
                                             ACTION_TOGGLE, nullptr, 0));
  pair.pump(3);
  ASSERT_EQ(pair.appA.nativeControlCalls, 1U);
  ASSERT_TRUE(pair.runtimeB.forgetSession(pair.peerB));
  pair.network.packets.clear();
  pair.runtimeA.testHooks()->dropNextAckTx = 0;
  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
  pair.pumpMs(700);
  EXPECT_EQ(pair.appA.nativeControlCalls, 1U);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().retries.used, 0U);
}

TEST(SupLanRuntime, NativeControlCapacityFailureDoesNotEvictAdmittedCommands) {
  RuntimePair pair;
  pair.datagramsA.available = false;
  for (int i = 0; i < SUPLAN_MAX_DEFERRED_APP_EVENTS; ++i)
    ASSERT_TRUE(pair.runtimeB.sendControlAction(pair.peerB, pair.resource,
                                               ACTION_TOGGLE, nullptr, 0));
  EXPECT_FALSE(pair.runtimeB.sendControlAction(pair.peerB, pair.resource,
                                              ACTION_TOGGLE, nullptr, 0));
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().deferredEvents.used,
            SUPLAN_MAX_DEFERRED_APP_EVENTS);
  pair.pumpMs(5000);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().deferredEvents.used, 0U);
  EXPECT_EQ(pair.appB.acknowledged, 0U);
}

TEST(SupLanRuntime, NativeControlQueuedBehindRetriesExpiresBeforeSlotReuse) {
  RuntimePair pair;
  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
  pair.pump(20);
  pair.runtimeA.testHooks()->dropNextAckTx = 100;
  const uint32_t admitted = pair.network.now;
  for (int i = 0; i < SUPLAN_MAX_RETRY_SLOTS; ++i)
    ASSERT_TRUE(pair.runtimeB.sendControlAction(pair.peerB, pair.resource,
                                               ACTION_TOGGLE, nullptr, 0));
  ASSERT_TRUE(pair.runtimeB.sendControlAction(pair.peerB, pair.resource,
                                             ACTION_TOGGLE, nullptr, 0));
  pair.runtimeA.iterate();
  ASSERT_EQ(pair.appA.nativeControlCalls,
            static_cast<unsigned>(SUPLAN_MAX_RETRY_SLOTS));
  ASSERT_EQ(pair.runtimeB.poolDiagnostics().deferredEvents.used, 1U);
  pair.network.now = admitted + 5000;
  // Admission itself drains work before iterateRecovery runs. An expired
  // Native/9 must not be sent when the newly admitted command frees retries.
  ASSERT_TRUE(pair.runtimeB.sendControlAction(pair.peerB, pair.resource,
                                             ACTION_TOGGLE, nullptr, 0));
  pair.runtimeA.iterate();
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().deferredEvents.used, 0U);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().retries.used, 1U);
  EXPECT_EQ(pair.appA.nativeControlCalls,
            static_cast<unsigned>(SUPLAN_MAX_RETRY_SLOTS + 1));
}

TEST(SupLanRuntime, NativeToggleAndGateSbsDeduplicateRealSourceRelayDispatch) {
  Supla::Channel::resetToDefaults();
  SimpleTime clock;
  for (const uint32_t action : {ACTION_TOGGLE, ACTION_OPEN_CLOSE}) {
    RuntimePair pair;
    class CountingRelay : public Supla::Control::VirtualRelay {
     public:
      int32_t handleNewValueFromServer(TSD_SuplaChannelNewValue *value)
          override {
        ++dispatches;
        return VirtualRelay::handleNewValueFromServer(value);
      }
      int dispatches = 0;
    } relay;
    relay.setDefaultFunction(action == ACTION_TOGGLE
        ? SUPLA_CHANNELFNC_POWERSWITCH : SUPLA_CHANNELFNC_CONTROLLINGTHEGATE);
    relay.onInit();
    pair.appA.nativeElement = &relay;
    ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
    pair.pump(20);
    pair.runtimeA.testHooks()->dropNextAckTx = 1;
    ASSERT_TRUE(pair.runtimeB.sendControlAction(pair.peerB, pair.resource,
                                               action, nullptr, 0));
    pair.pumpMs(400);
    EXPECT_EQ(relay.dispatches, 1);
    EXPECT_TRUE(relay.isOn());
    EXPECT_EQ(pair.appA.nativeControlCalls, 1U);
    EXPECT_EQ(pair.appB.lastAckResult, 3);
  }
}

TEST(SupLanRuntime, ControlDemandAutomaticallyReadsAndSharesHealthySnapshot) {
  RuntimePair pair;
  pair.peersB.get(pair.peerB)->destination = true;
  pair.appA.fullSnapshots = true;
  pair.appA.value = 1;
  Supla::Device::RemoteResourceManager manager(&pair.peersB, &pair.runtimeB,
                                               nullptr);
  pair.appB.remoteResources = &manager;
  ASSERT_TRUE(manager.consume({1, pair.resource,
                              Supla::SupLan::kPermissionControl, true}));
  manager.iterate(0);  // The application has not polled state().
  pair.pump(30);
  EXPECT_EQ(pair.runtimeA.diagnostics().readDispatched, 1U);
  EXPECT_TRUE(manager.state(pair.resource, pair.network.now).available());
  const auto snapshotBuilds = pair.appA.readCalls;
  ASSERT_TRUE(manager.consume({2, pair.resource,
                              Supla::SupLan::kPermissionControl, true}));
  manager.iterate(pair.network.now);
  pair.pump(30);
  EXPECT_EQ(pair.appA.readCalls, snapshotBuilds);
  EXPECT_EQ(pair.runtimeA.diagnostics().readDispatched, 1U);
  EXPECT_EQ(manager.resourceCount(), 1);
  EXPECT_EQ(manager.consumerCount(), 2);
  ASSERT_TRUE(pair.runtimeB.forgetSession(pair.peerB));
  EXPECT_TRUE(manager.state(pair.resource, pair.network.now).available());
  ASSERT_TRUE(pair.peersB.replaceAcl(pair.peerB, 2, nullptr, 0));
  EXPECT_FALSE(manager.state(pair.resource, pair.network.now).available());
  EXPECT_EQ(manager.effectivePermissions(pair.resource), 0);
}

TEST(SupLanRuntime, ScopedActionGrantNeedsNoReadAndRevocationStopsPublication) {
  RuntimePair pair;
  const ResourceId resource = {Supla::SupLan::kResourceTypeChannel, 50002};
  Supla::SupLan::AclEntry scope = {
      {Supla::SupLan::kResourceTypeDevice, 1002},
      Supla::SupLan::kPermissionActionTrigger};
  ASSERT_TRUE(pair.peersB.replaceAcl(pair.actionPeerB, 2, &scope, 1));
  ASSERT_TRUE(pair.peersA.replaceAcl(pair.actionPeerA, 2, &scope, 1));
  uint8_t event[15] = {0xff, 4};
  ASSERT_TRUE(pair.runtimeB.publishAction(pair.actionPeerB, resource, event,
                                         sizeof(event)));
  pair.pump(30);
  EXPECT_EQ(pair.appA.actionCalls, 1U);
  EXPECT_EQ(pair.runtimeA.poolDiagnostics().interests.used, 0U);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().interests.used, 0U);
  scope.permissions = Supla::SupLan::kPermissionRead;
  ASSERT_TRUE(pair.peersB.replaceAcl(pair.actionPeerB, 3, &scope, 1));
  EXPECT_FALSE(pair.runtimeB.publishAction(pair.actionPeerB, resource, event,
                                          sizeof(event)));
  pair.pump(30);
  EXPECT_EQ(pair.appA.actionCalls, 1U);
}

TEST(SupLanRuntime, ActionOnlySessionEvictionRepairsWithoutReadInterests) {
  RuntimePair pair;
  const ResourceId resource = {Supla::SupLan::kResourceTypeChannel, 50002};
  uint8_t event[15] = {0xff, 4};
  ASSERT_TRUE(pair.runtimeB.publishAction(pair.actionPeerB, resource, event,
                                         sizeof(event)));
  pair.pump(30);
  ASSERT_EQ(pair.appA.actionCalls, 1U);
  ASSERT_TRUE(pair.runtimeA.forgetSession(pair.actionPeerA));
  pair.runtimeB.clearEndpoint(pair.actionPeerB);
  const auto locateBefore = pair.runtimeB.diagnostics().locateTx;
  ASSERT_TRUE(pair.runtimeB.publishAction(pair.actionPeerB, resource, event,
                                         sizeof(event)));
  pair.pump(30);
  EXPECT_EQ(pair.appA.actionCalls, 2U);
  EXPECT_GT(pair.runtimeB.diagnostics().locateTx, locateBefore);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().retries.used, 0U);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().interests.used, 0U);
  EXPECT_EQ(pair.runtimeA.poolDiagnostics().interests.used, 0U);
}

TEST(SupLanRuntime, UnreachableActionPeerCannotBlockHealthyRecipientOrReplay) {
  RuntimePair pair;
  const ResourceId resource = {Supla::SupLan::kResourceTypeChannel, 50002};
  Supla::SupLan::PeerTable peersC;
  FakeApplication appC;
  FakeDatagramPort portC(&pair.network, {0x0300007f, 2016});
  Supla::SupLan::Runtime runtimeC(&pair.crypto, &portC, &appC, &peersC,
                                  RuntimePair::nodeAddress(1003), 29);
  Supla::SupLan::PeerContext context = {
      Supla::SupLan::kAuthorityServer, 0,
      {Supla::SupLan::kNodeIdDevice, 1002},
      {Supla::SupLan::kNodeIdDevice, 1003}, 1, 1};
  uint8_t key[32] = {2};
  Supla::SupLan::AclEntry acl = {
      resource, Supla::SupLan::kPermissionActionTrigger};
  uint8_t offlinePeer, destinationPeer;
  ASSERT_TRUE(pair.peersB.addPeer(&context, key, 1, &acl, 1, &offlinePeer));
  ASSERT_TRUE(peersC.addPeer(&context, key, 1, &acl, 1,
                            &destinationPeer, true));
  portC.available = false;
  uint8_t event[15] = {0xff, 4};
  // Attempt the unreachable recipient first, then the healthy one.
  ASSERT_TRUE(pair.runtimeB.publishAction(offlinePeer, resource, event,
                                         sizeof(event)));
  ASSERT_TRUE(pair.runtimeB.publishAction(pair.actionPeerB, resource, event,
                                         sizeof(event)));
  pair.pumpMs(500);
  EXPECT_EQ(pair.appA.actionCalls, 1U);
  EXPECT_EQ(appC.actionCalls, 0U);
  EXPECT_EQ(pair.runtimeA.poolDiagnostics().interests.used, 0U);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().interests.used, 0U);
  pair.pumpMs(3000);
  portC.available = true;
  for (int i = 0; i < 100; ++i) {
    pair.network.now += 10;
    runtimeC.iterate();
    pair.runtimeA.iterate();
    pair.runtimeB.iterate();
  }
  EXPECT_EQ(appC.actionCalls, 0U);
  EXPECT_EQ(pair.appA.actionCalls, 1U);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().deferredEvents.used, 0U);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().retries.used, 0U);
  EXPECT_TRUE(pair.peersB.authorize(offlinePeer, resource,
                                   Supla::SupLan::kPermissionActionTrigger));
}

TEST(SupLanSleepySource, SharedPeerResetRepairsOnlyAuthorizedResourceReads) {
  using namespace Supla::SupLan;  // NOLINT(build/namespaces)
  RuntimePair pair;
  pair.peersB.get(pair.peerB)->destination = true;
  AclEntry acl[] = {{{kResourceTypeChannel, 50001}, kPermissionRead},
                    {{kResourceTypeChannel, 50003}, kPermissionRead},
                    {{kResourceTypeChannel, 50004}, kPermissionRead}};
  ASSERT_TRUE(pair.peersA.replaceAcl(pair.peerA, 2, acl, 3));
  ASSERT_TRUE(pair.peersB.replaceAcl(pair.peerB, 2, acl, 3));
  Supla::Device::RemoteResourceManager resources(
      &pair.peersB, &pair.runtimeB, nullptr);
  pair.appB.remoteResources = &resources;
  pair.appB.runtime = &pair.runtimeB;
  pair.appA.fullSnapshots = pair.appB.fullSnapshots = true;
  for (const auto &entry : acl)
    ASSERT_TRUE(resources.consume({entry.resource.id, entry.resource,
                                   kPermissionRead}));
  resources.iterate(pair.network.now);
  pair.pump(40);
  ASSERT_EQ(pair.runtimeA.poolDiagnostics().interests.used, 3U);
  for (const auto &entry : acl)
    ASSERT_TRUE(resources.state(entry.resource, pair.network.now).available());
  pair.runtimeA.clearInterests();
  ASSERT_TRUE(pair.runtimeA.announcePresence(pair.peerA, 0, true));
  pair.runtimeB.iterate();
  resources.iterate(pair.network.now);
  pair.pump(40);
  EXPECT_EQ(pair.runtimeA.poolDiagnostics().interests.used, 3U);
  auto revoked = resources.state(acl[0].resource, pair.network.now);
  ASSERT_TRUE(pair.peersA.replaceAcl(pair.peerA, 3, acl + 1, 2));
  ASSERT_TRUE(pair.peersB.replaceAcl(pair.peerB, 3, acl + 1, 2));
  pair.runtimeA.authorizationChanged(pair.peerA);
  pair.runtimeB.authorizationChanged(pair.peerB);
  resources.authorizationChanged();
  EXPECT_FALSE(revoked.available());
  pair.runtimeA.clearInterests();
  pair.network.now += 1000;
  ASSERT_TRUE(pair.runtimeA.announcePresence(pair.peerA, 0, true));
  pair.runtimeB.iterate();
  resources.iterate(pair.network.now);
  pair.pump(40);
  EXPECT_EQ(pair.runtimeA.poolDiagnostics().interests.used, 2U);
  EXPECT_FALSE(resources.state(acl[0].resource, pair.network.now).available());
  EXPECT_TRUE(resources.state(acl[1].resource, pair.network.now).available());
  EXPECT_TRUE(resources.state(acl[2].resource, pair.network.now).available());
}

TEST(SupLanSleepySource, SharedWakeDeadlineExpiresAcrossMillisWrap) {
  using namespace Supla::SupLan;  // NOLINT(build/namespaces)
  RuntimePair pair;
  pair.peersB.get(pair.peerB)->destination = true;
  Supla::Device::RemoteResourceManager resources(
      &pair.peersB, &pair.runtimeB, nullptr);
  pair.appB.runtime = &pair.runtimeB;
  pair.appB.remoteResources = &resources;
  ASSERT_TRUE(resources.consume({1, pair.resource, kPermissionRead}));
  pair.network.now = UINT32_MAX - 100;
  uint8_t nonce[16] = {1};
  resources.peerAwake(pair.peerB, 200, false, nonce, pair.network.now);
  resources.iterate(pair.network.now);
  EXPECT_EQ(pair.runtimeB.recoveryStatus(pair.peerB).deadlineMs, 99U);
  pair.network.now += 500;
  // Renew demand after its original awake window, without another hint.
  resources.remove(1);
  ASSERT_TRUE(resources.consume({2, pair.resource, kPermissionRead}));
  resources.iterate(pair.network.now);
  // A stale wrapped deadline must not shorten the new recovery cycle.
  EXPECT_GT(static_cast<int32_t>(
      pair.runtimeB.recoveryStatus(pair.peerB).deadlineMs - pair.network.now),
      200);
}

#ifdef SUPLAN_R2_PRODUCT_TESTS
TEST(SupLanRuntime, R2Q40ProtectedRetryAndAcknowledgement) {
  RuntimePair pair;
  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
  pair.pumpMs(200);
  std::vector<uint8_t> body(1 + 12 * 40, 0);
  body[0] = 40;
  for (int i = 0; i < 40; ++i) {
    auto entry = body.data() + 1 + 12 * i;
    entry[0] = entry[1] = entry[6] = 1;
    Supla::SupLan::putUint32(entry + 2, pair.resource.id);
    Supla::SupLan::putUint32(entry + 7, 60001 + i);
  }
  const auto before = pair.network.history.size();
  EXPECT_FALSE(
      pair.runtimeA.sendBindings(pair.peerA, body.data(), body.size()));
  EXPECT_EQ(pair.network.history.size(), before);
  pair.appA.bindingPoolEnabled = true;
  std::vector<std::vector<uint8_t>> sent;
  const auto fragments = pair.runtimeA.diagnostics().fragmentTx;
  pair.runtimeB.testHooks()->dropNextAckTx = 1;
  ASSERT_TRUE(pair.runtimeA.sendBindings(pair.peerA, body.data(), body.size()));
  EXPECT_EQ(pair.appA.largeRetry.length, 519U);
  EXPECT_FALSE(
      pair.runtimeA.sendBindings(pair.peerA, body.data(), body.size()));
  pair.pumpMs(400);
  EXPECT_EQ(pair.appB.bindingCalls, 1);
  EXPECT_EQ(pair.appB.lastBindings, body);
  EXPECT_FALSE(pair.runtimeA.bindingsInFlight());
  EXPECT_GE(pair.runtimeA.diagnostics().fragmentTx - fragments, 6U);
  for (size_t i = before; i < pair.network.history.size(); ++i) {
    const auto &packet = pair.network.history[i];
    const auto &bytes = packet.bytes;
    if (packet.source.address != pair.endpointA.address || bytes.size() < 9 ||
        bytes[0] != Supla::SupLan::kAdaptationFragment ||
        Supla::SupLan::getUint16(bytes.data() + 5) != 519) continue;
    const size_t offset = Supla::SupLan::getUint16(bytes.data() + 7);
    if (!offset) sent.emplace_back();
    ASSERT_FALSE(sent.empty());
    ASSERT_EQ(offset, sent.back().size());
    sent.back().insert(sent.back().end(), bytes.begin() + 9, bytes.end());
  }
  ASSERT_GE(sent.size(), 2u);
  ASSERT_EQ(sent[0].size(), 519U);
  ASSERT_EQ(sent[1].size(), 519U);
  EXPECT_EQ(sent[0], sent[1]);
  const auto after = pair.network.history.size();
  std::vector<uint8_t> overflow(1 + 12 * (SUPLAN_MAX_BINDINGS_PER_PEER + 1), 0);
  overflow[0] = SUPLAN_MAX_BINDINGS_PER_PEER + 1;
  EXPECT_FALSE(pair.runtimeA.sendBindings(pair.peerA, overflow.data(),
                                         overflow.size()));
  EXPECT_EQ(pair.network.history.size(), after);
  ASSERT_TRUE(pair.runtimeA.sendBindings(pair.peerA, body.data(), body.size()));
  pair.runtimeA.authorizationChanged(pair.peerA);
  EXPECT_FALSE(pair.runtimeA.bindingsInFlight());
  // Session replacement drops protected retry; no replay into a new SESSION.
  ASSERT_TRUE(pair.runtimeA.sendBindings(pair.peerA, body.data(), body.size()));
  ASSERT_TRUE(pair.runtimeA.forgetSession(pair.peerA));
  pair.runtimeA.iterate();
  EXPECT_FALSE(pair.runtimeA.bindingsInFlight());
}

#endif  // SUPLAN_R2_PRODUCT_TESTS

TEST(SupLanRuntime, DiagnosticsObserveLogicalReadDispatchAndAckWithoutTraffic) {
  using E = Supla::SupLan::DiagnosticEvent;
  RuntimePair pair;
  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
  EXPECT_EQ(pair.appB.diagnosticEvents[static_cast<unsigned>(E::ReadAccepted)],
            1U);
  EXPECT_EQ(pair.appB.diagnosticEvents[static_cast<unsigned>(E::ReadTransmit)],
            0U);
  const auto dependencies = pair.runtimeB.poolDiagnostics().deferredEvents.used;
  pair.pump(40);
  EXPECT_GT(pair.appB.diagnosticEvents[static_cast<unsigned>(E::ReadTransmit)],
            0U);
  EXPECT_EQ(pair.appB.acknowledged, 1U);
  EXPECT_EQ(pair.appB.stateCalls, 1U);
  const auto frames = pair.runtimeB.diagnostics().dataTx;
  for (int i = 0; i < 1000; ++i) pair.runtimeB.iterate();
  EXPECT_EQ(pair.runtimeB.diagnostics().dataTx, frames);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().deferredEvents.used, dependencies);
}

TEST(SupLanRuntime, PresenceDiagnosticsIgnoreEmptyPeerSlots) {
  using E = Supla::SupLan::DiagnosticEvent;
  RuntimePair pair;
  const auto frames = pair.network.history.size();
  for (uint16_t index = 0; index < SUPLAN_MAX_PERSISTENT_PEERS; ++index) {
    if (pair.peersB.get(index)) continue;
    EXPECT_FALSE(pair.runtimeB.announcePresence(index, 0, true));
  }
  EXPECT_FALSE(pair.runtimeB.announcePresence(0xff, 0, true));
  EXPECT_EQ(pair.appB.diagnosticEvents[static_cast<unsigned>(E::ResetSkipped)],
            0U);
  EXPECT_EQ(pair.network.history.size(), frames);

  // A real Destination-only association still explains why no reset is sent.
  ASSERT_NE(pair.peersB.get(pair.peerB), nullptr);
  EXPECT_FALSE(pair.runtimeB.announcePresence(pair.peerB, 0, true));
  EXPECT_EQ(pair.appB.diagnosticEvents[static_cast<unsigned>(E::ResetSkipped)],
            1U);
  EXPECT_EQ(pair.network.history.size(), frames);

  EXPECT_TRUE(pair.runtimeA.announcePresence(pair.peerA, 0, true));
  EXPECT_EQ(pair.appA.diagnosticEvents[static_cast<unsigned>(E::ResetTx)], 1U);
}

namespace {
// Hold Source INIT while Destination starts its own attempt. Both actual
// runtimes have competing initiators before Source wins role arbitration.
void startCompetingHandshakes(RuntimePair &pair) {
  using namespace Supla::SupLan;  // NOLINT(build/namespaces)
  pair.appA.bindingPoolEnabled = true;
  pair.peersA.get(pair.peerA)->endpoint = pair.endpointB;
  pair.peersA.get(pair.peerA)->endpointState = kPeerEndpointLocateCandidate;
  pair.peersB.get(pair.peerB)->endpoint = pair.endpointA;
  pair.peersB.get(pair.peerB)->endpointState = kPeerEndpointLocateCandidate;
  const uint8_t empty = 0;
  ASSERT_FALSE(pair.runtimeA.sendBindings(pair.peerA, &empty, 1));
  ASSERT_EQ(pair.runtimeA.diagnostics().sessionInitTx, 1U);
  auto held = pair.network.packets;
  pair.network.packets.clear();
  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
  pair.runtimeB.iterate();
  ASSERT_EQ(pair.runtimeB.diagnostics().sessionInitTx, 1U);
  pair.runtimeA.iterate();  // Source ignores the competing Destination INIT.
  pair.network.packets.insert(pair.network.packets.end(),
                              held.begin(), held.end());
  pair.runtimeB.iterate();  // Destination yields and becomes a responder.
}
}  // namespace

TEST(SupLanHandshake, SimultaneousReadWaitsForResponderAndUsesFirstSession) {
  using namespace Supla::SupLan;  // NOLINT(build/namespaces)
  RuntimePair pair;
  startCompetingHandshakes(pair);
  ASSERT_EQ(pair.runtimeB.diagnostics().sessionInitTx, 1U);
  EXPECT_FALSE(pair.runtimeB.hasSession(pair.peerB));
  EXPECT_EQ(pair.appA.readCalls, 0U);
  // READ recovery must keep waiting for this responder handshake, rather
  // than starting another INIT on every loop.
  for (int i = 0; i < 10; ++i) pair.runtimeB.iterate();
  EXPECT_EQ(pair.runtimeB.diagnostics().sessionInitTx, 1U);
  pair.runtimeA.iterate();  // Source receives ACCEPT and can send bindings.
  ASSERT_TRUE(pair.runtimeA.hasSession(pair.peerA));
  const uint8_t empty = 0;
  ASSERT_TRUE(pair.runtimeA.sendBindings(pair.peerA, &empty, 1));
  pair.pump(20);
  EXPECT_EQ(pair.appB.bindingCalls, 1);
  EXPECT_EQ(pair.appB.lastBindings, std::vector<uint8_t>{0});
  EXPECT_EQ(pair.appA.readCalls, 1U);
  EXPECT_EQ(pair.appB.stateCalls, 1U);
  EXPECT_FALSE(pair.runtimeB.readNeedsRefresh(pair.peerB, pair.resource));
  EXPECT_EQ(pair.runtimeA.diagnostics().sessionInitTx, 1U);
  EXPECT_EQ(pair.runtimeB.diagnostics().sessionInitTx, 1U);
  EXPECT_EQ(pair.runtimeA.diagnostics().sessionEstablished, 1U);
  EXPECT_EQ(pair.runtimeB.diagnostics().sessionEstablished, 1U);
  EXPECT_EQ(pair.runtimeA.diagnostics().sessionReplaced, 0U);
  EXPECT_EQ(pair.runtimeB.diagnostics().sessionReplaced, 0U);
  const auto lost = static_cast<unsigned>(DiagnosticEvent::SessionLost);
  EXPECT_EQ(pair.appA.diagnosticEvents[lost], 0U);
  EXPECT_EQ(pair.appB.diagnosticEvents[lost], 0U);
  EXPECT_EQ(pair.runtimeA.poolDiagnostics().sessions.used, 1U);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().sessions.used, 1U);
}

TEST(SupLanHandshake, LateLocateReplyAndDuplicateInitKeepResponderAttempt) {
  using namespace Supla::SupLan;  // NOLINT(build/namespaces)
  RuntimePair pair;
  pair.appA.bindingPoolEnabled = true;
  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
  pair.runtimeB.iterate();  // LOCATE remains outstanding at Destination.
  pair.peersA.get(pair.peerA)->endpoint = pair.endpointB;
  pair.peersA.get(pair.peerA)->endpointState = kPeerEndpointLocateCandidate;
  const uint8_t empty = 0;
  ASSERT_FALSE(pair.runtimeA.sendBindings(pair.peerA, &empty, 1));
  pair.runtimeA.iterate();  // Answer LOCATE while own INIT is outstanding.
  std::vector<Packet> replies;
  Packet init = {};
  for (size_t i = 0; i < pair.network.packets.size();) {
    const auto &packet = pair.network.packets[i];
    if (packet.bytes.size() >= 2 && packet.bytes[1] == kFrameLocateReply) {
      replies.push_back(packet);
      pair.network.packets.erase(pair.network.packets.begin() + i);
    } else {
      if (packet.bytes.size() == kSessionInitSize &&
          packet.bytes[1] == kFrameSessionInit) init = packet;
      ++i;
    }
  }
  ASSERT_EQ(replies.size(), 1U);
  ASSERT_EQ(init.bytes.size(), kSessionInitSize);
  pair.runtimeB.iterate();  // Become responder before LOCATE_REPLY arrives.
  ASSERT_FALSE(pair.runtimeB.hasSession(pair.peerB));
  ASSERT_EQ(pair.runtimeB.diagnostics().sessionAcceptTx, 1U);
  pair.network.packets.insert(pair.network.packets.end(),
                              replies.begin(), replies.end());
  pair.network.packets.push_back(init);  // Retransmission of the same INIT.
  pair.runtimeB.iterate();
  EXPECT_EQ(pair.runtimeB.diagnostics().locateReplyRx, 1U);
  EXPECT_EQ(pair.runtimeB.diagnostics().sessionInitTx, 0U);
  EXPECT_EQ(pair.runtimeB.diagnostics().sessionAcceptTx, 2U);
  std::vector<std::vector<uint8_t>> accepts;
  for (const auto &packet : pair.network.history)
    if (packet.bytes.size() == kSessionAcceptSize &&
        packet.bytes[1] == kFrameSessionAccept) accepts.push_back(packet.bytes);
  ASSERT_EQ(accepts.size(), 2U);
  EXPECT_EQ(accepts[0], accepts[1]);
  pair.runtimeA.iterate();
  ASSERT_TRUE(pair.runtimeA.sendBindings(pair.peerA, &empty, 1));
  pair.pump(20);
  EXPECT_EQ(pair.appB.stateCalls, 1U);
  EXPECT_EQ(pair.runtimeA.diagnostics().sessionReplaced, 0U);
  EXPECT_EQ(pair.runtimeB.diagnostics().sessionReplaced, 0U);
}

TEST(SupLanHandshake, SilentInitiatorExpiresResponderAndReadRecovers) {
  using namespace Supla::SupLan;  // NOLINT(build/namespaces)
  for (uint32_t start : {0U, UINT32_MAX - 1500U}) {
    RuntimePair pair;
    pair.network.now = start;
    startCompetingHandshakes(pair);
    ASSERT_EQ(pair.runtimeB.diagnostics().sessionInitTx, 1U);
    pair.runtimeA.iterate();  // ACCEPT received, but no protected data follows.
    ASSERT_TRUE(pair.runtimeA.hasSession(pair.peerA));
    pair.pumpMs(kPendingHandshakeTimeoutMs - 10);
    EXPECT_FALSE(pair.runtimeB.hasSession(pair.peerB));
    EXPECT_EQ(pair.runtimeB.diagnostics().sessionInitTx, 1U);
    EXPECT_EQ(pair.appA.readCalls, 0U);
    // Existing timeout/backoff, with advancing monotonic time.
    pair.pumpMs(10000);
    EXPECT_GT(pair.runtimeB.diagnostics().sessionInitTx, 1U);
    EXPECT_GE(pair.appA.readCalls, 1U);
    EXPECT_GE(pair.appB.stateCalls, 1U);
    EXPECT_TRUE(pair.runtimeB.hasSession(pair.peerB));
    EXPECT_FALSE(pair.runtimeB.readNeedsRefresh(pair.peerB, pair.resource));
  }
}

TEST(SupLanHandshake, LostFirstBindingsFrameConfirmsSessionOnBoundedRetry) {
  using namespace Supla::SupLan;  // NOLINT(build/namespaces)
  RuntimePair pair;
  startCompetingHandshakes(pair);
  ASSERT_EQ(pair.runtimeB.diagnostics().sessionInitTx, 1U);
  pair.runtimeA.iterate();
  pair.network.dropNextFullData = true;
  const uint8_t empty = 0;
  ASSERT_TRUE(pair.runtimeA.sendBindings(pair.peerA, &empty, 1));
  pair.pump(1);
  EXPECT_FALSE(pair.runtimeB.hasSession(pair.peerB));
  EXPECT_EQ(pair.appB.bindingCalls, 0);
  pair.pumpMs(kAckRetryMs + 200);
  EXPECT_EQ(pair.appB.bindingCalls, 1);
  EXPECT_EQ(pair.appA.readCalls, 1U);
  EXPECT_EQ(pair.appB.stateCalls, 1U);
  EXPECT_EQ(pair.runtimeB.diagnostics().sessionInitTx, 1U);
  EXPECT_EQ(pair.runtimeA.diagnostics().sessionReplaced, 0U);
  EXPECT_EQ(pair.runtimeB.diagnostics().sessionReplaced, 0U);
  EXPECT_FALSE(pair.runtimeA.bindingsInFlight());
}

TEST(SupLanHandshake, SourceAlsoWaitsForItsExistingResponderAttempt) {
  using namespace Supla::SupLan;  // NOLINT(build/namespaces)
  RuntimePair pair;
  pair.appA.bindingPoolEnabled = true;
  pair.peersB.get(pair.peerB)->endpoint = pair.endpointA;
  pair.peersB.get(pair.peerB)->endpointState = kPeerEndpointLocateCandidate;
  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
  pair.runtimeB.iterate();
  pair.runtimeA.iterate();  // Source responds to Destination's first INIT.
  ASSERT_EQ(pair.runtimeA.diagnostics().sessionAcceptTx, 1U);
  ASSERT_FALSE(pair.runtimeA.hasSession(pair.peerA));
  const uint8_t empty = 0;
  ASSERT_FALSE(pair.runtimeA.sendBindings(pair.peerA, &empty, 1));
  EXPECT_EQ(pair.runtimeA.diagnostics().sessionInitTx, 0U);
  EXPECT_EQ(pair.runtimeA.diagnostics().locateTx, 0U);
  pair.pump(20);  // Destination's READ confirms this same session.
  ASSERT_EQ(pair.appB.stateCalls, 1U);
  ASSERT_TRUE(pair.runtimeA.sendBindings(pair.peerA, &empty, 1));
  pair.pump(20);
  EXPECT_EQ(pair.appB.bindingCalls, 1);
  EXPECT_EQ(pair.runtimeA.diagnostics().sessionInitTx, 0U);
  EXPECT_EQ(pair.runtimeB.diagnostics().sessionInitTx, 1U);
  EXPECT_EQ(pair.runtimeA.diagnostics().sessionReplaced, 0U);
  EXPECT_EQ(pair.runtimeB.diagnostics().sessionReplaced, 0U);
}

TEST(SupLanHandshake, ForgetSessionCancelsRecoveryOfDiscardedAction) {
  RuntimePair pair;
  const ResourceId resource = {Supla::SupLan::kResourceTypeChannel, 50002};
  ASSERT_TRUE(pair.runtimeA.requestRead(pair.actionPeerA, resource));
  pair.pump(20);
  uint8_t payload[15] = {0xff, 0xa5};
  pair.runtimeA.testHooks()->dropNextAckTx = 1;
  ASSERT_TRUE(pair.runtimeB.publishAction(pair.actionPeerB, resource,
                                         payload, sizeof(payload)));
  pair.pump(2);
  ASSERT_EQ(pair.appA.actionCalls, 1U);
  ASSERT_EQ(pair.runtimeB.poolDiagnostics().retries.used, 1U);
  ASSERT_TRUE(pair.runtimeB.forgetSession(pair.actionPeerB));
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().retries.used, 0U);
  EXPECT_FALSE(pair.runtimeB.recoveryStatus(pair.actionPeerB).active);
  const auto initTx = pair.runtimeB.diagnostics().sessionInitTx;
  pair.pump(30);
  EXPECT_EQ(pair.runtimeB.diagnostics().sessionInitTx, initTx);
  EXPECT_EQ(pair.appA.actionCalls, 1U);
}

TEST(SupLanHandshake, ForgetSessionPreservesRequiredBindingRecovery) {
  using namespace Supla::SupLan;  // NOLINT(build/namespaces)
  RuntimePair pair;
  pair.appA.bindingPoolEnabled = true;
  pair.peersA.get(pair.peerA)->endpoint = pair.endpointB;
  pair.peersA.get(pair.peerA)->endpointState = kPeerEndpointLocateCandidate;
  const uint8_t empty = 0;
  ASSERT_FALSE(pair.runtimeA.sendBindings(pair.peerA, &empty, 1));
  ASSERT_TRUE(pair.runtimeA.recoveryStatus(pair.peerA).active);
  pair.network.packets.clear();  // The first INIT never reaches Destination.
  ASSERT_TRUE(pair.runtimeA.forgetSession(pair.peerA));
  EXPECT_TRUE(pair.runtimeA.recoveryStatus(pair.peerA).active);
  pair.runtimeA.iterate();
  EXPECT_EQ(pair.runtimeA.diagnostics().sessionInitTx, 2U);
  pair.pump(20);
  ASSERT_TRUE(pair.runtimeA.sendBindings(pair.peerA, &empty, 1));
  pair.pump(20);
  EXPECT_EQ(pair.appB.bindingCalls, 1);
  EXPECT_TRUE(pair.runtimeA.hasSession(pair.peerA));
  EXPECT_TRUE(pair.runtimeB.hasSession(pair.peerB));
}

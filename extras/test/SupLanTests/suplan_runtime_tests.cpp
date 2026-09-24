// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright 2026 AC SOFTWARE SP. Z O.O.

#include <suplan/suplan_runtime.h>
#include <suplan/suplan_crypto.h>

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <vector>

#include "suplan_crypto_openssl.h"

static_assert(sizeof(Supla::SupLan::Runtime) +
                  sizeof(Supla::SupLan::PeerTable) <= 6 * 1024,
              "SupLAN core workspace exceeds its 6 KiB target");

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

  FakeNetwork() : packets(), history(), now(0) {}

  bool send(const Endpoint &source, const Endpoint &destination,
            bool multicast, const uint8_t *data, size_t length) {
    if (data == nullptr || length == 0 ||
        length > Supla::SupLan::kMaxDatagramPayload) {
      return false;
    }
    Packet packet = {};
    packet.source = source;
    packet.destination = destination;
    packet.multicast = multicast;
    packet.bytes.assign(data, data + length);
    packets.push_back(packet);
    history.push_back(packet);
    return true;
  }
};

class FakeDatagramPort : public Supla::SupLan::DatagramPort {
 public:
  FakeDatagramPort(FakeNetwork *network, const Endpoint &self)
      : network_(network), self_(self) {}

  bool sendUnicast(const Endpoint &endpoint, const uint8_t *data,
                   size_t length) override {
    return network_->send(self_, endpoint, false, data, length);
  }

  bool sendLocateMulticast(const uint8_t *data, size_t length) override {
    Endpoint group = {0x06C9FFEF, 2016};
    return network_->send(self_, group, true, data, length);
  }

  int pollReceive(uint8_t *buffer, size_t capacity,
                  Endpoint *endpoint) override {
    for (size_t i = 0; i < network_->packets.size(); ++i) {
      const Packet &packet = network_->packets[i];
      const bool matches = packet.multicast ||
          (packet.destination.address == self_.address &&
           packet.destination.port == self_.port);
      if (!matches || packet.source.address == self_.address ||
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

 private:
  FakeNetwork *network_;
  Endpoint self_;
};

class FakeRandomPort : public Supla::SupLan::RandomPort {
 public:
  explicit FakeRandomPort(uint8_t seed) : next_(seed) {}

  bool fillRandom(uint8_t *buffer, size_t length) override {
    for (size_t i = 0; i < length; ++i) {
      next_ = static_cast<uint8_t>(next_ + 29);
      buffer[i] = next_;
    }
    return true;
  }

 private:
  uint8_t next_;
};

class FakeApplication : public Supla::SupLan::ApplicationPort {
 public:
  FakeApplication() : value(0), controlCalls(0), stateCalls(0), actionCalls(0),
      acknowledged(0), lastAckResult(0), lastStateType(0), lastStateLength(0),
      runtime(nullptr) {}

  bool readResource(const ResourceId &resource, bool *eventOnly,
                    uint8_t *payload,
                    size_t capacity, size_t *payloadLength) override {
    if (resource.id == 50002) {
      *eventOnly = true;
      *payloadLength = 0;
      return false;
    }
    if (capacity < 14) {
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

  void receiveState(uint8_t, const ResourceId &, uint32_t messageType,
                    const uint8_t *payload, size_t payloadLength) override {
    ++stateCalls;
    lastStateType = messageType;
    lastStateLength = payloadLength;
    if (payloadLength == 14) {
      value = payload[6];
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
  Supla::SupLan::OpenSslCryptoPort crypto;
  FakeRandomPort randomA;
  FakeRandomPort randomB;
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

  RuntimePair()
      : crypto(), randomA(5), randomB(173), network(),
        endpointA({0x0100007F, 2016}), endpointB({0x0200007F, 2016}),
        datagramsA(&network, endpointA), datagramsB(&network, endpointB),
        appA(), appB(), peersA(), peersB(),
        runtimeA(&crypto, &randomA, &datagramsA, &appA, &peersA,
                 nodeAddress(1001), 27),
        runtimeB(&crypto, &randomB, &datagramsB, &appB, &peersB,
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
        &crypto, &context, rootKey, &material));
    EXPECT_TRUE(peersA.addPeerFromRoot(&crypto, &context, rootKey, 1, &acl, 1,
                                       &peerA));
    EXPECT_TRUE(peersB.addPeer(&crypto, &context, material.peerKey, 1, &acl, 1,
                               &peerB));
    const uint8_t reversePeerKey[32] = {
        0x70, 0xD1, 0x3F, 0x17, 0x8A, 0x1F, 0x93, 0xE3,
        0x4E, 0x0C, 0x8A, 0x0B, 0x84, 0x4C, 0x4E, 0xAB,
        0x04, 0xEC, 0xF8, 0x62, 0x90, 0x42, 0xD9, 0xC0,
        0xFA, 0x42, 0xB9, 0xC1, 0xC4, 0x62, 0x47, 0xCC};
    Supla::SupLan::AclEntry actionAcl = {};
    actionAcl.resource.type = Supla::SupLan::kResourceTypeChannel;
    actionAcl.resource.id = 50002;
    actionAcl.permissions = Supla::SupLan::kPermissionAction;
    EXPECT_TRUE(peersA.addPeer(&crypto, &actionContext, reversePeerKey, 1,
                               &actionAcl, 1, &actionPeerA));
    EXPECT_TRUE(peersB.addPeerFromRoot(&crypto, &actionContext, rootKey, 1,
                                       &actionAcl, 1, &actionPeerB));
    appA.runtime = &runtimeA;
  }

  void pump(uint8_t count) {
    for (uint8_t i = 0; i < count; ++i) {
      network.now += 10;
      runtimeA.iterate();
      runtimeB.iterate();
    }
  }
};

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
  EXPECT_EQ(pair.runtimeA.diagnostics().ackTx, 2U);
  EXPECT_EQ(pair.runtimeB.diagnostics().retryTx, 1U);
  EXPECT_EQ(pair.appB.acknowledged, 1U);
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

  EXPECT_EQ(pair.runtimeA.diagnostics().dataAuthFail, 1U);
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
  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
  pair.pump(10);

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

TEST(SupLanRuntime, ActionOnlyReadRefreshesInterestAndEventsAreBestEffort) {
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
  ASSERT_TRUE(pair.runtimeB.publishAction(pair.actionPeerB, actionResource,
                                           payload, sizeof(payload)));
  pair.pump(5);
  EXPECT_EQ(pair.appA.actionCalls, 1U);
  EXPECT_EQ(pair.appA.lastActionId, 0x1234U);
  EXPECT_EQ(pair.runtimeB.diagnostics().actionTx, 1U);
  EXPECT_EQ(pair.runtimeA.diagnostics().actionRx, 1U);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().retries.used, 0U);

  pair.runtimeA.testHooks()->dropNextDataRx = 1;
  payload[1] = 0x78;
  payload[2] = 0x56;
  ASSERT_TRUE(pair.runtimeB.publishAction(pair.actionPeerB, actionResource,
                                           payload, sizeof(payload)));
  pair.pump(5);
  EXPECT_EQ(pair.appA.actionCalls, 1U);
  EXPECT_EQ(pair.appA.lastActionId, 0x1234U);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().deferredEvents.used, 0U);
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

TEST(SupLanRuntime, LostRemoteSessionIsRecoveredAfterRetryExhaustion) {
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
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().sessions.used, 0U);

  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
  pair.pump(30);
  EXPECT_EQ(pair.runtimeA.diagnostics().readDispatched, 2U);
  EXPECT_EQ(pair.runtimeA.poolDiagnostics().sessions.used, 1U);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().sessions.used, 1U);
  EXPECT_EQ(pair.appB.stateCalls, 2U);
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
  EXPECT_EQ(pair.appB.acknowledged, 3U);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().retries.used, 0U);
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().deferredEvents.used, 0U);
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

}  // namespace

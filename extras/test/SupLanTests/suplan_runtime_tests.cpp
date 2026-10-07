// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright 2026 AC SOFTWARE SP. Z O.O.

#include <suplan/suplan_runtime.h>
#include <suplan/suplan_crypto.h>

#include <gtest/gtest.h>
#include <crypto_test_hooks.h>

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

  bool stateAvailable = true;
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
    EXPECT_TRUE(peersA.addPeer(&actionContext, reversePeerKey, 1,
                               &actionAcl, 1, &actionPeerA));
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
  EXPECT_EQ(pair.runtimeB.diagnostics().locateTx, 0U);
  EXPECT_TRUE(pair.network.packets.empty());
}

TEST(SupLanRuntime, LocateKdfRunsOnceAndNotAgainWhileLocateIsOutstanding) {
  RuntimePair pair;

  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resource));
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

TEST(SupLanRuntime, LostRemoteSessionRetriesControlAfterNewHandshake) {
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
  EXPECT_EQ(pair.runtimeB.diagnostics().sessionInitTx, 2U);
  EXPECT_EQ(pair.runtimeB.diagnostics().sessionEstablished, 2U);
  EXPECT_EQ(pair.runtimeA.diagnostics().sessionEstablished, 2U);
  EXPECT_EQ(pair.appA.controlCalls, 1U);
  EXPECT_EQ(pair.appB.acknowledged, 2U);
  EXPECT_EQ(pair.runtimeA.diagnostics().readDispatched, 1U);
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
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().deferredEvents.used, 2U);
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
    EXPECT_EQ(established ? pair.runtimeB.poolDiagnostics().retries.used
                          : pair.runtimeB.poolDiagnostics().deferredEvents.used,
              1U);
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
  EXPECT_EQ(pair.runtimeB.poolDiagnostics().deferredEvents.used, 1U);
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
                 : Supla::SupLan::kPermissionAction};
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
      {resource, Supla::SupLan::kPermissionAction},
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
                             Supla::SupLan::kPermissionAction}) {
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

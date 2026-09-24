// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright 2026 AC SOFTWARE SP. Z O.O.

#include <supla/control/virtual_relay.h>
#include <supla/protocol/suplan_protocol.h>
#include <suplan/suplan_crypto.h>

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <cstring>

#include "suplan_crypto_openssl.h"

namespace {

using Supla::SupLan::Endpoint;
using Supla::SupLan::NodeAddress;
using Supla::SupLan::ResourceId;

struct FanoutPacket {
  Endpoint source;
  Endpoint destination;
  bool multicast;
  std::array<uint8_t, Supla::SupLan::kMaxDatagramPayload> bytes;
  size_t length;
};

struct FanoutNetwork {
  FanoutPacket packets[128];
  size_t count;
  uint32_t now;

  FanoutNetwork() : packets(), count(0), now(0) {}

  bool send(const Endpoint &source, const Endpoint &destination, bool multicast,
            const uint8_t *data, size_t length) {
    if (data == nullptr || length == 0 ||
        length > Supla::SupLan::kMaxDatagramPayload || count >= 128) {
      return false;
    }
    FanoutPacket &packet = packets[count++];
    packet.source = source;
    packet.destination = destination;
    packet.multicast = multicast;
    packet.length = length;
    std::memcpy(packet.bytes.data(), data, length);
    return true;
  }
};

class FanoutDatagramPort : public Supla::SupLan::DatagramPort {
 public:
  FanoutDatagramPort(FanoutNetwork *network, const Endpoint &self)
      : network_(network), self_(self) {}

  bool sendUnicast(const Endpoint &endpoint, const uint8_t *data,
                  size_t length) override {
    return network_->send(self_, endpoint, false, data, length);
  }

  bool sendLocateMulticast(const uint8_t *data, size_t length) override {
    const Endpoint group = {0x06C9FFEF, 2016};
    return network_->send(self_, group, true, data, length);
  }

  int pollReceive(uint8_t *buffer, size_t capacity,
                  Endpoint *endpoint) override {
    for (size_t i = 0; i < network_->count; ++i) {
      const FanoutPacket &packet = network_->packets[i];
      const bool matches = packet.multicast ||
          (packet.destination.address == self_.address &&
           packet.destination.port == self_.port);
      if (!matches || packet.source.address == self_.address ||
          packet.length > capacity) {
        continue;
      }
      std::memcpy(buffer, packet.bytes.data(), packet.length);
      *endpoint = packet.source;
      const int length = static_cast<int>(packet.length);
      for (size_t j = i + 1; j < network_->count; ++j) {
        network_->packets[j - 1] = network_->packets[j];
      }
      --network_->count;
      return length;
    }
    return 0;
  }

  size_t maxDatagramPayload() const override {
    return Supla::SupLan::kMaxDatagramPayload;
  }

  uint32_t nowMs() const override { return network_->now; }

 private:
  FanoutNetwork *network_;
  Endpoint self_;
};

class FanoutRandomPort : public Supla::SupLan::RandomPort {
 public:
  explicit FanoutRandomPort(uint8_t seed) : next_(seed) {}

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

class FanoutApplication : public Supla::SupLan::ApplicationPort {
 public:
  FanoutApplication() : stateCalls(0), actionCalls(0) {}

  bool readResource(const ResourceId &, bool *eventOnly, uint8_t *payload,
                    size_t capacity, size_t *payloadLength) override {
    if (eventOnly == nullptr || payload == nullptr ||
        payloadLength == nullptr || capacity < 14) {
      return false;
    }
    *eventOnly = false;
    std::memset(payload, 0, 14);
    payload[0] = Supla::SupLan::kChannelNumberUnresolved;
    *payloadLength = 14;
    return true;
  }

  uint8_t dispatchControl(const ResourceId &, uint32_t, const uint8_t *,
                          size_t) override {
    return 0;
  }

  void receiveState(uint8_t, const ResourceId &, uint32_t, const uint8_t *,
                    size_t) override {
    ++stateCalls;
  }

  void receiveAction(uint8_t, const ResourceId &, uint32_t, const uint8_t *,
                     size_t) override {
    ++actionCalls;
  }

  void operationAcknowledged(uint8_t, const ResourceId &, uint32_t,
                             uint8_t) override {}

  uint32_t stateCalls;
  uint32_t actionCalls;
};

class AdapterFanoutPair {
 public:
  Supla::SupLan::OpenSslCryptoPort crypto;
  FanoutRandomPort randomA;
  FanoutRandomPort randomB;
  FanoutRandomPort randomC;
  FanoutNetwork network;
  Endpoint endpointA;
  Endpoint endpointB;
  Endpoint endpointC;
  FanoutDatagramPort datagramsA;
  FanoutDatagramPort datagramsB;
  FanoutDatagramPort datagramsC;
  FanoutApplication appB;
  FanoutApplication appC;
  Supla::SupLan::PeerTable peersA;
  Supla::SupLan::PeerTable peersB;
  Supla::SupLan::PeerTable peersC;
  Supla::Control::VirtualRelay relay;
  Supla::Protocol::SupLanResourceMapping mappings[2];
  Supla::Protocol::SupLan protocolA;
  Supla::SupLan::Runtime runtimeA;
  Supla::SupLan::Runtime runtimeB;
  Supla::SupLan::Runtime runtimeC;
  ResourceId resourceB;
  ResourceId resourceC;
  uint8_t peerAForB;
  uint8_t peerAForC;
  uint8_t peerB;
  uint8_t peerC;
  bool configured;

  AdapterFanoutPair()
      : crypto(), randomA(3), randomB(71), randomC(139), network(),
        endpointA({0x0100007F, 2016}), endpointB({0x0200007F, 2016}),
        endpointC({0x0300007F, 2016}),
        datagramsA(&network, endpointA), datagramsB(&network, endpointB),
        datagramsC(&network, endpointC), appB(), appC(), peersA(), peersB(),
        peersC(), relay(), mappings(),
        protocolA(nullptr, &peersA, mappings, 2),
        runtimeA(&crypto, &randomA, &datagramsA, &protocolA, &peersA,
                 nodeAddress(1001), 27),
        runtimeB(&crypto, &randomB, &datagramsB, &appB, &peersB,
                 nodeAddress(1002), 27),
        runtimeC(&crypto, &randomC, &datagramsC, &appC, &peersC,
                 nodeAddress(1003), 27),
        resourceB({Supla::SupLan::kResourceTypeChannel, 50001}),
        resourceC({Supla::SupLan::kResourceTypeChannel, 50003}),
        peerAForB(0), peerAForC(0), peerB(0), peerC(0), configured(false) {
    uint8_t rootKey[32];
    for (uint8_t i = 0; i < sizeof(rootKey); ++i) rootKey[i] = i;
    configured = addRelation(1002, resourceB, &peersB, &peerAForB, &peerB,
                             rootKey) &&
        addRelation(1003, resourceC, &peersC, &peerAForC, &peerC, rootKey);
    const uint8_t channelNumber =
        static_cast<uint8_t>(relay.getChannelNumber());
    mappings[0] = {resourceB.id, channelNumber, peerAForB, false};
    mappings[1] = {resourceC.id, channelNumber, peerAForC, false};
    protocolA.attachRuntime(&runtimeA);
  }

  static NodeAddress nodeAddress(uint32_t nodeId) {
    return {Supla::SupLan::kNodeIdDevice, nodeId};
  }

  bool addRelation(uint32_t destinationId, const ResourceId &resource,
                   Supla::SupLan::PeerTable *destinationPeers,
                   uint8_t *sourcePeer, uint8_t *destinationPeer,
                   const uint8_t rootKey[32]) {
    Supla::SupLan::PeerContext context = {};
    context.authorityType = Supla::SupLan::kAuthorityServer;
    context.source = nodeAddress(1001);
    context.destination = nodeAddress(destinationId);
    context.rootEpoch = 1;
    context.peerGeneration = 1;
    Supla::SupLan::PeerMaterial material = {};
    if (!Supla::SupLan::derivePeerMaterial(&crypto, &context, rootKey,
                                           &material)) {
      return false;
    }
    Supla::SupLan::AclEntry acl = {};
    acl.resource = resource;
    acl.permissions = Supla::SupLan::kPermissionRead |
        Supla::SupLan::kPermissionAction;
    return peersA.addPeerFromRoot(&crypto, &context, rootKey, 1, &acl, 1,
                                  sourcePeer) &&
        destinationPeers->addPeer(&crypto, &context, material.peerKey, 1,
                                  &acl, 1, destinationPeer);
  }

  void pump(uint8_t count) {
    for (uint8_t i = 0; i < count; ++i) {
      network.now += 10;
      runtimeA.iterate();
      runtimeB.iterate();
      runtimeC.iterate();
    }
  }
};

TEST(SupLanAdapter, FansOutStateExtendedStateAndActionToMappedPeers) {
  AdapterFanoutPair pair;
  ASSERT_TRUE(pair.configured);
  ASSERT_TRUE(pair.protocolA.verifyConfig());

  ASSERT_TRUE(pair.runtimeB.requestRead(pair.peerB, pair.resourceB));
  pair.pump(24);
  ASSERT_EQ(pair.runtimeA.poolDiagnostics().interests.used, 1U);
  ASSERT_TRUE(pair.runtimeC.requestRead(pair.peerC, pair.resourceC));
  pair.pump(24);
  ASSERT_EQ(pair.runtimeA.poolDiagnostics().interests.used, 2U);

  int8_t rawValue[SUPLA_CHANNELVALUE_SIZE] = {};
  rawValue[0] = 1;
  const uint32_t txBeforeState =
      pair.runtimeA.diagnostics().stateNotificationTx;
  const uint32_t bStateBefore = pair.appB.stateCalls;
  const uint32_t cStateBefore = pair.appC.stateCalls;
  pair.protocolA.sendChannelValueChanged(
      static_cast<uint8_t>(pair.relay.getChannelNumber()), rawValue, 0, 60);
  pair.pump(8);
  EXPECT_EQ(pair.runtimeA.diagnostics().stateNotificationTx,
            txBeforeState + 2);
  EXPECT_EQ(pair.appB.stateCalls, bStateBefore + 1);
  EXPECT_EQ(pair.appC.stateCalls, cStateBefore + 1);

  TSuplaChannelExtendedValue extended = {};
  extended.size = 1;
  extended.value[0] = 0x2A;
  const uint32_t bStateBeforeExtended = pair.appB.stateCalls;
  const uint32_t cStateBeforeExtended = pair.appC.stateCalls;
  pair.protocolA.sendExtendedChannelValueChanged(
      static_cast<uint8_t>(pair.relay.getChannelNumber()), &extended);
  pair.pump(8);
  EXPECT_EQ(pair.appB.stateCalls, bStateBeforeExtended + 1);
  EXPECT_EQ(pair.appC.stateCalls, cStateBeforeExtended + 1);

  pair.protocolA.sendActionTrigger(
      static_cast<uint8_t>(pair.relay.getChannelNumber()), 0x1234);
  pair.pump(8);
  EXPECT_EQ(pair.appB.actionCalls, 1U);
  EXPECT_EQ(pair.appC.actionCalls, 1U);
}

}  // namespace

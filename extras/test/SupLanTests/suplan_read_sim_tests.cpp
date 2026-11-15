// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright 2026 AC SOFTWARE SP. Z O.O.

// Deterministic multi-node READ simulation: one Destination reads M Channels
// from each of N Sources. Every node has its own loop period on a shared fake
// clock, LOCATE multicast reaches every node, and loss uses a fixed seed.
// The scenarios need more peers than the constrained profile holds.

#include <suplan/suplan_config.h>

#if SUPLAN_PROFILE == SUPLAN_PROFILE_LARGE

#include <suplan/suplan_crypto.h>
#include <suplan/suplan_runtime.h>

#include <gtest/gtest.h>
#include <crypto_test_hooks.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <memory>
#include <string>
#include <vector>

#include "suplan_crypto_openssl.h"

namespace {

using Supla::SupLan::DiagnosticEvent;
using Supla::SupLan::Endpoint;
using Supla::SupLan::ResourceId;

constexpr uint32_t kSimBaseAddress = 0x0A000001;
constexpr uint32_t kSimDestinationNode = 1002;
constexpr uint32_t kSimFirstSourceNode = 2000;

struct SimPacket {
  uint32_t deliverAtMs;
  Endpoint source;
  std::vector<uint8_t> bytes;
};

struct SimNetwork {
  uint32_t now = 0;
  uint32_t datagrams = 0;
  uint32_t lost = 0;
  uint32_t lossPermille = 0;
  // One-way delay: latencyMs plus up to the same amount of seeded jitter.
  uint32_t latencyMs = 0;
  uint32_t random = 0x2545F491;
  std::vector<std::deque<SimPacket>> inbox;

  uint32_t nextRandom() {
    random = random * 1664525U + 1013904223U;
    return random >> 16;
  }
  bool lose() {
    if (!lossPermille) return false;
    if (nextRandom() % 1000 >= lossPermille) return false;
    ++lost;
    return true;
  }
  void deliver(size_t from, size_t to, const uint8_t *data, size_t length) {
    if (to >= inbox.size() || lose()) return;
    SimPacket packet;
    packet.deliverAtMs = now +
        (latencyMs ? latencyMs + nextRandom() % (latencyMs + 1) : 0);
    packet.source = {kSimBaseAddress + static_cast<uint32_t>(from), 2016};
    packet.bytes.assign(data, data + length);
    inbox[to].push_back(packet);
  }
};

class SimPort : public Supla::SupLan::DatagramPort {
 public:
  SimPort(SimNetwork *network, size_t self) : network_(network), self_(self) {}
  bool sendUnicast(const Endpoint &endpoint, const uint8_t *data,
                   size_t length) override {
    ++network_->datagrams;
    network_->deliver(self_, endpoint.address - kSimBaseAddress, data, length);
    return true;
  }
  bool sendLocateMulticast(const uint8_t *data, size_t length) override {
    ++network_->datagrams;
    for (size_t node = 0; node < network_->inbox.size(); ++node)
      if (node != self_) network_->deliver(self_, node, data, length);
    return true;
  }
  int pollReceive(uint8_t *buffer, size_t capacity,
                  Endpoint *endpoint) override {
    auto &inbox = network_->inbox[self_];
    for (auto it = inbox.begin(); it != inbox.end(); ++it) {
      if (static_cast<int32_t>(network_->now - it->deliverAtMs) < 0) continue;
      const SimPacket packet = *it;
      inbox.erase(it);
      if (packet.bytes.size() > capacity) return 0;
      std::memcpy(buffer, packet.bytes.data(), packet.bytes.size());
      *endpoint = packet.source;
      return static_cast<int>(packet.bytes.size());
    }
    return 0;
  }
  size_t maxDatagramPayload() const override {
    return Supla::SupLan::kMaxDatagramPayload;
  }
  uint32_t nowMs() const override { return network_->now; }

 private:
  SimNetwork *network_;
  size_t self_;
};

class SimApplication : public Supla::SupLan::ApplicationPort {
 public:
  void diagnostic(DiagnosticEvent event, uint8_t, const ResourceId &,
                  uint32_t) override {
    ++events[static_cast<unsigned>(event)];
  }
  void capacityLimited(
      const Supla::SupLan::CapacityDiagnostic &event) override {
    ++(event.outcome == Supla::SupLan::CapacityDiagnostic::Outcome::Rejected
           ? capacityRejected : capacityDeferred);
  }
  bool readResource(const ResourceId &, bool *eventOnly, uint8_t *payload,
                    size_t capacity, size_t *payloadLength) override {
    *eventOnly = false;
    if (capacity < 14) return false;
    std::memset(payload, 0, 14);
    payload[0] = 0xFF;
    payload[5] = 60;
    *payloadLength = 14;
    return true;
  }
  uint8_t dispatchControl(const ResourceId &, uint32_t, const uint8_t *,
                          size_t) override { return 3; }
  void receiveState(uint8_t, const ResourceId &resource, uint32_t,
                    const uint8_t *, size_t) override {
    const size_t index = resource.id - firstResource;
    if (index < states.size()) ++states[index];
  }
  void receiveAction(uint8_t, const ResourceId &, uint32_t, const uint8_t *,
                     size_t) override {}
  void operationAcknowledged(uint8_t, const ResourceId &, uint32_t,
                             uint8_t) override {}

  uint32_t count(DiagnosticEvent event) const {
    return events[static_cast<unsigned>(event)];
  }
  uint32_t events[40] = {};
  uint32_t capacityRejected = 0;
  uint32_t capacityDeferred = 0;
  uint32_t firstResource = 0;
  std::vector<uint32_t> states;
};

struct SimNode {
  SimNode(Supla::SupLan::CryptoPort *crypto, SimNetwork *network, size_t index,
          uint32_t nodeId)
      : port(network, index),
        runtime(crypto, &port, &application, &peers,
                {Supla::SupLan::kNodeIdDevice, nodeId}, 27) {}
  SimPort port;
  SimApplication application;
  Supla::SupLan::PeerTable peers;
  Supla::SupLan::Runtime runtime;
};

struct SimResult {
  bool complete = false;
  uint32_t elapsedMs = 0;
  uint32_t destinationLoops = 0;
  uint32_t datagrams = 0;
  uint32_t recoveryFailed = 0;
  uint32_t sessionLost = 0;
  uint32_t capacityRejected = 0;
  uint32_t capacityDeferred = 0;
  uint32_t retryTx = 0;
};

class ReadSimulation {
 public:
  ReadSimulation(uint8_t sources, uint8_t channels, uint32_t lossPermille = 0)
      : sources_(sources), channels_(channels) {
    network_.inbox.resize(sources + 1U);
    nodes_.emplace_back(new SimNode(&crypto_, &network_, 0,
                                    kSimDestinationNode));
    auto &destination = *nodes_[0];
    destination.application.firstResource = kFirstResource;
    destination.application.states.assign(sources * channels, 0);
    destinationPeers_.resize(sources);
    for (uint8_t source = 0; source < sources; ++source) {
      nodes_.emplace_back(new SimNode(&crypto_, &network_, source + 1U,
                                      kSimFirstSourceNode + source));
      Supla::SupLan::PeerContext context = {};
      context.authorityType = Supla::SupLan::kAuthorityServer;
      context.source = {Supla::SupLan::kNodeIdDevice,
                        kSimFirstSourceNode + source};
      context.destination = {Supla::SupLan::kNodeIdDevice,
                             kSimDestinationNode};
      context.rootEpoch = 1;
      context.peerGeneration = 1;
      uint8_t rootKey[32];
      for (uint8_t i = 0; i < sizeof(rootKey); ++i)
        rootKey[i] = static_cast<uint8_t>(source * 7 + i);
      std::vector<Supla::SupLan::AclEntry> acl(channels);
      for (uint8_t channel = 0; channel < channels; ++channel) {
        acl[channel].resource = resource(source, channel);
        acl[channel].permissions = Supla::SupLan::kPermissionRead;
      }
      Supla::SupLan::PeerMaterial material = {};
      uint8_t sourcePeer = 0;
      EXPECT_TRUE(Supla::SupLan::derivePeerMaterial(
          &context, rootKey, &material));
      EXPECT_TRUE(nodes_.back()->peers.addPeerFromRoot(
          &context, rootKey, 1, acl.data(), channels, &sourcePeer));
      EXPECT_TRUE(destination.peers.addPeer(
          &context, material.peerKey, 1, acl.data(), channels,
          &destinationPeers_[source], true));
    }
    network_.lossPermille = lossPermille;
  }

  // The Destination loop stops for this long once every 2.5 s.
  uint32_t stallMs = 0;
  void setLatency(uint32_t latencyMs) { network_.latencyMs = latencyMs; }

  ResourceId resource(uint8_t source, uint8_t channel) const {
    return {Supla::SupLan::kResourceTypeChannel,
            kFirstResource + source * channels_ + channel};
  }

  // Lets retransmissions of the previous phase finish. A late ACK of an older
  // READ would otherwise satisfy fresh demand without delivering a new state.
  void settle(uint32_t destinationPeriodMs, uint32_t sourcePeriodMs) {
    for (uint32_t elapsed = 0; elapsed < 2000 ||
         nodes_[0]->runtime.poolDiagnostics().retries.used; ++elapsed) {
      ASSERT_LT(elapsed, 60000U);
      step(destinationPeriodMs, sourcePeriodMs);
    }
  }

  // Demands every Channel and runs until each one delivered a new state.
  SimResult readAll(uint32_t destinationPeriodMs, uint32_t sourcePeriodMs,
                    uint32_t timeoutMs) {
    auto &destination = *nodes_[0];
    auto &application = destination.application;
    const std::vector<uint32_t> before = application.states;
    const uint32_t failedBefore =
        application.count(DiagnosticEvent::RecoveryFailed);
    const uint32_t lostBefore = application.count(DiagnosticEvent::SessionLost);
    const uint32_t rejectedBefore = application.capacityRejected;
    const uint32_t deferredBefore = application.capacityDeferred;
    const uint32_t retryBefore = destination.runtime.diagnostics().retryTx;
    const uint32_t datagramsBefore = network_.datagrams;
    bool admitted = true;
    for (uint8_t source = 0; source < sources_; ++source)
      for (uint8_t channel = 0; channel < channels_; ++channel)
        admitted = destination.runtime.requestRead(
            destinationPeers_[source], resource(source, channel)) && admitted;
    EXPECT_TRUE(admitted);
    SimResult result;
    const uint32_t start = network_.now;
    while (!result.complete && network_.now - start < timeoutMs) {
      if (!step(destinationPeriodMs, sourcePeriodMs)) continue;
      ++result.destinationLoops;
      result.complete = true;
      for (size_t i = 0; result.complete && i < before.size(); ++i)
        result.complete = application.states[i] != before[i];
    }
    if (!result.complete) {
      for (uint8_t source = 0; source < sources_; ++source) {
        const auto summary = destination.runtime.readRecoverySummary(
            destinationPeers_[source]);
        const auto status = destination.runtime.recoveryStatus(
            destinationPeers_[source]);
        uint32_t missing = 0;
        for (uint8_t channel = 0; channel < channels_; ++channel)
          missing += application.states[source * channels_ + channel] ==
              before[source * channels_ + channel];
        if (missing || summary.waiting)
          std::printf("[ READSIM  ] stuck source=%u missing=%u waiting=%u"
                      " notQueued=%u queued=%u ack=%u state=%u active=%d"
                      " session=%d stage=%u\n", source, missing,
                      summary.waiting, summary.notQueued, summary.queued,
                      summary.awaitingAck, summary.awaitingState,
                      status.active,
                      destination.runtime.hasSession(destinationPeers_[source]),
                      status.backgroundStage);
      }
    }
    result.elapsedMs = network_.now - start;
    result.datagrams = network_.datagrams - datagramsBefore;
    result.recoveryFailed =
        application.count(DiagnosticEvent::RecoveryFailed) - failedBefore;
    result.sessionLost =
        application.count(DiagnosticEvent::SessionLost) - lostBefore;
    result.capacityRejected = application.capacityRejected - rejectedBefore;
    result.capacityDeferred = application.capacityDeferred - deferredBefore;
    result.retryTx = destination.runtime.diagnostics().retryTx - retryBefore;
    return result;
  }

 private:
  // Advances the clock by 1 ms; true when the Destination iterated.
  bool step(uint32_t destinationPeriodMs, uint32_t sourcePeriodMs) {
    ++network_.now;
    for (uint8_t source = 0; source < sources_; ++source)
      // Spread the Sources over their period, as independent devices are.
      if ((network_.now + source * 7U) % sourcePeriodMs == 0)
        nodes_[source + 1U]->runtime.iterate();
    if (network_.now % destinationPeriodMs) return false;
    // A blocked main loop (TLS connect, registration) skips whole passes.
    if (stallMs && network_.now % 2500 < stallMs) return false;
    nodes_[0]->runtime.iterate();
    return true;
  }

  static constexpr uint32_t kFirstResource = 50000;
  ScopedCryptoTestState rng_{11};
  Supla::SupLan::OpenSslCryptoPort crypto_;
  SimNetwork network_;
  uint8_t sources_;
  uint8_t channels_;
  std::vector<std::unique_ptr<SimNode>> nodes_;
  std::vector<uint8_t> destinationPeers_;
};

struct ReadScenario {
  const char *name;
  uint8_t sources;
  uint8_t channels;
  uint32_t destinationPeriodMs;
  uint32_t sourcePeriodMs;
  uint32_t firstLimitMs;    // 0: report only.
  uint32_t refreshLimitMs;  // 0: report only.
  uint32_t lossPermille;
  uint32_t stallMs;
  uint32_t latencyMs;
};

void report(const ReadScenario &scenario, const char *phase,
            const SimResult &result) {
  std::printf("[ READSIM  ] %-22s %-7s %s ms=%u loops=%u datagrams=%u"
              " retry_tx=%u recovery_failed=%u session_lost=%u"
              " capacity_rejected=%u capacity_deferred=%u\n",
              scenario.name, phase, result.complete ? "done" : "TIMEOUT",
              result.elapsedMs, result.destinationLoops, result.datagrams,
              result.retryTx, result.recoveryFailed, result.sessionLost,
              result.capacityRejected, result.capacityDeferred);
}

// Acceptance targets: lossless network, Sources looping every 20 ms unless the
// scenario says otherwise. Change the rows to explore other topologies.
constexpr uint32_t kSimTimeoutMs = 180000;
const ReadScenario kScenarios[] = {
    {"12x15 dst=10", 12, 15, 10, 20, 0, 0, 0, 0, 0},
    {"12x15 dst=50", 12, 15, 50, 20, 1500, 1000, 0, 0, 0},
    {"12x15 dst=200", 12, 15, 200, 20, 5000, 0, 0, 0, 0},
    {"12x15 dst=500", 12, 15, 500, 20, 0, 0, 0, 0, 0},
    {"60x3 dst=50", 60, 3, 50, 20, 3000, 0, 0, 0, 0},
    {"32x6 dst=50", 32, 6, 50, 20, 3000, 0, 0, 0, 0},
    {"12x15 dst=20 src=200", 12, 15, 20, 200, 5000, 0, 0, 0, 0},
    {"12x15 dst=50 loss=5%", 12, 15, 50, 20, 0, 0, 50, 0, 0},
    {"12x15 dst=50 loss=10%", 12, 15, 50, 20, 0, 0, 100, 0, 0},
    // Field conditions: replies arrive later than the next Destination pass
    // and the main loop stalls for a second now and then.
    {"12x15 dst=180 lat=100", 12, 15, 180, 50, 0, 0, 0, 0, 100},
    {"12x15 dst=180 field", 12, 15, 180, 50, 0, 0, 20, 1000, 100},
};

class SupLanReadSim : public testing::TestWithParam<ReadScenario> {};

TEST_P(SupLanReadSim, ReadsEveryResourceWithinTarget) {
  const ReadScenario &scenario = GetParam();
  ReadSimulation simulation(scenario.sources, scenario.channels,
                            scenario.lossPermille);
  simulation.stallMs = scenario.stallMs;
  simulation.setLatency(scenario.latencyMs);
  const SimResult first = simulation.readAll(
      scenario.destinationPeriodMs, scenario.sourcePeriodMs, kSimTimeoutMs);
  report(scenario, "first", first);
  ASSERT_TRUE(first.complete);
  simulation.settle(scenario.destinationPeriodMs, scenario.sourcePeriodMs);
  const SimResult refresh = simulation.readAll(
      scenario.destinationPeriodMs, scenario.sourcePeriodMs, kSimTimeoutMs);
  report(scenario, "refresh", refresh);
  EXPECT_TRUE(refresh.complete);
  if (scenario.firstLimitMs) {
    EXPECT_LE(first.elapsedMs, scenario.firstLimitMs);
  }
  if (scenario.refreshLimitMs) {
    EXPECT_LE(refresh.elapsedMs, scenario.refreshLimitMs);
  }
  if (!scenario.lossPermille) {
    EXPECT_EQ(first.recoveryFailed, 0U);
    EXPECT_EQ(refresh.recoveryFailed, 0U);
  }
}

std::string scenarioName(const testing::TestParamInfo<ReadScenario> &info) {
  std::string name = info.param.name;
  for (char &c : name)
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z'))) c = '_';
  return name;
}

INSTANTIATE_TEST_SUITE_P(Scenarios, SupLanReadSim,
                         testing::ValuesIn(kScenarios), scenarioName);

}  // namespace

#endif  // SUPLAN_PROFILE == SUPLAN_PROFILE_LARGE

// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later
#include "legacy_native9_peer.h"
#include <string.h>
#include <supla-common/proto.h>
#include <suplan/suplan_runtime.h>
#include <suplan/suplan_crypto.h>
#include <suplan_crypto_openssl.h>
#include <deque>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
using namespace Supla::SupLan;  // NOLINT(build/namespaces)
void require(bool condition, const char *message) {
  if (!condition) throw std::runtime_error(message);
}
class CurrentDatagrams : public DatagramPort {
 public:
  bool sendUnicast(const Endpoint &, const uint8_t *data,
                   size_t length) override {
    legacyPeerReceive(data, length);
    if (length > 3 && data[0] == kAdaptationFull && data[2] == kFrameData)
      protectedFrames.emplace_back(data, data + length);
    return true;
  }
  bool sendLocateMulticast(const uint8_t *data, size_t length) override {
    return sendUnicast({}, data, length);
  }
  int pollReceive(uint8_t *data, size_t capacity, Endpoint *endpoint) override {
    const auto length = legacyPeerTransmit(data, capacity);
    *endpoint = {0x0100007f, 2016};
    return length;
  }
  uint32_t nowMs() const override { return now; }
  size_t maxDatagramPayload() const override { return 250; }
  uint32_t now = 0;
  std::vector<std::vector<uint8_t>> protectedFrames;
};
class CurrentApplication : public ApplicationPort {
 public:
  void receiveState(uint8_t, const ResourceId &, uint32_t,
                    const uint8_t *, size_t) override { ++states; }
  void operationAcknowledged(uint8_t, const ResourceId &, uint32_t,
                             uint8_t result) override {
    ++acks;
    lastResult = result;
  }
  bool readResource(const ResourceId &, bool *, uint8_t *, size_t,
                    size_t *) override { return false; }
  uint8_t dispatchControl(const ResourceId &, uint32_t,
                          const uint8_t *, size_t) override { return 24; }
  void receiveAction(uint8_t, const ResourceId &, uint32_t,
                     const uint8_t *, size_t) override {}
  uint32_t states = 0, acks = 0;
  uint8_t lastResult = 0;
};
void verify() {
  require(legacyPeerInit(), "legacy Source peer setup failed");
  OpenSslCryptoPort crypto;
  CurrentDatagrams datagrams;
  CurrentApplication application;
  PeerTable peers;
  PeerContext context = {kAuthorityServer, 0, {kNodeIdDevice, 1001},
                         {kNodeIdDevice, 1002}, 1, 1};
  uint8_t root[32] = {1};
  PeerMaterial material;
  require(derivePeerMaterial(&context, root, &material), "peer KDF failed");
  ResourceId resource = {kResourceTypeChannel, 50001};
  AclEntry acl = {resource, kPermissionRead | kPermissionControl};
  uint8_t index;
  require(peers.addPeer(&context, material.peerKey, 1, &acl, 1, &index),
          "Destination peer setup failed");
  Runtime runtime(&crypto, &datagrams, &application, &peers,
                  {kNodeIdDevice, 1002}, 29);
  auto pump = [&](uint32_t duration) {
    for (uint32_t elapsed = 0; elapsed < duration; elapsed += 10) {
      datagrams.now += 10;
      legacyPeerIterate(datagrams.now);
      runtime.iterate();
    }
  };
  require(runtime.requestRead(index, resource), "READ admission failed");
  pump(500);
  require(runtime.hasSession(index) && application.states == 1,
          "current Destination cannot READ from legacy Source");
  const auto acksBefore = application.acks;
  datagrams.protectedFrames.clear();
  require(runtime.sendControlAction(index, resource, ACTION_TOGGLE, nullptr, 0),
          "Native/9 admission failed");
  pump(5500);
  require(legacyPeerControlCalls() == 0, "unknown Native/9 mutated Source");
  require(application.acks == acksBefore && application.lastResult != 33,
          "silent old Source was incorrectly reported as ACK(33)");
  require(runtime.poolDiagnostics().retries.used == 0 &&
          runtime.poolDiagnostics().deferredEvents.used == 0,
          "Native/9 timeout retained work");
  require(legacyPeerInvalidData() >= 1, "legacy Source did not decode DATA");
  require(datagrams.protectedFrames.size() == 3,
          "Native/9 must send initial protected DATA plus two retries");
  require(datagrams.protectedFrames[0] == datagrams.protectedFrames[1] &&
          datagrams.protectedFrames[1] == datagrams.protectedFrames[2],
          "same-session retry changed protected bytes");
  uint8_t value[17] = {};
  value[4] = 0xff;
  value[9] = 1;
  require(runtime.sendControl(index, resource, value, sizeof(value)),
          "legacy CONTROL_VALUE admission failed after Native/9 timeout");
  pump(500);
  require(legacyPeerControlCalls() == 1 && application.lastResult == 3 &&
          application.acks == acksBefore + 1,
          "ordinary CONTROL_VALUE no longer works with old Source");
  std::cout << "PASS: legacy READ, silent Native/9 timeout, identical retries, "
               "no ACK(33), CONTROL_VALUE preserved\n";
}
}  // namespace
int main() {
  try {
    verify();
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "FAIL: " << error.what() << '\n';
    return 1;
  }
}

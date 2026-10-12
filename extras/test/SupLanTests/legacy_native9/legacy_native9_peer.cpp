// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later
#include "legacy_native9_peer.h"
#include <string.h>
#include <suplan/suplan_runtime.h>
#include <suplan_crypto_openssl.h>
#include <deque>
#include <vector>

namespace {
using namespace Supla::SupLan;  // NOLINT(build/namespaces)
class LegacyDatagrams : public DatagramPort {
 public:
  bool sendUnicast(const Endpoint &, const uint8_t *data,
                   size_t length) override {
    tx.emplace_back(data, data + length);
    return true;
  }
  bool sendLocateMulticast(const uint8_t *data, size_t length) override {
    return sendUnicast({}, data, length);
  }
  int pollReceive(uint8_t *data, size_t capacity, Endpoint *endpoint) override {
    if (rx.empty() || rx.front().size() > capacity) return 0;
    const int length = rx.front().size();
    memcpy(data, rx.front().data(), length);
    *endpoint = {0x0200007f, 2016};
    rx.pop_front();
    return length;
  }
  uint32_t nowMs() const override { return now; }
  size_t maxDatagramPayload() const override { return 250; }
  uint32_t now = 0;
  std::deque<std::vector<uint8_t>> rx, tx;
};
class LegacyApplication : public ApplicationPort {
 public:
  bool readResource(const ResourceId &resource, bool *eventOnly,
                    uint8_t *data, size_t capacity, size_t *length) override {
    if (resource.id != 50001 || capacity < 14) return false;
    *eventOnly = false;
    *length = 14;
    memset(data, 0, 14);
    data[0] = 0xff;
    return true;
  }
  uint8_t dispatchControl(const ResourceId &resource, uint32_t,
                          const uint8_t *, size_t length) override {
    if (resource.id != 50001 || length != 17) return 24;
    ++controls;
    return 3;
  }
  void receiveState(uint8_t, const ResourceId &, uint32_t,
                    const uint8_t *, size_t) override {}
  void receiveAction(uint8_t, const ResourceId &, uint32_t,
                     const uint8_t *, size_t) override {}
  void operationAcknowledged(uint8_t, const ResourceId &, uint32_t,
                             uint8_t) override {}
  uint32_t controls = 0;
};
struct LegacyPeer {
  OpenSslCryptoPort crypto;
  LegacyDatagrams datagrams;
  LegacyApplication application;
  PeerTable peers;
  Runtime runtime{&crypto, &datagrams, &application, &peers,
                  {kNodeIdDevice, 1001}, 29};
};
LegacyPeer &peer() {
  static LegacyPeer instance;
  return instance;
}
}  // namespace

bool legacyPeerInit() {
  PeerContext context = {kAuthorityServer, 0, {kNodeIdDevice, 1001},
                         {kNodeIdDevice, 1002}, 1, 1};
  uint8_t root[32] = {1};
  AclEntry acl = {{kResourceTypeChannel, 50001},
                  kPermissionRead | kPermissionControl};
  uint8_t index;
  return peer().peers.addPeerFromRoot(&context, root, 1, &acl, 1, &index);
}
void legacyPeerReceive(const uint8_t *data, size_t length) {
  peer().datagrams.rx.emplace_back(data, data + length);
}
void legacyPeerIterate(uint32_t now) {
  peer().datagrams.now = now;
  peer().runtime.iterate();
}
size_t legacyPeerTransmit(uint8_t *data, size_t capacity) {
  auto &tx = peer().datagrams.tx;
  if (tx.empty() || tx.front().size() > capacity) return 0;
  const auto length = tx.front().size();
  memcpy(data, tx.front().data(), length);
  tx.pop_front();
  return length;
}
uint32_t legacyPeerControlCalls() { return peer().application.controls; }
uint32_t legacyPeerInvalidData() {
  return peer().runtime.diagnostics().invalidDataDrop;
}

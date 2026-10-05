// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright 2026 AC SOFTWARE SP. Z O.O.
#include <gtest/gtest.h>
#include <crypto_test_hooks.h>
#include <suplan/suplan_crypto.h>
#include <suplan/suplan_runtime.h>
#include <suplan/suplan_wire.h>

#include <cstring>
#include <vector>

#include "suplan_crypto_openssl.h"

namespace {
using Supla::SupLan::AclEntry;
using Supla::SupLan::AdaptedFrameView;
using Supla::SupLan::ApplicationPort;
using Supla::SupLan::DatagramPort;
using Supla::SupLan::decodeSessionAccept;
using Supla::SupLan::decodeSessionInit;
using Supla::SupLan::deriveSessionKeys;
using Supla::SupLan::DirectionalKeys;
using Supla::SupLan::encodeApplicationData;
using Supla::SupLan::encodeProtectedData;
using Supla::SupLan::encodeResourceId;
using Supla::SupLan::encodeSessionAccept;
using Supla::SupLan::encodeSessionInit;
using Supla::SupLan::Endpoint;
using Supla::SupLan::FragmentReassembler;
using Supla::SupLan::FragmentSender;
using Supla::SupLan::kAckRequired;
using Supla::SupLan::kAdaptationFragment;
using Supla::SupLan::kAdaptationFull;
using Supla::SupLan::kAdaptationMalformed;
using Supla::SupLan::kAeadTagSize;
using Supla::SupLan::kAuthorityServer;
using Supla::SupLan::kChannelNumberUnresolved;
using Supla::SupLan::kFrameData;
using Supla::SupLan::kMaxDatagramPayload;
using Supla::SupLan::kMessageClassNative;
using Supla::SupLan::kNativeReadResource;
using Supla::SupLan::kNodeIdDevice;
using Supla::SupLan::kPermissionAction;
using Supla::SupLan::kPermissionControl;
using Supla::SupLan::kPermissionRead;
using Supla::SupLan::kProtectedHeaderSize;
using Supla::SupLan::kResourceTypeChannel;
using Supla::SupLan::kSessionAcceptSize;
using Supla::SupLan::kSessionInitSize;
using Supla::SupLan::kSuplaCallDeviceChannelExtendedValueChanged;
using Supla::SupLan::kVersion;
using Supla::SupLan::NodeAddress;
using Supla::SupLan::OpenSslCryptoPort;
using Supla::SupLan::PeerContext;
using Supla::SupLan::PeerMaterial;
using Supla::SupLan::PeerTable;
using Supla::SupLan::putUint16;
using Supla::SupLan::putUint32;
using Supla::SupLan::ResourceId;
using Supla::SupLan::Runtime;
using Supla::SupLan::SessionAccept;
using Supla::SupLan::SessionInit;
using Supla::SupLan::SessionKeys;

class Transport : public DatagramPort {
 public:
  bool sendUnicast(const Endpoint &, const uint8_t *data,
                   size_t length) override {
    sent.emplace_back(data, data + length);
    return true;
  }
  bool sendLocateMulticast(const uint8_t *, size_t) override { return false; }
  int pollReceive(uint8_t *out, size_t capacity, Endpoint *source) override {
    if (incoming.empty()) return 0;
    if (incoming.front().size() > capacity) return -1;
    const int length = static_cast<int>(incoming.front().size());
    std::memcpy(out, incoming.front().data(), length);
    incoming.erase(incoming.begin());
    *source = Endpoint{0x0200007f, 32172};
    return length;
  }
  size_t maxDatagramPayload() const override { return kMaxDatagramPayload; }
  uint32_t nowMs() const override { return 100; }
  std::vector<std::vector<uint8_t>> incoming, sent;
};
class Application : public ApplicationPort {
 public:
  bool readResource(const ResourceId &, bool *eventOnly, uint8_t *out,
                    size_t capacity, size_t *length) override {
    *eventOnly = false;
    if (capacity < 14) return false;
    std::memset(out, 0, 14);
    out[0] = kChannelNumberUnresolved;
    *length = 14;
    return true;
  }
  uint8_t dispatchControl(const ResourceId &, uint32_t, const uint8_t *,
                          size_t) override {
    return 3;
  }
  void receiveState(uint8_t, const ResourceId &, uint32_t, const uint8_t *,
                    size_t) override {}
  void receiveAction(uint8_t, const ResourceId &, uint32_t, const uint8_t *,
                     size_t) override {}
  void operationAcknowledged(uint8_t, const ResourceId &, uint32_t,
                             uint8_t) override {}
};

class DirectionalPeer : public ::testing::Test {
 protected:
  DirectionalPeer()
      : runtime(&crypto, &transport, &application, &peers,
                NodeAddress{kNodeIdDevice, 1001}, 27) {}
  void SetUp() override {
    const PeerContext context = {
        kAuthorityServer,      0, {kNodeIdDevice, 1001},
        {kNodeIdDevice, 1002}, 1, 1};
    const AclEntry acl = {
        resource, static_cast<uint8_t>(kPermissionRead | kPermissionControl |
                                       kPermissionAction)};
    uint8_t root[32] = {};
    ASSERT_TRUE(
        peers.addPeerFromRoot(&context, root, 1, &acl, 1, &peer));
    ASSERT_TRUE(peers.materialFor(peer, &material));
  }
  void establish(uint16_t remoteLimit, bool localInitiator) {
    SessionInit init = {};
    SessionAccept accept = {};
    if (localInitiator) {
      // Establish READ interest first, then trigger the actual Source-initiated
      // recovery path with a current Action Trigger after session eviction.
      establish(remoteLimit, false);
      ASSERT_TRUE(runtime.forgetSession(peer));
      transport.sent.clear();
      uint8_t action[15] = {};
      action[0] = kChannelNumberUnresolved;
      action[1] = 1;
      ASSERT_TRUE(
          runtime.publishAction(peer, resource, action, sizeof(action)));
      ASSERT_FALSE(transport.sent.empty());
      ASSERT_EQ(transport.sent.back().size(), kSessionInitSize);
      std::memcpy(initFrame, transport.sent.back().data(), sizeof(initFrame));
      ASSERT_TRUE(decodeSessionInit(material.initMacKey, initFrame,
                                    sizeof(initFrame), &init));
      EXPECT_EQ(init.rxMaxReassembledFrame, 2048);
    } else {
      std::memcpy(init.peerLocator, material.peerLocator, 16);
      init.ni[0] = ++nonce;
      init.suplaProtoVersionMax = 27;
      init.rxMaxReassembledFrame = remoteLimit;
      ASSERT_TRUE(
          encodeSessionInit(material.initMacKey, &init, initFrame));
      transport.incoming.emplace_back(initFrame, initFrame + sizeof(initFrame));
      transport.sent.clear();
      runtime.iterate();
      ASSERT_FALSE(transport.sent.empty());
      ASSERT_EQ(transport.sent.back().size(), kSessionAcceptSize);
      std::memcpy(acceptFrame, transport.sent.back().data(),
                  sizeof(acceptFrame));
      ASSERT_TRUE(decodeSessionAccept(material.acceptMacKey, initFrame,
                                      &init, acceptFrame, sizeof(acceptFrame),
                                      &accept));
      EXPECT_EQ(accept.responderRxMaxReassembledFrame, 2048);
    }
    if (localInitiator) {
      std::memcpy(accept.peerLocator, init.peerLocator, 16);
      accept.nr[0] = ++nonce;
      accept.sessionId = ++sessionId;
      accept.selectedSuplaProtoVersion = 27;
      accept.responderRxMaxReassembledFrame = remoteLimit;
      ASSERT_TRUE(encodeSessionAccept(material.acceptMacKey, initFrame,
                                      &accept, acceptFrame));
      transport.incoming.emplace_back(acceptFrame,
                                      acceptFrame + sizeof(acceptFrame));
      runtime.iterate();
    }
    sessionId = accept.sessionId;
    uint8_t transcriptHash[32];
    ASSERT_TRUE(deriveSessionKeys(
        material.peerKey, material.contextHash, init.ni, accept.nr,
        initFrame, sizeof(initFrame), acceptFrame, sizeof(acceptFrame),
        sessionId, &keys, transcriptHash));
    remoteTransmit =
        localInitiator ? keys.responderToInitiator : keys.initiatorToResponder;
    remoteSequence = 0;
    sendRead();
    EXPECT_EQ(runtime.poolDiagnostics().sessions.used, 1);
  }
  void sendRead() {
    uint8_t body[5], app[11], frame[43], datagram[44];
    ASSERT_TRUE(encodeResourceId(resource.type, resource.id, body));
    size_t length = 0;
    ASSERT_TRUE(encodeApplicationData(kMessageClassNative, kNativeReadResource,
                                      kAckRequired, body, sizeof(body), app,
                                      sizeof(app), &length));
    ASSERT_TRUE(encodeProtectedData(&crypto, &remoteTransmit, sessionId,
                                    remoteSequence++, app, length, frame,
                                    sizeof(frame), &length));
    datagram[0] = kAdaptationFull;
    std::memcpy(datagram + 1, frame, length);
    transport.incoming.emplace_back(datagram, datagram + length + 1);
    runtime.iterate();
  }
  bool sendExtended(size_t protectedLength) {
    // Existing STATE extended-value entry point with an exact payload length.
    const size_t valueLength = protectedLength - 43 - 6;
    uint8_t prefix[6] = {kChannelNumberUnresolved,
                         100,
                         static_cast<uint8_t>(valueLength),
                         static_cast<uint8_t>(valueLength >> 8),
                         0,
                         0};
    std::vector<uint8_t> value(valueLength, 0x5a);
    return runtime.publishStateParts(
        peer, resource, kSuplaCallDeviceChannelExtendedValueChanged, prefix,
        sizeof(prefix), value.data(), value.size());
  }
  ScopedCryptoTestState rng{1};
  OpenSslCryptoPort crypto;
  Transport transport;
  Application application;
  PeerTable peers;
  Runtime runtime;
  ResourceId resource{kResourceTypeChannel, 50001};
  PeerMaterial material{};
  uint8_t peer = 0, nonce = 0;
  uint64_t sessionId = 123;
  uint32_t remoteSequence = 0;
  SessionKeys keys{};
  DirectionalKeys remoteTransmit{};
  uint8_t initFrame[kSessionInitSize], acceptFrame[kSessionAcceptSize];
};

TEST_F(DirectionalPeer, ResponderRetainsInitiatorLimitAfterConfirmation) {
  establish(1024, false);
  transport.sent.clear();
  EXPECT_FALSE(sendExtended(1025));
  EXPECT_TRUE(transport.sent.empty());
  EXPECT_TRUE(sendExtended(1024));
  EXPECT_FALSE(transport.sent.empty());
}
TEST_F(DirectionalPeer, InitiatorRetainsResponderLimitAfterConfirmation) {
  establish(1024, true);
  transport.sent.clear();
  EXPECT_FALSE(sendExtended(1025));
  EXPECT_TRUE(transport.sent.empty());
  EXPECT_TRUE(sendExtended(1024));
}
TEST_F(DirectionalPeer, LargerPeerAllowsLargerProtectedOutbound) {
  establish(4096, false);
  EXPECT_TRUE(sendExtended(1400));
}
TEST_F(DirectionalPeer, RejectsLimitsBelowProtocolMinimum) {
  SessionInit init = {};
  init.suplaProtoVersionMax = 27;
  init.rxMaxReassembledFrame = 1023;
  EXPECT_FALSE(
      encodeSessionInit(material.initMacKey, &init, initFrame));
}
TEST_F(DirectionalPeer, InboundReassemblyRemainsLocal) {
  uint8_t first[32] = {};
  first[0] = kAdaptationFragment;
  putUint32(first + 1, 1);
  putUint16(first + 5, 2049);
  putUint16(first + 7, 0);
  FragmentReassembler reassembly;
  AdaptedFrameView frame{};
  EXPECT_EQ(reassembly.accept(Endpoint{1, 2016}, first, sizeof(first), 0, 1000,
                              nullptr, nullptr, &frame),
            kAdaptationMalformed);
  EXPECT_FALSE(reassembly.active());
}

TEST(DirectionalFragmentation, SenderDoesNotApplyLocalInboundLimit) {
  std::vector<uint8_t> frame(2300, 0);
  frame[0] = kVersion;
  frame[1] = kFrameData;
  putUint16(frame.data() + 14,
            frame.size() - kProtectedHeaderSize - kAeadTagSize);
  FragmentSender sender;
  size_t datagrams = 0;
  const auto capture = [](void *context, const uint8_t *, size_t) {
    ++*static_cast<size_t *>(context);
    return true;
  };
  ASSERT_TRUE(
      sender.send(frame.data(), frame.size(), 250, 1, capture, &datagrams));
  EXPECT_EQ(datagrams, 10U);
}
}  // namespace

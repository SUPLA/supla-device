// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright 2026 AC SOFTWARE SP. Z O.O.

#include <suplan/suplan_crypto.h>
#include <suplan/suplan_acl.h>
#include <suplan/suplan_data.h>
#include <suplan/suplan_fragment.h>
#include <suplan/suplan_session.h>
#include <suplan/suplan_wire.h>

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "suplan_crypto_openssl.h"
#include "suplan_poc1_profile.h"

namespace {

std::vector<uint8_t> fromHex(const char *input) {
  std::vector<uint8_t> output;
  const size_t length = std::strlen(input);
  if (length % 2 != 0) {
    return output;
  }
  output.reserve(length / 2);
  for (size_t i = 0; i < length; i += 2) {
    unsigned int value = 0;
    if (std::sscanf(input + i, "%2x", &value) != 1) {
      return std::vector<uint8_t>();
    }
    output.push_back(static_cast<uint8_t>(value));
  }
  return output;
}

using Supla::SupLan::PeerContext;
using Supla::SupLan::PeerMaterial;
using Supla::SupLan::OpenSslCryptoPort;

struct DatagramCapture {
  std::vector<std::vector<uint8_t> > datagrams;
};

bool captureDatagram(void *context, const uint8_t *datagram, size_t length) {
  DatagramCapture *capture = static_cast<DatagramCapture *>(context);
  capture->datagrams.push_back(std::vector<uint8_t>(datagram,
                                                   datagram + length));
  return true;
}

bool knownSession(void *, uint64_t sessionId) {
  return sessionId == UINT64_C(0x0102030405060708);
}

PeerContext serverPeer() {
  PeerContext context = {};
  context.authorityType = Supla::SupLan::kAuthorityServer;
  context.authorityId = 0;
  context.source.nameSpace = Supla::SupLan::kNodeIdDevice;
  context.source.nodeId = 1001;
  context.destination.nameSpace = Supla::SupLan::kNodeIdDevice;
  context.destination.nodeId = 1002;
  context.rootEpoch = 1;
  context.peerGeneration = 1;
  return context;
}

TEST(SupLanWire, PeerContextUsesCanonical27ByteNetworkOrderEncoding) {
  const PeerContext context = serverPeer();
  std::array<uint8_t, Supla::SupLan::kPeerContextSize> encoded = {};
  ASSERT_TRUE(Supla::SupLan::encodePeerContext(&context, encoded.data()));
  EXPECT_EQ(std::vector<uint8_t>(encoded.begin(), encoded.end()),
            fromHex("01000000000000000001000003e901000003ea00"
                "00000100000001"));
  PeerContext decoded = {};
  ASSERT_TRUE(Supla::SupLan::decodePeerContext(encoded.data(), &decoded));
  EXPECT_EQ(decoded.authorityType, context.authorityType);
  EXPECT_EQ(decoded.authorityId, context.authorityId);
  EXPECT_EQ(decoded.source.nameSpace, context.source.nameSpace);
  EXPECT_EQ(decoded.source.nodeId, context.source.nodeId);
  EXPECT_EQ(decoded.destination.nameSpace, context.destination.nameSpace);
  EXPECT_EQ(decoded.destination.nodeId, context.destination.nodeId);
  EXPECT_EQ(decoded.rootEpoch, context.rootEpoch);
  EXPECT_EQ(decoded.peerGeneration, context.peerGeneration);

  PeerContext invalid = context;
  invalid.source.nameSpace = Supla::SupLan::kNodeIdInvalid;
  EXPECT_FALSE(Supla::SupLan::encodePeerContext(&invalid, encoded.data()));
}

TEST(SupLanWire, ServerClientAndLocalContextsMatchFixedVectors) {
  PeerContext client = serverPeer();
  client.destination.nameSpace = Supla::SupLan::kNodeIdClient;
  client.destination.nodeId = 2001;
  PeerContext local = {};
  local.authorityType = Supla::SupLan::kAuthorityLocal;
  local.authorityId = UINT64_C(0x0102030405060708);
  local.source.nameSpace = Supla::SupLan::kNodeIdLocal;
  local.source.nodeId = 1001;
  local.destination.nameSpace = Supla::SupLan::kNodeIdLocal;
  local.destination.nodeId = 1002;
  local.rootEpoch = 1;
  local.peerGeneration = 1;
  uint8_t encoded[Supla::SupLan::kPeerContextSize];
  ASSERT_TRUE(Supla::SupLan::encodePeerContext(&client, encoded));
  EXPECT_EQ(std::vector<uint8_t>(encoded, encoded + sizeof(encoded)),
            fromHex("01000000000000000001000003e902000007d100"
                "00000100000001"));
  ASSERT_TRUE(Supla::SupLan::encodePeerContext(&local, encoded));
  EXPECT_EQ(std::vector<uint8_t>(encoded, encoded + sizeof(encoded)),
            fromHex("02010203040506070803000003e903000003ea00"
                "00000100000001"));

  OpenSslCryptoPort crypto;
  std::array<uint8_t, 32> root = {};
  for (uint8_t i = 0; i < root.size(); ++i) {
    root[i] = i;
  }
  PeerMaterial material = {};
  ASSERT_TRUE(Supla::SupLan::derivePeerMaterial(
      &crypto, &client, root.data(), &material));
  EXPECT_EQ(std::vector<uint8_t>(
                material.contextHash,
                material.contextHash + sizeof(material.contextHash)),
            fromHex("25b35fec0cf88d92999d28f2d5e701f759abae90"
                "1396389fb11d5e629a2ad7ca"));
  EXPECT_EQ(std::vector<uint8_t>(material.peerKey,
                                 material.peerKey + sizeof(material.peerKey)),
            fromHex("9c3d4ca6bb13849d484db32b26b25b46fb1a85e4"
                "86caf16bb887d077936ed77e"));
  EXPECT_EQ(std::vector<uint8_t>(
                material.peerLocator,
                material.peerLocator + sizeof(material.peerLocator)),
            fromHex("0354d4eb9b964b9c3602751e46751094"));

  ASSERT_TRUE(Supla::SupLan::derivePeerMaterial(
      &crypto, &local, root.data(), &material));
  EXPECT_EQ(std::vector<uint8_t>(
                material.contextHash,
                material.contextHash + sizeof(material.contextHash)),
            fromHex("fcbafa2f4db2e8ce70d9b2c1a09d2e27b263b524"
                "03d584918e963031cdb2881a"));
  EXPECT_EQ(std::vector<uint8_t>(material.peerKey,
                                 material.peerKey + sizeof(material.peerKey)),
            fromHex("bf9ca8ae8b2e6f61791de9b64630587da2ccadf9"
                "e1d63a80a9706e631a7eeefc"));
  EXPECT_EQ(std::vector<uint8_t>(
                material.peerLocator,
                material.peerLocator + sizeof(material.peerLocator)),
            fromHex("5cd6c3c7836977f658834cb35bfcb9f5"));
}

TEST(SupLanCrypto, ServerDevicePeerDerivationMatchesFixedVector) {
  OpenSslCryptoPort crypto;
  const PeerContext context = serverPeer();
  std::array<uint8_t, 32> root = {};
  for (uint8_t i = 0; i < root.size(); ++i) {
    root[i] = i;
  }
  PeerMaterial material = {};
  ASSERT_TRUE(Supla::SupLan::derivePeerMaterial(
      &crypto, &context, root.data(), &material));
  EXPECT_EQ(std::vector<uint8_t>(material.contextHash,
                                 material.contextHash + 32),
            fromHex("d1d3ac62106e821f22fc31efc1f0a5f7045f00b0"
                "852a9d1dae375db9b0329213"));
  uint8_t rootPrk[32];
  ASSERT_TRUE(Supla::SupLan::hkdfExtract(
      &crypto, material.contextHash, sizeof(material.contextHash),
      root.data(), root.size(), rootPrk));
  EXPECT_EQ(std::vector<uint8_t>(rootPrk, rootPrk + sizeof(rootPrk)),
            fromHex("74f6ada86dd5405813d18743d03b9841141b1623"
                "7132666718afb6865b0974e6"));
  EXPECT_EQ(std::vector<uint8_t>(material.peerKey, material.peerKey + 32),
            fromHex("615620310e8a6abbf50536fdcb665e96341e8881"
                "4a5fcee5474636357ca7e985"));
  uint8_t peerPrk[32];
  ASSERT_TRUE(Supla::SupLan::hkdfExtract(
      &crypto, material.contextHash, sizeof(material.contextHash),
      material.peerKey, sizeof(material.peerKey), peerPrk));
  EXPECT_EQ(std::vector<uint8_t>(peerPrk, peerPrk + sizeof(peerPrk)),
            fromHex("4f2fc5494644d96dee08ea00927c80224e3b9ea2"
                "d06191a70e2d64a1abbab22c"));
  EXPECT_EQ(std::vector<uint8_t>(material.peerLocator,
                                 material.peerLocator + 16),
            fromHex("46521d2f6e26c5015da7c5748b606850"));
  EXPECT_EQ(std::vector<uint8_t>(material.locateMacKey,
                                 material.locateMacKey + 32),
            fromHex("63eabb942df91f988fb1deca163ee8a033e27cb8"
                "35aa3fdf92f6fabe00ecd325"));
  EXPECT_EQ(std::vector<uint8_t>(material.initMacKey,
                                 material.initMacKey + 32),
            fromHex("57fbbf237a5fd6b9cb6e71fb0cdab1d189b5cd13"
                "fa4b90030c491acd2efc307d"));
  EXPECT_EQ(std::vector<uint8_t>(material.acceptMacKey,
                                 material.acceptMacKey + 32),
            fromHex("d325c7928d0cd97df26912f4e98f9bafe932c7ea"
                "ffef7b0490661b4804bcfe00"));
}

TEST(SupLanProfile, StaticPeerKeysMatchRootDerivationAndFixedVector) {
  OpenSslCryptoPort crypto;
  Supla::SupLan::PeerTable nodeA;
  Supla::SupLan::PeerTable nodeB;
  uint8_t primaryA = 0;
  uint8_t actionA = 0;
  uint8_t primaryB = 0;
  uint8_t actionB = 0;
  ASSERT_TRUE(Supla::SupLan::Poc1::configurePeerTable(
      &crypto, &nodeA, true, &primaryA, &actionA));
  ASSERT_TRUE(Supla::SupLan::Poc1::configurePeerTable(
      &crypto, &nodeB, false, &primaryB, &actionB));

  Supla::SupLan::PeerMaterial primaryMaterialA = {};
  Supla::SupLan::PeerMaterial primaryMaterialB = {};
  ASSERT_TRUE(nodeA.materialFor(&crypto, primaryA, &primaryMaterialA));
  ASSERT_TRUE(nodeB.materialFor(&crypto, primaryB, &primaryMaterialB));
  const std::vector<uint8_t> expectedPrimaryKey = fromHex(
      "615620310e8a6abbf50536fdcb665e96341e8881"
      "4a5fcee5474636357ca7e985");
  const std::vector<uint8_t> expectedPrimaryLocator =
      fromHex("46521d2f6e26c5015da7c5748b606850");
  EXPECT_EQ(std::vector<uint8_t>(primaryMaterialA.peerKey,
                                 primaryMaterialA.peerKey + 32),
            expectedPrimaryKey);
  EXPECT_EQ(std::vector<uint8_t>(primaryMaterialB.peerKey,
                                 primaryMaterialB.peerKey + 32),
            expectedPrimaryKey);
  EXPECT_EQ(std::vector<uint8_t>(primaryMaterialA.peerLocator,
                                 primaryMaterialA.peerLocator + 16),
            expectedPrimaryLocator);
  EXPECT_EQ(std::memcmp(primaryMaterialA.peerLocator,
                        primaryMaterialB.peerLocator, 16), 0);

  Supla::SupLan::PeerMaterial actionMaterialA = {};
  Supla::SupLan::PeerMaterial actionMaterialB = {};
  ASSERT_TRUE(nodeA.materialFor(&crypto, actionA, &actionMaterialA));
  ASSERT_TRUE(nodeB.materialFor(&crypto, actionB, &actionMaterialB));
  EXPECT_EQ(std::memcmp(actionMaterialA.peerKey, actionMaterialB.peerKey, 32),
            0);
  EXPECT_EQ(std::memcmp(actionMaterialA.peerLocator,
                        actionMaterialB.peerLocator, 16), 0);
}

TEST(SupLanCrypto, LocateMacsAndFramesMatchFixedVectors) {
  OpenSslCryptoPort crypto;
  PeerMaterial material = {};
  const auto locator = fromHex("46521d2f6e26c5015da7c5748b606850");
  const auto locateKey = fromHex(
      "63eabb942df91f988fb1deca163ee8a033e27cb8"
          "35aa3fdf92f6fabe00ecd325");
  std::memcpy(material.peerLocator, locator.data(), locator.size());
  std::memcpy(material.locateMacKey, locateKey.data(), locateKey.size());
  uint8_t nonce[16];
  for (uint8_t i = 0; i < sizeof(nonce); ++i) {
    nonce[i] = i;
  }
  uint8_t mac[16];
  ASSERT_TRUE(Supla::SupLan::locateQueryMac(
      &crypto, &material, 1, Supla::SupLan::kFrameLocate, nonce, mac));
  EXPECT_EQ(std::vector<uint8_t>(mac, mac + sizeof(mac)),
            fromHex("8b1dd4da9c29ffe4d0a1c6c1895a4eb1"));
  uint8_t locate[50];
  ASSERT_TRUE(Supla::SupLan::encodeLocate(
      locate, material.peerLocator, nonce, mac));
  EXPECT_EQ(std::vector<uint8_t>(locate, locate + sizeof(locate)),
            fromHex("010146521d2f6e26c5015da7c5748b6068500001"
                "02030405060708090a0b0c0d0e0f8b1dd4da9c29"
                "ffe4d0a1c6c1895a4eb1"));

  ASSERT_TRUE(Supla::SupLan::locateReplyMac(
      &crypto, &material, 1, Supla::SupLan::kFrameLocateReply, nonce, mac));
  EXPECT_EQ(std::vector<uint8_t>(mac, mac + sizeof(mac)),
            fromHex("fed3613442b3459351ef93febcfa9c4b"));
  uint8_t reply[34];
  ASSERT_TRUE(Supla::SupLan::encodeLocateReply(reply, nonce, mac));
  uint8_t decodedNonce[16];
  uint8_t decodedMac[16];
  ASSERT_TRUE(Supla::SupLan::decodeLocateReply(
      reply, sizeof(reply), decodedNonce, decodedMac));
  EXPECT_EQ(std::memcmp(decodedNonce, nonce, 16), 0);
  EXPECT_EQ(std::memcmp(decodedMac, mac, 16), 0);
  EXPECT_FALSE(Supla::SupLan::decodeLocateReply(
      reply, sizeof(reply) - 1, decodedNonce, decodedMac));
}

TEST(SupLanSession, HandshakeFramesAndDirectionalKeysMatchFixedVector) {
  OpenSslCryptoPort crypto;
  const auto peerKey = fromHex(
      "615620310e8a6abbf50536fdcb665e96341e8881"
          "4a5fcee5474636357ca7e985");
  const auto initKey = fromHex(
      "57fbbf237a5fd6b9cb6e71fb0cdab1d189b5cd13"
          "fa4b90030c491acd2efc307d");
  const auto acceptKey = fromHex(
      "d325c7928d0cd97df26912f4e98f9bafe932c7ea"
          "ffef7b0490661b4804bcfe00");
  const auto locator = fromHex("46521d2f6e26c5015da7c5748b606850");
  Supla::SupLan::SessionInit init = {};
  std::memcpy(init.peerLocator, locator.data(), locator.size());
  for (uint8_t i = 0; i < sizeof(init.ni); ++i) {
    init.ni[i] = i;
  }
  init.suplaProtoVersionMax = 29;
  init.rxMaxReassembledFrame = SUPLAN_RX_MAX_REASSEMBLED_FRAME;
  uint8_t encodedInit[Supla::SupLan::kSessionInitSize];
  ASSERT_TRUE(Supla::SupLan::encodeSessionInit(
      &crypto, initKey.data(), &init, encodedInit));
  EXPECT_EQ(std::vector<uint8_t>(encodedInit,
                                 encodedInit + sizeof(encodedInit)),
            fromHex("010346521d2f6e26c5015da7c5748b6068500001"
                "02030405060708090a0b0c0d0e0f1d0400000000"
                "00a3cc88271c9a3d94afeba2db8f8ed286"));
  Supla::SupLan::SessionInit decodedInit = {};
  ASSERT_TRUE(Supla::SupLan::decodeSessionInit(
      &crypto, initKey.data(), encodedInit, sizeof(encodedInit), &decodedInit));
  EXPECT_EQ(decodedInit.featureBits, 0U);

  Supla::SupLan::SessionAccept accept = {};
  std::memcpy(accept.peerLocator, locator.data(), locator.size());
  for (uint8_t i = 0; i < sizeof(accept.nr); ++i) {
    accept.nr[i] = static_cast<uint8_t>(16 + i);
  }
  accept.sessionId = UINT64_C(0x0102030405060708);
  accept.selectedSuplaProtoVersion = 29;
  accept.responderRxMaxReassembledFrame =
      SUPLAN_RX_MAX_REASSEMBLED_FRAME;
  uint8_t encodedAccept[Supla::SupLan::kSessionAcceptSize];
  ASSERT_TRUE(Supla::SupLan::encodeSessionAccept(
      &crypto, acceptKey.data(), encodedInit, &accept, encodedAccept));
  EXPECT_EQ(std::vector<uint8_t>(encodedAccept,
                                 encodedAccept + sizeof(encodedAccept)),
            fromHex("010446521d2f6e26c5015da7c5748b6068501011"
                "12131415161718191a1b1c1d1e1f010203040506"
                "07081d0400000000008534c7c2750f4a5177d204"
                "69a5004dea"));
  Supla::SupLan::SessionAccept decodedAccept = {};
  ASSERT_TRUE(Supla::SupLan::decodeSessionAccept(
      &crypto, acceptKey.data(), encodedInit, &decodedInit, encodedAccept,
      sizeof(encodedAccept), &decodedAccept));

  const auto contextBytes = fromHex(
      "01000000000000000001000003e901000003ea00"
          "00000100000001");
  uint8_t contextHash[32];
  ASSERT_TRUE(crypto.sha256(contextBytes.data(), contextBytes.size(),
                            contextHash));
  Supla::SupLan::SessionKeys keys = {};
  uint8_t transcriptHash[32];
  ASSERT_TRUE(Supla::SupLan::deriveSessionKeys(
      &crypto, peerKey.data(), contextHash, init.ni, accept.nr,
      encodedInit, sizeof(encodedInit), encodedAccept, sizeof(encodedAccept),
      accept.sessionId, &keys, transcriptHash));
  EXPECT_EQ(std::vector<uint8_t>(transcriptHash, transcriptHash + 32),
            fromHex("f2eed74421dae9bfcf46967d67b42dffa3ba25ec"
                "d5f00a9ea23aad804668ba3c"));
  uint8_t sessionPrk[32];
  uint8_t sessionSalt[32];
  std::memcpy(sessionSalt, init.ni, sizeof(init.ni));
  std::memcpy(sessionSalt + sizeof(init.ni), accept.nr, sizeof(accept.nr));
  ASSERT_TRUE(Supla::SupLan::hkdfExtract(
      &crypto, sessionSalt, sizeof(sessionSalt), peerKey.data(),
      peerKey.size(), sessionPrk));
  EXPECT_EQ(std::vector<uint8_t>(sessionPrk, sessionPrk + sizeof(sessionPrk)),
            fromHex("09d69c18215e0d9aaa5fc5dec9f67102ab857dc8"
                "d6db60898f9c0751eb482577"));
  EXPECT_EQ(std::vector<uint8_t>(keys.initiatorToResponder.trafficKey,
                                 keys.initiatorToResponder.trafficKey + 16),
            fromHex("f207633a8d05799b28b0c734e138ccfa"));
  EXPECT_EQ(std::vector<uint8_t>(keys.initiatorToResponder.noncePrefix,
                                 keys.initiatorToResponder.noncePrefix + 8),
            fromHex("7933a4a94ddd66a9"));
  EXPECT_EQ(std::vector<uint8_t>(keys.responderToInitiator.trafficKey,
                                 keys.responderToInitiator.trafficKey + 16),
            fromHex("06a16e7e5fdfa15373b0726712d8a580"));
  EXPECT_EQ(std::vector<uint8_t>(keys.responderToInitiator.noncePrefix,
                                 keys.responderToInitiator.noncePrefix + 8),
            fromHex("84066b7bfba10814"));
  EXPECT_EQ(std::vector<uint8_t>(keys.responderToInitiator.noncePrefix,
                                 keys.responderToInitiator.noncePrefix + 8),
            fromHex("84066b7bfba10814"));

  encodedAccept[64] ^= 1;
  EXPECT_FALSE(Supla::SupLan::decodeSessionAccept(
      &crypto, acceptKey.data(), encodedInit, &decodedInit, encodedAccept,
      sizeof(encodedAccept), &decodedAccept));
}

TEST(SupLanAcl, RevisionReplacementAndControlImpliesRead) {
  OpenSslCryptoPort crypto;
  const auto peerKey = fromHex(
      "615620310e8a6abbf50536fdcb665e96341e8881"
          "4a5fcee5474636357ca7e985");
  Supla::SupLan::PeerTable peers;
  const PeerContext context = serverPeer();
  Supla::SupLan::AclEntry entry = {};
  entry.resource.type = Supla::SupLan::kResourceTypeChannel;
  entry.resource.id = 50001;
  entry.permissions = Supla::SupLan::kPermissionControl;
  uint8_t index = 0;
  ASSERT_TRUE(peers.addPeer(&crypto, &context, peerKey.data(), 1, &entry, 1,
                            &index));
  EXPECT_EQ(peers.findByLocator(
                fromHex("46521d2f6e26c5015da7c5748b606850").data()), 0);
  EXPECT_TRUE(peers.hasActiveGrants(index));
  EXPECT_TRUE(peers.authorize(index, entry.resource,
                              Supla::SupLan::kPermissionRead));
  EXPECT_TRUE(peers.authorize(index, entry.resource,
                              Supla::SupLan::kPermissionControl));
  EXPECT_FALSE(peers.authorize(index, entry.resource,
                               Supla::SupLan::kPermissionAction));
  EXPECT_TRUE(peers.replaceAcl(index, 1, &entry, 1));

  Supla::SupLan::AclEntry readOnly = entry;
  readOnly.permissions = Supla::SupLan::kPermissionRead;
  EXPECT_FALSE(peers.replaceAcl(index, 1, &readOnly, 1));
  ASSERT_TRUE(peers.replaceAcl(index, 2, &readOnly, 1));
  EXPECT_TRUE(peers.authorize(index, entry.resource,
                              Supla::SupLan::kPermissionRead));
  EXPECT_FALSE(peers.authorize(index, entry.resource,
                               Supla::SupLan::kPermissionControl));
  EXPECT_FALSE(peers.replaceAcl(index, 1, &entry, 1));
  ASSERT_TRUE(peers.replaceAcl(index, 3, nullptr, 0));
  EXPECT_FALSE(peers.hasActiveGrants(index));
}

TEST(SupLanData, ProtectedReadMatchesByteExactVectorAndReplayRules) {
  OpenSslCryptoPort crypto;
  Supla::SupLan::DirectionalKeys keys = {};
  const auto trafficKey = fromHex("f207633a8d05799b28b0c734e138ccfa");
  const auto noncePrefix = fromHex("7933a4a94ddd66a9");
  std::memcpy(keys.trafficKey, trafficKey.data(), trafficKey.size());
  std::memcpy(keys.noncePrefix, noncePrefix.data(), noncePrefix.size());
  const auto resource = fromHex("010000c351");
  uint8_t application[64];
  size_t applicationLength = 0;
  ASSERT_TRUE(Supla::SupLan::encodeApplicationData(
      Supla::SupLan::kMessageClassNative,
      Supla::SupLan::kNativeReadResource, 0, resource.data(), resource.size(),
      application, sizeof(application), &applicationLength));
  EXPECT_EQ(std::vector<uint8_t>(application,
                                 application + applicationLength),
            fromHex("020000000300010000c351"));

  uint8_t frame[128];
  size_t frameLength = 0;
  const uint64_t sessionId = UINT64_C(0x0102030405060708);
  ASSERT_TRUE(Supla::SupLan::encodeProtectedData(
      &crypto, &keys, sessionId, 0, application, applicationLength, frame,
      sizeof(frame), &frameLength));
  EXPECT_EQ(std::vector<uint8_t>(frame, frame + frameLength),
            fromHex("0105010203040506070800000000000b70c4e890"
                "5f267bee703c2e52c30a1928572f2691efcc5d91"
                "4674fc"));

  Supla::SupLan::ReplayWindow replay;
  uint8_t plaintext[128];
  uint64_t decodedSessionId = 0;
  uint32_t sequence = 0;
  size_t plaintextLength = 0;
  uint8_t corrupted[128];
  std::memcpy(corrupted, frame, frameLength);
  corrupted[frameLength - 1] ^= 1;
  EXPECT_EQ(Supla::SupLan::decodeProtectedData(
                &crypto, &keys, corrupted, frameLength, &replay, plaintext,
                sizeof(plaintext), &decodedSessionId, &sequence,
                &plaintextLength),
            Supla::SupLan::kProtectedDataAuthenticationFailed);
  EXPECT_FALSE(replay.initialized());

  EXPECT_EQ(Supla::SupLan::decodeProtectedData(
                &crypto, &keys, frame, frameLength, &replay, plaintext,
                sizeof(plaintext), &decodedSessionId, &sequence,
                &plaintextLength), Supla::SupLan::kProtectedDataOk);
  EXPECT_EQ(decodedSessionId, sessionId);
  EXPECT_EQ(sequence, 0U);
  EXPECT_EQ(std::vector<uint8_t>(plaintext, plaintext + plaintextLength),
            std::vector<uint8_t>(application,
                                 application + applicationLength));
  EXPECT_EQ(Supla::SupLan::decodeProtectedData(
                &crypto, &keys, frame, frameLength, &replay, plaintext,
                sizeof(plaintext), &decodedSessionId, &sequence,
                &plaintextLength), Supla::SupLan::kProtectedDataDuplicate);

  uint8_t changedHeader[128];
  std::memcpy(changedHeader, frame, frameLength);
  changedHeader[10] = 1;
  EXPECT_EQ(Supla::SupLan::decodeProtectedData(
                &crypto, &keys, changedHeader, frameLength, &replay,
                plaintext, sizeof(plaintext), &decodedSessionId, &sequence,
                &plaintextLength),
            Supla::SupLan::kProtectedDataAuthenticationFailed);
  EXPECT_EQ(replay.highestSequence(), 0U);
}

TEST(SupLanData, ProtectedControlActionAndAckMatchFixedVectors) {
  OpenSslCryptoPort crypto;
  Supla::SupLan::DirectionalKeys keys = {};
  const auto trafficKey = fromHex("f207633a8d05799b28b0c734e138ccfa");
  const auto noncePrefix = fromHex("7933a4a94ddd66a9");
  std::memcpy(keys.trafficKey, trafficKey.data(), trafficKey.size());
  std::memcpy(keys.noncePrefix, noncePrefix.data(), noncePrefix.size());
  const uint8_t resource[5] = {0x01, 0x00, 0x00, 0xC3, 0x51};
  const uint64_t sessionId = UINT64_C(0x0102030405060708);
  uint8_t application[64];
  size_t applicationLength = 0;
  uint8_t frame[128];
  size_t frameLength = 0;

  const uint8_t control[17] = {
      0x44, 0x33, 0x22, 0x11, 0xFF, 0x88, 0x13, 0x00, 0x00,
      0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  uint8_t controlBody[sizeof(resource) + sizeof(control)];
  std::memcpy(controlBody, resource, sizeof(resource));
  std::memcpy(controlBody + sizeof(resource), control, sizeof(control));
  ASSERT_TRUE(Supla::SupLan::encodeApplicationData(
      Supla::SupLan::kMessageClassSuplaCall,
      110,  // SUPLA_DS_CALL_CHANNEL_SET_VALUE.
      Supla::SupLan::kAckRequired, controlBody, sizeof(controlBody),
      application, sizeof(application), &applicationLength));
  EXPECT_EQ(std::vector<uint8_t>(application,
                                 application + applicationLength),
            fromHex("010000006e01010000c35144332211ff88130000"
                "0100000000000000"));
  ASSERT_TRUE(Supla::SupLan::encodeProtectedData(
      &crypto, &keys, sessionId, 1, application, applicationLength, frame,
      sizeof(frame), &frameLength));
  EXPECT_EQ(std::vector<uint8_t>(frame, frame + frameLength),
            fromHex("0105010203040506070800000001001c7bdddcc8"
                "64fe626c6baea641b5232cece9a91bd981a19d4e"
                "ec5ae47dfaca44c7ec4483542227f8b8880cafe1"));

  uint8_t actionBody[sizeof(resource) + 15] = {
      0x01, 0x00, 0x00, 0xC3, 0x51, 0xFF, 0x04, 0x03, 0x02, 0x01};
  ASSERT_TRUE(Supla::SupLan::encodeApplicationData(
      Supla::SupLan::kMessageClassSuplaCall,
      700,  // SUPLA_DS_CALL_ACTIONTRIGGER.
      0, actionBody,
      sizeof(actionBody), application, sizeof(application),
      &applicationLength));
  EXPECT_EQ(std::vector<uint8_t>(application,
                                 application + applicationLength),
            fromHex("01000002bc00010000c351ff0403020100000000"
                "000000000000"));
  ASSERT_TRUE(Supla::SupLan::encodeProtectedData(
      &crypto, &keys, sessionId, 2, application, applicationLength, frame,
      sizeof(frame), &frameLength));
  EXPECT_EQ(std::vector<uint8_t>(frame, frame + frameLength),
            fromHex("0105010203040506070800000002001a7bd24d38"
                "9b3a44ab44eb43d2101cdcbbf7c4c07a66ab8994"
                "c32005ba05cdde4a7a693105d7e8653e52c8"));

  const uint8_t ackBody[5] = {0x00, 0x00, 0x00, 0x00, 0x03};
  ASSERT_TRUE(Supla::SupLan::encodeApplicationData(
      Supla::SupLan::kMessageClassNative, Supla::SupLan::kNativeAck, 0,
      ackBody, sizeof(ackBody), application, sizeof(application),
      &applicationLength));
  EXPECT_EQ(std::vector<uint8_t>(application,
                                 application + applicationLength),
            fromHex("0200000001000000000003"));
  ASSERT_TRUE(Supla::SupLan::encodeProtectedData(
      &crypto, &keys, sessionId, 3, application, applicationLength, frame,
      sizeof(frame), &frameLength));
  EXPECT_EQ(std::vector<uint8_t>(frame, frame + frameLength),
            fromHex("0105010203040506070800000003000b689686b2"
                "b332f4ffaa3e72a2bdb2f94fe5d2ff6d23caf7a1"
                "86f363"));
}

TEST(SupLanData, ReplayWindowAcceptsReorderingAndRejectsOldSequence) {
  Supla::SupLan::ReplayWindow replay;
  EXPECT_EQ(replay.classify(0), Supla::SupLan::kReplayUnseen);
  EXPECT_TRUE(replay.commit(0));
  EXPECT_EQ(replay.classify(0), Supla::SupLan::kReplayDuplicate);
  EXPECT_TRUE(replay.commit(2));
  EXPECT_TRUE(replay.commit(1));
  EXPECT_FALSE(replay.commit(1));
  EXPECT_TRUE(replay.commit(66));
  EXPECT_EQ(replay.classify(2), Supla::SupLan::kReplayTooOld);
}

TEST(SupLanData, ProtectedFramesRespectPoCApplicationBound) {
  OpenSslCryptoPort crypto;
  Supla::SupLan::DirectionalKeys keys = {};
  uint8_t oversizedApplication[SUPLAN_MAX_APPLICATION_BYTES + 1] = {};
  oversizedApplication[0] = Supla::SupLan::kMessageClassNative;
  Supla::SupLan::putUint32(oversizedApplication + 1,
                           Supla::SupLan::kNativeAck);
  uint8_t output[SUPLAN_MAX_APPLICATION_BYTES + 64] = {};
  size_t outputLength = 0;
  EXPECT_FALSE(Supla::SupLan::encodeProtectedData(
      &crypto, &keys, 1, 0, oversizedApplication,
      sizeof(oversizedApplication), output, sizeof(output), &outputLength));

  uint8_t oversizedFrame[Supla::SupLan::kProtectedHeaderSize +
      SUPLAN_MAX_APPLICATION_BYTES + 1 + Supla::SupLan::kAeadTagSize] = {};
  oversizedFrame[0] = Supla::SupLan::kVersion;
  oversizedFrame[1] = Supla::SupLan::kFrameData;
  Supla::SupLan::putUint64(oversizedFrame + 2, 1);
  Supla::SupLan::putUint32(oversizedFrame + 10, 0);
  Supla::SupLan::putUint16(oversizedFrame + 14,
                           SUPLAN_MAX_APPLICATION_BYTES + 1);
  Supla::SupLan::ReplayWindow replay;
  uint8_t plaintext[SUPLAN_MAX_APPLICATION_BYTES] = {};
  uint64_t sessionId = 0;
  uint32_t sequence = 0;
  size_t plaintextLength = 0;
  EXPECT_EQ(Supla::SupLan::decodeProtectedData(
                &crypto, &keys, oversizedFrame, sizeof(oversizedFrame),
                &replay, plaintext, sizeof(plaintext), &sessionId, &sequence,
                &plaintextLength),
            Supla::SupLan::kProtectedDataMalformed);
}

TEST(SupLanFragmentation, FullAndFragmentFramesUseAssignedTypeBytes) {
  const auto frame = fromHex(
      "0105010203040506070800000000000b70c4e890"
          "5f267bee703c2e52c30a1928572f2691efcc5d91"
          "4674fc");
  EXPECT_EQ(Supla::SupLan::kAdaptationFull, 0);
  EXPECT_EQ(Supla::SupLan::kAdaptationFragment, 1);
  Supla::SupLan::FragmentSender sender;
  DatagramCapture capture;
  ASSERT_TRUE(sender.send(frame.data(), frame.size(), 250, 77,
                          captureDatagram, &capture));
  ASSERT_EQ(capture.datagrams.size(), 1U);
  EXPECT_EQ(capture.datagrams[0][0], Supla::SupLan::kAdaptationFull);

  capture.datagrams.clear();
  ASSERT_TRUE(sender.send(frame.data(), frame.size(), 20, 77,
                          captureDatagram, &capture));
  ASSERT_GT(capture.datagrams.size(), 1U);
  EXPECT_EQ(capture.datagrams[0][0], Supla::SupLan::kAdaptationFragment);
  EXPECT_EQ(Supla::SupLan::getUint16(capture.datagrams[0].data() + 7), 0);
  for (size_t i = 0; i < capture.datagrams.size(); ++i) {
    EXPECT_EQ(capture.datagrams[i][0], Supla::SupLan::kAdaptationFragment);
    EXPECT_EQ(Supla::SupLan::getUint32(capture.datagrams[i].data() + 1), 77U);
    EXPECT_EQ(Supla::SupLan::getUint16(capture.datagrams[i].data() + 5),
              frame.size());
  }
}

TEST(SupLanFragmentation, ReassemblesOutOfOrderAndPrefiltersSession) {
  const auto frame = fromHex(
      "0105010203040506070800000000000b70c4e890"
          "5f267bee703c2e52c30a1928572f2691efcc5d91"
          "4674fc");
  Supla::SupLan::FragmentSender sender;
  DatagramCapture capture;
  ASSERT_TRUE(sender.send(frame.data(), frame.size(), 20, 88,
                          captureDatagram, &capture));
  Supla::SupLan::FragmentReassembler receiver;
  Supla::SupLan::Endpoint source = {0x0100007f, 2016};
  Supla::SupLan::AdaptedFrameView result = {};
  EXPECT_EQ(receiver.accept(source, capture.datagrams[1].data(),
                            capture.datagrams[1].size(), 1, 500, knownSession,
                            nullptr, &result),
            Supla::SupLan::kAdaptationDropped);
  ASSERT_FALSE(capture.datagrams.empty());
  for (size_t i = 0; i < capture.datagrams.size(); ++i) {
    const Supla::SupLan::AdaptationResult status = receiver.accept(
        source, capture.datagrams[i].data(), capture.datagrams[i].size(),
        static_cast<uint32_t>(2 + i), 500, knownSession, nullptr, &result);
    if (i + 1 == capture.datagrams.size()) {
      EXPECT_EQ(status, Supla::SupLan::kAdaptationComplete);
    } else {
      EXPECT_EQ(status, Supla::SupLan::kAdaptationPending);
    }
  }
  ASSERT_EQ(result.length, frame.size());
  EXPECT_EQ(std::vector<uint8_t>(result.data,
                                result.data + result.length),
            frame);

  capture.datagrams.clear();
  ASSERT_TRUE(sender.send(frame.data(), frame.size(), 20, 89,
                          captureDatagram, &capture));
  EXPECT_EQ(receiver.accept(source, capture.datagrams[0].data(),
                            capture.datagrams[0].size(), 10, 500,
                            nullptr, nullptr, &result),
            Supla::SupLan::kAdaptationPending);
  Supla::SupLan::Endpoint other = {0x0200007f, 2016};
  EXPECT_EQ(receiver.accept(other, capture.datagrams[1].data(),
                            capture.datagrams[1].size(), 11, 500,
                            nullptr, nullptr, &result),
            Supla::SupLan::kAdaptationCapacityExceeded);
  EXPECT_TRUE(receiver.expire(510, 500));
  EXPECT_FALSE(receiver.active());
}

TEST(SupLanFragmentation, RejectsMalformedRangesAndConflictingOverlap) {
  Supla::SupLan::FragmentReassembler receiver;
  Supla::SupLan::Endpoint source = {0x0100007f, 2016};
  Supla::SupLan::AdaptedFrameView result = {};
  uint8_t malformed[11] = {Supla::SupLan::kAdaptationFragment};
  Supla::SupLan::putUint32(malformed + 1, 1);
  Supla::SupLan::putUint16(malformed + 5, 40);
  Supla::SupLan::putUint16(malformed + 7, 39);
  malformed[9] = 1;
  malformed[10] = 2;
  EXPECT_EQ(receiver.accept(source, malformed, sizeof(malformed), 0, 100,
                            nullptr, nullptr, &result),
            Supla::SupLan::kAdaptationMalformed);

  const auto frame = fromHex(
      "0105010203040506070800000000000b70c4e890"
          "5f267bee703c2e52c30a1928572f2691efcc5d91"
          "4674fc");
  Supla::SupLan::FragmentSender sender;
  DatagramCapture capture;
  ASSERT_TRUE(sender.send(frame.data(), frame.size(), 20, 2,
                          captureDatagram, &capture));
  ASSERT_EQ(receiver.accept(source, capture.datagrams[0].data(),
                            capture.datagrams[0].size(), 1, 500, nullptr,
                            nullptr, &result),
            Supla::SupLan::kAdaptationPending);
  std::vector<uint8_t> conflicting = capture.datagrams[0];
  conflicting.back() ^= 1;
  EXPECT_EQ(receiver.accept(source, conflicting.data(), conflicting.size(), 2,
                            500, nullptr, nullptr, &result),
            Supla::SupLan::kAdaptationMalformed);
  EXPECT_FALSE(receiver.active());
}

TEST(SupLanCrypto, OpenSslPrimitiveKnownAnswers) {
  OpenSslCryptoPort crypto;
  const uint8_t message[] = {'a', 'b', 'c'};
  uint8_t digest[32];
  ASSERT_TRUE(crypto.sha256(message, sizeof(message), digest));
  EXPECT_EQ(std::vector<uint8_t>(digest, digest + sizeof(digest)),
            fromHex("ba7816bf8f01cfea414140de5dae2223b00361a3"
                "96177a9cb410ff61f20015ad"));

  const auto key = fromHex("0b" "0b" "0b" "0b" "0b" "0b" "0b" "0b"
                           "0b" "0b" "0b" "0b" "0b" "0b" "0b" "0b"
                           "0b" "0b" "0b" "0b");
  const uint8_t hmacMessage[] = "Hi There";
  ASSERT_TRUE(crypto.hmacSha256(key.data(), key.size(), hmacMessage,
                                sizeof(hmacMessage) - 1, digest));
  EXPECT_EQ(std::vector<uint8_t>(digest, digest + sizeof(digest)),
            fromHex("b0344c61d8db38535ca8afceaf0bf12b881dc200"
                "c9833da726e9376c2e32cff7"));
}

}  // namespace

// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_EXAMPLES_SUPLAN_POC1_COMMON_SUPLAN_POC1_PROFILE_H_
#define EXTRAS_EXAMPLES_SUPLAN_POC1_COMMON_SUPLAN_POC1_PROFILE_H_

#include <suplan/suplan_acl.h>

namespace Supla {
namespace SupLan {
namespace Poc1 {

static const uint32_t kRelayResourceId = 50001;
static const uint32_t kActionResourceId = 50002;
static const uint32_t kDeviceAId = 1001;
static const uint32_t kDeviceBId = 1002;

// TEST FIXTURE ONLY. Never use these deterministic PoC credentials in a
// deployed device or copy them into production provisioning.
static const uint8_t kRootKey[32] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
};

// SERVER Device:1001 -> Device:1002 PeerKey for kRootKey above.
static const uint8_t kPrimaryPeerKey[32] = {
    0x61, 0x56, 0x20, 0x31, 0x0E, 0x8A, 0x6A, 0xBB,
    0xF5, 0x05, 0x36, 0xFD, 0xCB, 0x66, 0x5E, 0x96,
    0x34, 0x1E, 0x88, 0x81, 0x4A, 0x5F, 0xCE, 0xE5,
    0x47, 0x46, 0x36, 0x35, 0x7C, 0xA7, 0xE9, 0x85,
};

// SERVER Device:1002 -> Device:1001 PeerKey for kRootKey above.
static const uint8_t kActionPeerKey[32] = {
    0x70, 0xD1, 0x3F, 0x17, 0x8A, 0x1F, 0x93, 0xE3,
    0x4E, 0x0C, 0x8A, 0x0B, 0x84, 0x4C, 0x4E, 0xAB,
    0x04, 0xEC, 0xF8, 0x62, 0x90, 0x42, 0xD9, 0xC0,
    0xFA, 0x42, 0xB9, 0xC1, 0xC4, 0x62, 0x47, 0xCC,
};

inline bool configurePeerTable(CryptoPort *crypto, PeerTable *peers,
                               bool nodeA, uint8_t *primaryPeer,
                               uint8_t *actionPeer) {
  if (crypto == nullptr || peers == nullptr || primaryPeer == nullptr ||
      actionPeer == nullptr) {
    return false;
  }
  const PeerContext primary = {
      kAuthorityServer, 0,
      {kNodeIdDevice, kDeviceAId}, {kNodeIdDevice, kDeviceBId}, 1, 1};
  const PeerContext action = {
      kAuthorityServer, 0,
      {kNodeIdDevice, kDeviceBId}, {kNodeIdDevice, kDeviceAId}, 1, 1};
  const AclEntry primaryAcl = {
      {kResourceTypeChannel, kRelayResourceId},
      static_cast<uint8_t>(kPermissionRead | kPermissionControl)};
  const AclEntry actionAcl = {
      {kResourceTypeChannel, kActionResourceId}, kPermissionAction};

  const bool primaryAdded = nodeA
      ? peers->addPeerFromRoot(crypto, &primary, kRootKey, 1, &primaryAcl, 1,
                               primaryPeer)
      : peers->addPeer(crypto, &primary, kPrimaryPeerKey, 1, &primaryAcl, 1,
                       primaryPeer);
  const bool actionAdded = nodeA
      ? peers->addPeer(crypto, &action, kActionPeerKey, 1, &actionAcl, 1,
                       actionPeer)
      : peers->addPeerFromRoot(crypto, &action, kRootKey, 1, &actionAcl, 1,
                               actionPeer);
  return primaryAdded && actionAdded && *primaryPeer == 0 && *actionPeer == 1;
}

inline NodeAddress localNodeAddress(bool nodeA) {
  const NodeAddress local = {
      kNodeIdDevice, nodeA ? kDeviceAId : kDeviceBId};
  return local;
}

}  // namespace Poc1
}  // namespace SupLan
}  // namespace Supla

#endif  // EXTRAS_EXAMPLES_SUPLAN_POC1_COMMON_SUPLAN_POC1_PROFILE_H_

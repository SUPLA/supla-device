// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "suplan_wire.h"

#include <string.h>

namespace Supla {
namespace SupLan {

void putUint16(uint8_t *output, uint16_t value) {
  output[0] = static_cast<uint8_t>(value >> 8);
  output[1] = static_cast<uint8_t>(value);
}

void putUint32(uint8_t *output, uint32_t value) {
  output[0] = static_cast<uint8_t>(value >> 24);
  output[1] = static_cast<uint8_t>(value >> 16);
  output[2] = static_cast<uint8_t>(value >> 8);
  output[3] = static_cast<uint8_t>(value);
}

void putUint64(uint8_t *output, uint64_t value) {
  output[0] = static_cast<uint8_t>(value >> 56);
  output[1] = static_cast<uint8_t>(value >> 48);
  output[2] = static_cast<uint8_t>(value >> 40);
  output[3] = static_cast<uint8_t>(value >> 32);
  output[4] = static_cast<uint8_t>(value >> 24);
  output[5] = static_cast<uint8_t>(value >> 16);
  output[6] = static_cast<uint8_t>(value >> 8);
  output[7] = static_cast<uint8_t>(value);
}

uint16_t getUint16(const uint8_t *input) {
  return static_cast<uint16_t>((static_cast<uint16_t>(input[0]) << 8) |
                                input[1]);
}

uint32_t getUint32(const uint8_t *input) {
  return (static_cast<uint32_t>(input[0]) << 24) |
         (static_cast<uint32_t>(input[1]) << 16) |
         (static_cast<uint32_t>(input[2]) << 8) |
         static_cast<uint32_t>(input[3]);
}

uint64_t getUint64(const uint8_t *input) {
  uint64_t value = 0;
  for (uint8_t i = 0; i < 8; ++i) {
    value = (value << 8) | input[i];
  }
  return value;
}

static bool validAddress(const NodeAddress &address) {
  if (address.nameSpace < kNodeIdDevice ||
      address.nameSpace > kNodeIdLocal) {
    return false;
  }
  if (address.nameSpace == kNodeIdLocal && address.nodeId > 0xFFFFU) {
    return false;
  }
  return true;
}

bool validPeerContext(const PeerContext *context) {
  if (context == nullptr ||
      (context->authorityType != kAuthorityServer &&
       context->authorityType != kAuthorityLocal) ||
      !validAddress(context->source) ||
      !validAddress(context->destination)) {
    return false;
  }
  if (context->authorityType == kAuthorityServer &&
      context->authorityId != 0) {
    return false;
  }
  return true;
}

bool encodePeerContext(const PeerContext *context,
                       uint8_t output[kPeerContextSize]) {
  if (output == nullptr || !validPeerContext(context)) {
    return false;
  }
  output[0] = context->authorityType;
  putUint64(output + 1, context->authorityId);
  output[9] = context->source.nameSpace;
  putUint32(output + 10, context->source.nodeId);
  output[14] = context->destination.nameSpace;
  putUint32(output + 15, context->destination.nodeId);
  putUint32(output + 19, context->rootEpoch);
  putUint32(output + 23, context->peerGeneration);
  return true;
}

bool decodePeerContext(const uint8_t input[kPeerContextSize],
                       PeerContext *context) {
  if (input == nullptr || context == nullptr) {
    return false;
  }
  PeerContext decoded = {};
  decoded.authorityType = input[0];
  decoded.authorityId = getUint64(input + 1);
  decoded.source.nameSpace = input[9];
  decoded.source.nodeId = getUint32(input + 10);
  decoded.destination.nameSpace = input[14];
  decoded.destination.nodeId = getUint32(input + 15);
  decoded.rootEpoch = getUint32(input + 19);
  decoded.peerGeneration = getUint32(input + 23);
  if (!validPeerContext(&decoded)) {
    return false;
  }
  *context = decoded;
  return true;
}

bool encodeLocate(uint8_t output[50], const uint8_t peerLocator[16],
                  const uint8_t nonce[16], const uint8_t mac[16]) {
  if (output == nullptr || peerLocator == nullptr || nonce == nullptr ||
      mac == nullptr) {
    return false;
  }
  output[0] = kVersion;
  output[1] = kFrameLocate;
  memcpy(output + 2, peerLocator, 16);
  memcpy(output + 18, nonce, 16);
  memcpy(output + 34, mac, 16);
  return true;
}

bool decodeLocate(const uint8_t *input, size_t length,
                  uint8_t peerLocator[16], uint8_t nonce[16],
                  uint8_t mac[16]) {
  if (input == nullptr || peerLocator == nullptr || nonce == nullptr ||
      mac == nullptr || length != 50 || input[0] != kVersion ||
      input[1] != kFrameLocate) {
    return false;
  }
  memcpy(peerLocator, input + 2, 16);
  memcpy(nonce, input + 18, 16);
  memcpy(mac, input + 34, 16);
  return true;
}

bool encodeLocateReply(uint8_t output[34], const uint8_t nonce[16],
                       const uint8_t mac[16]) {
  if (output == nullptr || nonce == nullptr || mac == nullptr) {
    return false;
  }
  output[0] = kVersion;
  output[1] = kFrameLocateReply;
  memcpy(output + 2, nonce, 16);
  memcpy(output + 18, mac, 16);
  return true;
}

bool decodeLocateReply(const uint8_t *input, size_t length,
                       uint8_t nonce[16], uint8_t mac[16]) {
  if (input == nullptr || nonce == nullptr || mac == nullptr ||
      length != 34 || input[0] != kVersion ||
      input[1] != kFrameLocateReply) {
    return false;
  }
  memcpy(nonce, input + 2, 16);
  memcpy(mac, input + 18, 16);
  return true;
}

}  // namespace SupLan
}  // namespace Supla

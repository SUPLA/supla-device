// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "suplan_session.h"

#include <string.h>

#include "suplan_wire.h"

namespace Supla {
namespace SupLan {

static bool equalBytes(const uint8_t *left, const uint8_t *right,
                       size_t length) {
  uint8_t different = 0;
  for (size_t i = 0; i < length; ++i) {
    different |= static_cast<uint8_t>(left[i] ^ right[i]);
  }
  return different == 0;
}

static bool validNegotiation(const uint8_t maximumVersion,
                             uint16_t receiveLimit,
                             uint32_t features) {
  return maximumVersion >= kMinimumSuplaProtoVersion &&
      receiveLimit >= SUPLAN_RX_MAX_REASSEMBLED_FRAME && features == 0;
}

bool encodeSessionInit(CryptoPort *crypto, const uint8_t initMacKey[32],
                       const SessionInit *init, uint8_t output[57]) {
  if (crypto == nullptr || initMacKey == nullptr || init == nullptr ||
      output == nullptr ||
      !validNegotiation(init->suplaProtoVersionMax,
                        init->rxMaxReassembledFrame, init->featureBits)) {
    return false;
  }
  output[0] = kVersion;
  output[1] = kFrameSessionInit;
  memcpy(output + 2, init->peerLocator, 16);
  memcpy(output + 18, init->ni, 16);
  output[34] = init->suplaProtoVersionMax;
  putUint16(output + 35, init->rxMaxReassembledFrame);
  putUint32(output + 37, init->featureBits);
  uint8_t fullMac[32];
  if (!crypto->hmacSha256(initMacKey, 32, output, 41, fullMac)) {
    return false;
  }
  memcpy(output + 41, fullMac, 16);
  memset(fullMac, 0, sizeof(fullMac));
  return true;
}

bool decodeSessionInit(CryptoPort *crypto, const uint8_t initMacKey[32],
                       const uint8_t *input, size_t length,
                       SessionInit *init) {
  if (crypto == nullptr || initMacKey == nullptr || input == nullptr ||
      init == nullptr || length != kSessionInitSize ||
      input[0] != kVersion || input[1] != kFrameSessionInit) {
    return false;
  }
  const uint16_t rxLimit = getUint16(input + 35);
  const uint32_t features = getUint32(input + 37);
  if (!validNegotiation(input[34], rxLimit, features)) {
    return false;
  }
  uint8_t fullMac[32];
  if (!crypto->hmacSha256(initMacKey, 32, input, 41, fullMac)) {
    return false;
  }
  const bool valid = equalBytes(fullMac, input + 41, 16);
  memset(fullMac, 0, sizeof(fullMac));
  if (!valid) {
    return false;
  }
  memcpy(init->peerLocator, input + 2, 16);
  memcpy(init->ni, input + 18, 16);
  init->suplaProtoVersionMax = input[34];
  init->rxMaxReassembledFrame = rxLimit;
  init->featureBits = features;
  return true;
}

bool encodeSessionAccept(CryptoPort *crypto, const uint8_t acceptMacKey[32],
                         const uint8_t encodedInit[57],
                         const SessionAccept *accept, uint8_t output[65]) {
  if (crypto == nullptr || acceptMacKey == nullptr || encodedInit == nullptr ||
      accept == nullptr || output == nullptr ||
      memcmp(accept->peerLocator, encodedInit + 2, 16) != 0 ||
      accept->selectedSuplaProtoVersion < kMinimumSuplaProtoVersion ||
      accept->responderRxMaxReassembledFrame <
          SUPLAN_RX_MAX_REASSEMBLED_FRAME ||
      accept->selectedFeatureBits != 0 ||
      accept->selectedSuplaProtoVersion > encodedInit[34]) {
    return false;
  }
  output[0] = kVersion;
  output[1] = kFrameSessionAccept;
  memcpy(output + 2, accept->peerLocator, 16);
  memcpy(output + 18, accept->nr, 16);
  putUint64(output + 34, accept->sessionId);
  output[42] = accept->selectedSuplaProtoVersion;
  putUint16(output + 43, accept->responderRxMaxReassembledFrame);
  putUint32(output + 45, accept->selectedFeatureBits);
  uint8_t transcriptPrefix[57 + 49];
  memcpy(transcriptPrefix, encodedInit, 57);
  memcpy(transcriptPrefix + 57, output, 49);
  uint8_t fullMac[32];
  if (!crypto->hmacSha256(acceptMacKey, 32, transcriptPrefix,
                          sizeof(transcriptPrefix), fullMac)) {
    memset(transcriptPrefix, 0, sizeof(transcriptPrefix));
    return false;
  }
  memcpy(output + 49, fullMac, 16);
  memset(fullMac, 0, sizeof(fullMac));
  memset(transcriptPrefix, 0, sizeof(transcriptPrefix));
  return true;
}

bool decodeSessionAccept(CryptoPort *crypto, const uint8_t acceptMacKey[32],
                         const uint8_t encodedInit[57],
                         const SessionInit *init, const uint8_t *input,
                         size_t length, SessionAccept *accept) {
  if (crypto == nullptr || acceptMacKey == nullptr || encodedInit == nullptr ||
      init == nullptr || input == nullptr || accept == nullptr ||
      length != kSessionAcceptSize || input[0] != kVersion ||
      input[1] != kFrameSessionAccept ||
      memcmp(encodedInit + 2, init->peerLocator, 16) != 0 ||
      memcmp(input + 2, init->peerLocator, 16) != 0 ||
      input[42] > init->suplaProtoVersionMax ||
      input[42] < kMinimumSuplaProtoVersion ||
      getUint16(input + 43) < SUPLAN_RX_MAX_REASSEMBLED_FRAME ||
      getUint32(input + 45) != 0) {
    return false;
  }
  uint8_t transcriptPrefix[57 + 49];
  memcpy(transcriptPrefix, encodedInit, 57);
  memcpy(transcriptPrefix + 57, input, 49);
  uint8_t fullMac[32];
  if (!crypto->hmacSha256(acceptMacKey, 32, transcriptPrefix,
                          sizeof(transcriptPrefix), fullMac)) {
    memset(transcriptPrefix, 0, sizeof(transcriptPrefix));
    return false;
  }
  const bool valid = equalBytes(fullMac, input + 49, 16);
  memset(fullMac, 0, sizeof(fullMac));
  memset(transcriptPrefix, 0, sizeof(transcriptPrefix));
  if (!valid) {
    return false;
  }
  memcpy(accept->peerLocator, input + 2, 16);
  memcpy(accept->nr, input + 18, 16);
  accept->sessionId = getUint64(input + 34);
  accept->selectedSuplaProtoVersion = input[42];
  accept->responderRxMaxReassembledFrame = getUint16(input + 43);
  accept->selectedFeatureBits = getUint32(input + 45);
  return true;
}

}  // namespace SupLan
}  // namespace Supla

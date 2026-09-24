// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "suplan_crypto.h"

#include <string.h>

#include "suplan_session.h"
#include "suplan_wire.h"

namespace Supla {
namespace SupLan {

namespace {
static const size_t kMaxHkdfInfoBytes = 100;
}

bool hkdfExtract(CryptoPort *crypto, const uint8_t *salt,
                 size_t saltLength, const uint8_t *input,
                 size_t inputLength, uint8_t output[32]) {
  if (crypto == nullptr || input == nullptr || output == nullptr ||
      (salt == nullptr && saltLength != 0)) {
    return false;
  }
  uint8_t zeroSalt[32] = {};
  const uint8_t *actualSalt = saltLength == 0 ? zeroSalt : salt;
  const size_t actualSaltLength = saltLength == 0 ? sizeof(zeroSalt) :
      saltLength;
  return crypto->hmacSha256(actualSalt, actualSaltLength, input, inputLength,
                            output);
}

bool hkdfExpand(CryptoPort *crypto, const uint8_t prk[32],
                const uint8_t *info, size_t infoLength,
                uint8_t *output, size_t outputLength) {
  if (crypto == nullptr || prk == nullptr || output == nullptr ||
      (info == nullptr && infoLength != 0) || outputLength > 255U * 32U ||
      infoLength > kMaxHkdfInfoBytes) {
    return false;
  }

  uint8_t input[32 + kMaxHkdfInfoBytes + 1];
  uint8_t block[32];
  size_t previousLength = 0;
  size_t outputOffset = 0;
  uint8_t counter = 1;
  while (outputOffset < outputLength) {
    if (previousLength != 0) {
      memcpy(input, block, previousLength);
    }
    if (infoLength != 0) {
      memcpy(input + previousLength, info, infoLength);
    }
    input[previousLength + infoLength] = counter;
    if (!crypto->hmacSha256(prk, 32, input,
                            previousLength + infoLength + 1, block)) {
      memset(input, 0, sizeof(input));
      memset(block, 0, sizeof(block));
      return false;
    }
    size_t amount = outputLength - outputOffset;
    if (amount > sizeof(block)) {
      amount = sizeof(block);
    }
    memcpy(output + outputOffset, block, amount);
    outputOffset += amount;
    previousLength = sizeof(block);
    ++counter;
  }
  memset(input, 0, sizeof(input));
  memset(block, 0, sizeof(block));
  return true;
}

static bool expandLabel(CryptoPort *crypto, const uint8_t prk[32],
                        const char *label, size_t labelLength,
                        const uint8_t *context, size_t contextLength,
                        uint8_t *output, size_t outputLength) {
  uint8_t info[kMaxHkdfInfoBytes];
  if (labelLength + contextLength > sizeof(info)) {
    return false;
  }
  memcpy(info, label, labelLength);
  if (contextLength != 0) {
    memcpy(info + labelLength, context, contextLength);
  }
  bool result = hkdfExpand(crypto, prk, info, labelLength + contextLength,
                           output, outputLength);
  memset(info, 0, sizeof(info));
  return result;
}

bool derivePeerMaterial(CryptoPort *crypto, const PeerContext *context,
                        const uint8_t rootKey[32], PeerMaterial *material) {
  if (crypto == nullptr || rootKey == nullptr || material == nullptr) {
    return false;
  }
  uint8_t canonical[kPeerContextSize];
  uint8_t rootPrk[32];
  static const char peerLabel[] = "SupLAN/v1/peer-key";
  if (!encodePeerContext(context, canonical) ||
      !crypto->sha256(canonical, sizeof(canonical), material->contextHash) ||
      !hkdfExtract(crypto, material->contextHash, 32, rootKey, 32, rootPrk) ||
      !hkdfExpand(crypto, rootPrk,
                  reinterpret_cast<const uint8_t *>(peerLabel),
                  sizeof(peerLabel) - 1, material->peerKey, 32)) {
    memset(rootPrk, 0, sizeof(rootPrk));
    return false;
  }
  memset(rootPrk, 0, sizeof(rootPrk));
  memset(canonical, 0, sizeof(canonical));
  return derivePeerMaterialFromKey(crypto, context, material->peerKey,
                                  material);
}

bool derivePeerMaterialFromKey(CryptoPort *crypto,
                               const PeerContext *context,
                               const uint8_t peerKey[32],
                               PeerMaterial *material) {
  if (crypto == nullptr || context == nullptr || peerKey == nullptr ||
      material == nullptr) {
    return false;
  }
  uint8_t canonical[kPeerContextSize];
  uint8_t peerPrk[32];
  static const char locatorLabel[] = "SupLAN/v1/peer-locator";
  static const char locateLabel[] = "SupLAN/v1/locate-mac";
  static const char initLabel[] = "SupLAN/v1/session-init-mac";
  static const char acceptLabel[] = "SupLAN/v1/session-accept-mac";
  memcpy(material->peerKey, peerKey, 32);
  if (!encodePeerContext(context, canonical) ||
      !crypto->sha256(canonical, sizeof(canonical), material->contextHash) ||
      !hkdfExtract(crypto, material->contextHash, 32, material->peerKey, 32,
                   peerPrk) ||
      !expandLabel(crypto, peerPrk, locateLabel, sizeof(locateLabel) - 1,
                   nullptr, 0, material->locateMacKey, 32) ||
      !expandLabel(crypto, peerPrk, initLabel, sizeof(initLabel) - 1,
                   nullptr, 0, material->initMacKey, 32) ||
      !expandLabel(crypto, peerPrk, acceptLabel, sizeof(acceptLabel) - 1,
                   nullptr, 0, material->acceptMacKey, 32)) {
    memset(peerPrk, 0, sizeof(peerPrk));
    memset(canonical, 0, sizeof(canonical));
    return false;
  }
  uint8_t locatorInput[sizeof(locatorLabel) - 1 + 32];
  memcpy(locatorInput, locatorLabel, sizeof(locatorLabel) - 1);
  memcpy(locatorInput + sizeof(locatorLabel) - 1, material->contextHash, 32);
  uint8_t locatorFull[32];
  if (!crypto->hmacSha256(material->peerKey, 32, locatorInput,
                          sizeof(locatorInput), locatorFull)) {
    memset(peerPrk, 0, sizeof(peerPrk));
    memset(locatorInput, 0, sizeof(locatorInput));
    return false;
  }
  memcpy(material->peerLocator, locatorFull, 16);
  memset(peerPrk, 0, sizeof(peerPrk));
  memset(locatorFull, 0, sizeof(locatorFull));
  memset(locatorInput, 0, sizeof(locatorInput));
  memset(canonical, 0, sizeof(canonical));
  return true;
}

static bool locateMac(CryptoPort *crypto, const PeerMaterial *material,
                      const char *label, size_t labelLength,
                      uint8_t version, uint8_t type,
                      const uint8_t nonce[16], uint8_t mac[16]) {
  if (crypto == nullptr || material == nullptr || nonce == nullptr ||
      mac == nullptr) {
    return false;
  }
  uint8_t input[sizeof("SupLAN/v1/locate-query") - 1 + 34];
  if (labelLength + 2 + 16 + 16 > sizeof(input)) {
    return false;
  }
  memcpy(input, label, labelLength);
  size_t offset = labelLength;
  input[offset++] = version;
  input[offset++] = type;
  if (type == kFrameLocate) {
    memcpy(input + offset, material->peerLocator, 16);
    offset += 16;
  }
  memcpy(input + offset, nonce, 16);
  offset += 16;
  uint8_t full[32];
  if (!crypto->hmacSha256(material->locateMacKey, 32, input, offset, full)) {
    memset(input, 0, sizeof(input));
    return false;
  }
  memcpy(mac, full, 16);
  memset(full, 0, sizeof(full));
  memset(input, 0, sizeof(input));
  return true;
}

bool locateQueryMac(CryptoPort *crypto, const PeerMaterial *material,
                    const uint8_t version, const uint8_t type,
                    const uint8_t nonce[16], uint8_t mac[16]) {
  static const char label[] = "SupLAN/v1/locate-query";
  if (type != kFrameLocate) {
    return false;
  }
  return locateMac(crypto, material, label, sizeof(label) - 1, version, type,
                   nonce, mac);
}

bool locateReplyMac(CryptoPort *crypto, const PeerMaterial *material,
                    const uint8_t version, const uint8_t type,
                    const uint8_t nonce[16], uint8_t mac[16]) {
  static const char label[] = "SupLAN/v1/locate-reply";
  if (type != kFrameLocateReply) {
    return false;
  }
  return locateMac(crypto, material, label, sizeof(label) - 1, version, type,
                   nonce, mac);
}

bool deriveSessionKeys(CryptoPort *crypto, const uint8_t peerKey[32],
                       const uint8_t contextHash[32],
                       const uint8_t ni[16], const uint8_t nr[16],
                       const uint8_t *encodedInit, size_t initLength,
                       const uint8_t *encodedAccept, size_t acceptLength,
                       uint64_t sessionId, SessionKeys *keys,
                       uint8_t transcriptHash[32]) {
  if (crypto == nullptr || peerKey == nullptr || contextHash == nullptr ||
      ni == nullptr || nr == nullptr || encodedInit == nullptr ||
      encodedAccept == nullptr || keys == nullptr ||
      transcriptHash == nullptr ||
      initLength != kSessionInitSize ||
      acceptLength != kSessionAcceptSize) {
    return false;
  }
  uint8_t salt[32];
  memcpy(salt, ni, 16);
  memcpy(salt + 16, nr, 16);
  uint8_t sessionPrk[32];
  uint8_t transcript[kSessionInitSize + kSessionAcceptSize];
  memcpy(transcript, encodedInit, initLength);
  memcpy(transcript + initLength, encodedAccept, acceptLength);
  bool ok = hkdfExtract(crypto, salt, sizeof(salt), peerKey, 32, sessionPrk) &&
      crypto->sha256(transcript, initLength + acceptLength, transcriptHash);
  static const char i2rKeyLabel[] = "SupLAN/v1/i2r/traffic-key";
  static const char i2rNonceLabel[] = "SupLAN/v1/i2r/nonce-prefix";
  static const char r2iKeyLabel[] = "SupLAN/v1/r2i/traffic-key";
  static const char r2iNonceLabel[] = "SupLAN/v1/r2i/nonce-prefix";
  if (ok) {
    memcpy(transcript, contextHash, 32);
    memcpy(transcript + 32, transcriptHash, 32);
    putUint64(transcript + 64, sessionId);
    ok = expandLabel(crypto, sessionPrk, i2rKeyLabel,
                     sizeof(i2rKeyLabel) - 1, transcript, 72,
                     keys->initiatorToResponder.trafficKey,
                     16) &&
         expandLabel(crypto, sessionPrk, i2rNonceLabel,
                     sizeof(i2rNonceLabel) - 1, transcript, 72,
                     keys->initiatorToResponder.noncePrefix, 8) &&
         expandLabel(crypto, sessionPrk, r2iKeyLabel,
                     sizeof(r2iKeyLabel) - 1, transcript, 72,
                     keys->responderToInitiator.trafficKey,
                     16) &&
         expandLabel(crypto, sessionPrk, r2iNonceLabel,
                     sizeof(r2iNonceLabel) - 1, transcript, 72,
                     keys->responderToInitiator.noncePrefix, 8);
  }
  memset(salt, 0, sizeof(salt));
  memset(sessionPrk, 0, sizeof(sessionPrk));
  memset(transcript, 0, sizeof(transcript));
  return ok;
}

}  // namespace SupLan
}  // namespace Supla

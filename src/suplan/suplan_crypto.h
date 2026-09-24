// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLAN_SUPLAN_CRYPTO_H_
#define SRC_SUPLAN_SUPLAN_CRYPTO_H_

#include <stddef.h>
#include <stdint.h>

#include "suplan_ports.h"
#include "suplan_types.h"

namespace Supla {
namespace SupLan {

bool hkdfExtract(CryptoPort *crypto, const uint8_t *salt,
                 size_t saltLength, const uint8_t *input,
                 size_t inputLength, uint8_t output[32]);
bool hkdfExpand(CryptoPort *crypto, const uint8_t prk[32],
                const uint8_t *info, size_t infoLength,
                uint8_t *output, size_t outputLength);
bool derivePeerMaterial(CryptoPort *crypto, const PeerContext *context,
                        const uint8_t rootKey[32], PeerMaterial *material);
bool derivePeerMaterialFromKey(CryptoPort *crypto,
                               const PeerContext *context,
                               const uint8_t peerKey[32],
                               PeerMaterial *material);
bool locateQueryMac(CryptoPort *crypto, const PeerMaterial *material,
                    const uint8_t version, const uint8_t type,
                    const uint8_t nonce[16], uint8_t mac[16]);
bool locateReplyMac(CryptoPort *crypto, const PeerMaterial *material,
                    const uint8_t version, const uint8_t type,
                    const uint8_t nonce[16], uint8_t mac[16]);
bool deriveSessionKeys(CryptoPort *crypto, const uint8_t peerKey[32],
                       const uint8_t contextHash[32],
                       const uint8_t ni[16], const uint8_t nr[16],
                       const uint8_t *encodedInit, size_t initLength,
                       const uint8_t *encodedAccept, size_t acceptLength,
                       uint64_t sessionId, SessionKeys *keys,
                       uint8_t transcriptHash[32]);

}  // namespace SupLan
}  // namespace Supla

#endif  // SRC_SUPLAN_SUPLAN_CRYPTO_H_

// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLAN_SUPLAN_WIRE_H_
#define SRC_SUPLAN_SUPLAN_WIRE_H_

#include <stddef.h>
#include <stdint.h>

#include "suplan_types.h"

namespace Supla {
namespace SupLan {

bool validPeerContext(const PeerContext *context);
bool encodePeerContext(const PeerContext *context,
                       uint8_t output[kPeerContextSize]);
bool decodePeerContext(const uint8_t input[kPeerContextSize],
                       PeerContext *context);

void putUint16(uint8_t *output, uint16_t value);
void putUint32(uint8_t *output, uint32_t value);
void putUint64(uint8_t *output, uint64_t value);
uint16_t getUint16(const uint8_t *input);
uint32_t getUint32(const uint8_t *input);
uint64_t getUint64(const uint8_t *input);

bool encodeLocate(uint8_t output[50], const uint8_t peerLocator[16],
                  const uint8_t nonce[16], const uint8_t mac[16]);
bool decodeLocate(const uint8_t *input, size_t length,
                  uint8_t peerLocator[16], uint8_t nonce[16],
                  uint8_t mac[16]);
bool encodeLocateReply(uint8_t output[34], const uint8_t nonce[16],
                       const uint8_t mac[16]);
bool decodeLocateReply(const uint8_t *input, size_t length,
                       uint8_t nonce[16], uint8_t mac[16]);

}  // namespace SupLan
}  // namespace Supla

#endif  // SRC_SUPLAN_SUPLAN_WIRE_H_

// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLAN_SUPLAN_SESSION_H_
#define SRC_SUPLAN_SUPLAN_SESSION_H_

#include <stddef.h>
#include <stdint.h>

#include "suplan_ports.h"
#include "suplan_types.h"

namespace Supla {
namespace SupLan {

static const size_t kSessionInitSize = 57;
static const size_t kSessionAcceptSize = 65;

struct SessionInit {
  uint8_t peerLocator[16];
  uint8_t ni[16];
  uint8_t suplaProtoVersionMax;
  uint16_t rxMaxReassembledFrame;
  uint32_t featureBits;
};

struct SessionAccept {
  uint8_t peerLocator[16];
  uint8_t nr[16];
  uint64_t sessionId;
  uint8_t selectedSuplaProtoVersion;
  uint16_t responderRxMaxReassembledFrame;
  uint32_t selectedFeatureBits;
};

bool encodeSessionInit(CryptoPort *crypto, const uint8_t initMacKey[32],
                       const SessionInit *init, uint8_t output[57]);
bool decodeSessionInit(CryptoPort *crypto, const uint8_t initMacKey[32],
                       const uint8_t *input, size_t length,
                       SessionInit *init);
bool encodeSessionAccept(CryptoPort *crypto, const uint8_t acceptMacKey[32],
                         const uint8_t encodedInit[57],
                         const SessionAccept *accept, uint8_t output[65]);
bool decodeSessionAccept(CryptoPort *crypto, const uint8_t acceptMacKey[32],
                         const uint8_t encodedInit[57],
                         const SessionInit *init, const uint8_t *input,
                         size_t length, SessionAccept *accept);

}  // namespace SupLan
}  // namespace Supla

#endif  // SRC_SUPLAN_SUPLAN_SESSION_H_

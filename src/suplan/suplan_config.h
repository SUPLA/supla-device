// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLAN_SUPLAN_CONFIG_H_
#define SRC_SUPLAN_SUPLAN_CONFIG_H_

#include <stdint.h>

// PoC1 resource profile. Ports may override these at compile time, but the
// fixed bounds must remain explicit and independent of network input.
#ifndef SUPLAN_MAX_PERSISTENT_PEERS
#define SUPLAN_MAX_PERSISTENT_PEERS 8
#endif
#ifndef SUPLAN_MAX_TOTAL_ACL_ENTRIES
#define SUPLAN_MAX_TOTAL_ACL_ENTRIES 16
#endif
#ifndef SUPLAN_MAX_ACTIVE_SESSIONS
#define SUPLAN_MAX_ACTIVE_SESSIONS 4
#endif
#ifndef SUPLAN_MAX_PENDING_HANDSHAKES
#define SUPLAN_MAX_PENDING_HANDSHAKES 2
#endif
#ifndef SUPLAN_MAX_OUTSTANDING_LOCATES
#define SUPLAN_MAX_OUTSTANDING_LOCATES 2
#endif
#ifndef SUPLAN_MAX_RUNTIME_INTERESTS
#define SUPLAN_MAX_RUNTIME_INTERESTS 8
#endif
#ifndef SUPLAN_MAX_REASSEMBLY_SLOTS
#define SUPLAN_MAX_REASSEMBLY_SLOTS 1
#endif
#ifndef SUPLAN_RX_MAX_REASSEMBLED_FRAME
#define SUPLAN_RX_MAX_REASSEMBLED_FRAME 1024
#endif
#ifndef SUPLAN_MAX_RETRY_SLOTS
#define SUPLAN_MAX_RETRY_SLOTS 2
#endif
#ifndef SUPLAN_MAX_RETRY_FRAME_BYTES
#define SUPLAN_MAX_RETRY_FRAME_BYTES 256
#endif
#ifndef SUPLAN_MAX_DEFERRED_APP_EVENTS
#define SUPLAN_MAX_DEFERRED_APP_EVENTS 4
#endif
#ifndef SUPLAN_MAX_APPLICATION_BYTES
#define SUPLAN_MAX_APPLICATION_BYTES 768
#endif
#ifndef SUPLAN_MAX_REASSEMBLED_FRAME
#define SUPLAN_MAX_REASSEMBLED_FRAME 1024
#endif

namespace Supla {
namespace SupLan {

static const uint8_t kVersion = 1;
static const uint8_t kPeerContextSize = 27;
static const uint8_t kSha256Size = 32;
static const uint8_t kKeySize = 32;
static const uint8_t kPeerLocatorSize = 16;
static const uint8_t kNonceSize = 16;
static const uint8_t kAeadTagSize = 16;
static const uint8_t kAeadNonceSize = 12;
static const uint8_t kNoncePrefixSize = 8;
static const uint8_t kProtectedHeaderSize = 16;
static const uint8_t kApplicationHeaderSize = 6;
static const uint8_t kResourceHeaderSize = 5;

static const uint8_t kFrameLocate = 1;
static const uint8_t kFrameLocateReply = 2;
static const uint8_t kFrameSessionInit = 3;
static const uint8_t kFrameSessionAccept = 4;
static const uint8_t kFrameData = 5;

static const uint8_t kMessageClassSuplaCall = 1;
static const uint8_t kMessageClassNative = 2;
static const uint8_t kResourceTypeChannel = 1;
static const uint8_t kAckRequired = 1;
static const uint8_t kNativeAck = 1;
static const uint8_t kNativeSessionReject = 2;
static const uint8_t kNativeReadResource = 3;
static const uint8_t kChannelNumberUnresolved = 0xFF;
static const uint8_t kSessionRejectResourceLimit = 1;

static const uint8_t kPermissionRead = 1 << 0;
static const uint8_t kPermissionControl = 1 << 1;
static const uint8_t kPermissionAction = 1 << 2;

static const uint32_t kLocateReplyWindowMs = 250;
static const uint32_t kSessionInitRetryMs = 250;
static const uint8_t kSessionInitMaxAttempts = 3;
static const uint32_t kAckRetryMs = 150;
static const uint8_t kAckMaxAttempts = 3;
static const uint32_t kReassemblyTimeoutMs = 1000;
static const uint8_t kMaxDatagramsPerIterate = 4;
static const uint32_t kPendingHandshakeTimeoutMs = 3000;
static const uint32_t kRuntimeInterestTimeoutMs = 300000;
// Action Trigger call 700 is part of the PoC payload subset from SUPLA v16.
static const uint8_t kMinimumSuplaProtoVersion = 16;

}  // namespace SupLan
}  // namespace Supla

#endif  // SRC_SUPLAN_SUPLAN_CONFIG_H_

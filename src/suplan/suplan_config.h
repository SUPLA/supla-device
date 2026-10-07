// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLAN_SUPLAN_CONFIG_H_
#define SRC_SUPLAN_SUPLAN_CONFIG_H_

#ifndef ARDUINO_ARCH_AVR

#include <stdint.h>

// Build-selected public-v1 profiles. The default preserves existing constrained
// builds. ESP32 defaults to Standard; gateways select Large. No input growth.
#define SUPLAN_PROFILE_CONSTRAINED 1
#define SUPLAN_PROFILE_STANDARD 2
#define SUPLAN_PROFILE_LARGE 3
#ifndef SUPLAN_PROFILE
#if defined(ESP32) || defined(SUPLA_DEVICE_ESP32) || defined(ESP_PLATFORM)
#define SUPLAN_PROFILE SUPLAN_PROFILE_STANDARD
#else
#define SUPLAN_PROFILE SUPLAN_PROFILE_CONSTRAINED
#endif
#endif
#if SUPLAN_PROFILE == SUPLAN_PROFILE_LARGE
#define SUPLAN_PROFILE_PEERS 128
#define SUPLAN_PROFILE_ENTRIES 256
#define SUPLAN_PROFILE_SESSIONS 16
#define SUPLAN_PROFILE_INTERESTS 128
#elif SUPLAN_PROFILE == SUPLAN_PROFILE_STANDARD
#define SUPLAN_PROFILE_PEERS 32
#define SUPLAN_PROFILE_ENTRIES 64
#define SUPLAN_PROFILE_SESSIONS 8
#define SUPLAN_PROFILE_INTERESTS 32
#elif SUPLAN_PROFILE == SUPLAN_PROFILE_CONSTRAINED
#define SUPLAN_PROFILE_PEERS 8
#define SUPLAN_PROFILE_ENTRIES 16
#define SUPLAN_PROFILE_SESSIONS 4
#define SUPLAN_PROFILE_INTERESTS 8
#else
#error Invalid SUPLAN_PROFILE
#endif
#ifndef SUPLAN_MAX_PERSISTENT_PEERS
#define SUPLAN_MAX_PERSISTENT_PEERS SUPLAN_PROFILE_PEERS
#endif
#ifndef SUPLAN_MAX_TOTAL_ACL_ENTRIES
#define SUPLAN_MAX_TOTAL_ACL_ENTRIES SUPLAN_PROFILE_ENTRIES
#endif
#ifndef SUPLAN_MAX_TOTAL_EXPECTED_ENTRIES
#define SUPLAN_MAX_TOTAL_EXPECTED_ENTRIES SUPLAN_MAX_TOTAL_ACL_ENTRIES
#endif
#ifndef SUPLAN_MAX_ACTIVE_SESSIONS
#define SUPLAN_MAX_ACTIVE_SESSIONS SUPLAN_PROFILE_SESSIONS
#endif
#ifndef SUPLAN_MAX_PENDING_HANDSHAKES
#define SUPLAN_MAX_PENDING_HANDSHAKES 2
#endif
#ifndef SUPLAN_MAX_OUTSTANDING_LOCATES
#define SUPLAN_MAX_OUTSTANDING_LOCATES 2
#endif
#ifndef SUPLAN_MAX_RUNTIME_INTERESTS
#define SUPLAN_MAX_RUNTIME_INTERESTS SUPLAN_PROFILE_INTERESTS
#endif
#ifndef SUPLAN_MAX_READ_DEPENDENCIES
#define SUPLAN_MAX_READ_DEPENDENCIES SUPLAN_MAX_TOTAL_ACL_ENTRIES
#endif
#ifndef SUPLAN_MAX_PEER_RECOVERY_STATES
#define SUPLAN_MAX_PEER_RECOVERY_STATES SUPLAN_MAX_PERSISTENT_PEERS
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
// READ=43, CONTROL=60, ACTION=58 protected bytes. ACTION also needs five
// bytes of local deadline metadata. Extended STATE is never acknowledged.
#define SUPLAN_MAX_RETRY_FRAME_BYTES 64
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
static const uint8_t kResourceTypeDevice = 2;
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
static const uint32_t kActionDeliveryLifetimeMs = 3000;
static const uint32_t kReadStateResponseTimeoutMs = 500;
static const uint32_t kReassemblyTimeoutMs = 1000;
static const uint8_t kMaxDatagramsPerIterate = 4;
static const uint32_t kPendingHandshakeTimeoutMs = 3000;
static const uint32_t kRuntimeInterestTimeoutMs = 300000;
// Action Trigger call 700 is part of the PoC payload subset from SUPLA v16.
static const uint8_t kMinimumSuplaProtoVersion = 16;
static_assert(SUPLAN_MAX_PERSISTENT_PEERS <= 128,
              "Stable slots use an 8-bit index with 255 reserved");
static_assert(SUPLAN_MAX_TOTAL_ACL_ENTRIES +
                  SUPLAN_MAX_TOTAL_EXPECTED_ENTRIES <= UINT16_MAX,
              "ACL and Expected totals use 16-bit counters");
static_assert(SUPLAN_MAX_PEER_RECOVERY_STATES >= SUPLAN_MAX_PERSISTENT_PEERS,
              "Recovery uses one bounded state per persistent peer");
// Wire requirement, independent of either endpoint's compile-time profile.
static const uint16_t kMinimumRxMaxReassembledFrame = 1024;
static_assert(SUPLAN_RX_MAX_REASSEMBLED_FRAME >=
                  kMinimumRxMaxReassembledFrame &&
                  SUPLAN_RX_MAX_REASSEMBLED_FRAME <= UINT16_MAX,
              "Local RX profile must satisfy the v1 fragmentation minimum");
static_assert(SUPLAN_RX_MAX_REASSEMBLED_FRAME % 8 == 0,
              "Reassembly bitmap requires a whole number of bytes");

}  // namespace SupLan
}  // namespace Supla

#endif  // !ARDUINO_ARCH_AVR

#endif  // SRC_SUPLAN_SUPLAN_CONFIG_H_

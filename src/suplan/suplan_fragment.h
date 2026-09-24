// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLAN_SUPLAN_FRAGMENT_H_
#define SRC_SUPLAN_SUPLAN_FRAGMENT_H_

#include <stddef.h>
#include <stdint.h>

#include "suplan_types.h"

namespace Supla {
namespace SupLan {

// The spec names the adaptation values but does not assign their wire bytes.
// These PoC1 values were selected with the spec owner: FULL=0, FRAGMENT=1.
static const uint8_t kAdaptationFull = 0;
static const uint8_t kAdaptationFragment = 1;
static const size_t kFragmentHeaderSize = 9;
// PoC1 keeps one small datagram workspace. IPv4 UDP can carry larger payloads,
// but those are split by this existing adaptation layer.
static const size_t kMaxDatagramPayload = 250;

typedef bool (*DatagramSendFn)(void *context, const uint8_t *datagram,
                               size_t length);
typedef bool (*SessionIdKnownFn)(void *context, uint64_t sessionId);

struct AdaptedFrameView {
  const uint8_t *data;
  size_t length;
};

enum AdaptationResult : uint8_t {
  kAdaptationDropped = 0,
  kAdaptationPending = 1,
  kAdaptationComplete = 2,
  kAdaptationMalformed = 3,
  kAdaptationCapacityExceeded = 4,
};

class FragmentSender {
 public:
  FragmentSender();
  bool send(const uint8_t *protectedFrame, size_t frameLength,
            size_t maxDatagramPayload, uint32_t frameId,
            DatagramSendFn sendFn, void *sendContext);
  uint32_t fragmentCount() const;

 private:
  uint32_t fragmentCount_;
};

class FragmentReassembler {
 public:
  FragmentReassembler();
  AdaptationResult accept(const Endpoint &source, const uint8_t *datagram,
                          size_t datagramLength, uint32_t nowMs,
                          uint32_t timeoutMs, SessionIdKnownFn sessionKnown,
                          void *sessionContext, AdaptedFrameView *frame);
  bool expire(uint32_t nowMs, uint32_t timeoutMs);
  bool active() const;
  uint16_t receivedBytes() const;
  uint32_t rejectedCount() const;

 private:
  bool sameSource(const Endpoint &source) const;
  bool hasPrefix(size_t length) const;
  void clear();
  void markReceived(uint16_t offset, uint16_t length);

  bool active_;
  Endpoint source_;
  uint32_t frameId_;
  uint16_t totalLength_;
  uint16_t receivedBytes_;
  uint32_t lastActivityMs_;
  uint8_t frame_[SUPLAN_RX_MAX_REASSEMBLED_FRAME];
  uint8_t received_[SUPLAN_RX_MAX_REASSEMBLED_FRAME / 8];
  uint32_t rejectedCount_;
};

}  // namespace SupLan
}  // namespace Supla

#endif  // SRC_SUPLAN_SUPLAN_FRAGMENT_H_

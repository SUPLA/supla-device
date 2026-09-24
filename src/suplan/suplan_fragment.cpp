// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "suplan_fragment.h"

#include <string.h>

#include "suplan_wire.h"

namespace Supla {
namespace SupLan {

static bool validProtectedFrame(const uint8_t *frame, size_t length) {
  if (frame == nullptr || length < kProtectedHeaderSize +
          kApplicationHeaderSize + kAeadTagSize ||
      frame[0] != kVersion || frame[1] != kFrameData) {
    return false;
  }
  const uint16_t cipherLength = getUint16(frame + 14);
  return cipherLength >= kApplicationHeaderSize &&
      length == kProtectedHeaderSize + static_cast<size_t>(cipherLength) +
          kAeadTagSize &&
      length <= SUPLAN_RX_MAX_REASSEMBLED_FRAME;
}

FragmentSender::FragmentSender() : fragmentCount_(0) {}

bool FragmentSender::send(const uint8_t *protectedFrame, size_t frameLength,
                          size_t maxDatagramPayload, uint32_t frameId,
                          DatagramSendFn sendFn, void *sendContext) {
  if (sendFn == nullptr || !validProtectedFrame(protectedFrame, frameLength) ||
      maxDatagramPayload < 1 || maxDatagramPayload > kMaxDatagramPayload) {
    return false;
  }
  fragmentCount_ = 0;
  // Datagram callbacks consume bytes synchronously, so keep only one bounded
  // scratch copy on the call stack instead of in every FragmentSender.
  uint8_t datagram[kMaxDatagramPayload];
  if (frameLength + 1 <= maxDatagramPayload) {
    datagram[0] = kAdaptationFull;
    memcpy(datagram + 1, protectedFrame, frameLength);
    fragmentCount_ = 1;
    return sendFn(sendContext, datagram, frameLength + 1);
  }
  if (maxDatagramPayload <= kFragmentHeaderSize) {
    return false;
  }
  const size_t chunkCapacity = maxDatagramPayload - kFragmentHeaderSize;
  size_t offset = 0;
  while (offset < frameLength) {
    const size_t remaining = frameLength - offset;
    const size_t chunkLength = remaining < chunkCapacity
        ? remaining : chunkCapacity;
    datagram[0] = kAdaptationFragment;
    putUint32(datagram + 1, frameId);
    putUint16(datagram + 5, static_cast<uint16_t>(frameLength));
    putUint16(datagram + 7, static_cast<uint16_t>(offset));
    memcpy(datagram + kFragmentHeaderSize, protectedFrame + offset,
           chunkLength);
    if (!sendFn(sendContext, datagram, kFragmentHeaderSize + chunkLength)) {
      return false;
    }
    ++fragmentCount_;
    offset += chunkLength;
  }
  return true;
}

uint32_t FragmentSender::fragmentCount() const {
  return fragmentCount_;
}

FragmentReassembler::FragmentReassembler()
    : active_(false),
      source_(),
      frameId_(0),
      totalLength_(0),
      receivedBytes_(0),
      lastActivityMs_(0),
      frame_(),
      received_(),
      rejectedCount_(0)
{}

bool FragmentReassembler::sameSource(const Endpoint &source) const {
  return source_.address == source.address && source_.port == source.port;
}

bool FragmentReassembler::hasPrefix(size_t length) const {
  for (size_t i = 0; i < length; ++i) {
    if ((received_[i >> 3] & static_cast<uint8_t>(1U << (i & 7))) == 0) {
      return false;
    }
  }
  return true;
}

void FragmentReassembler::clear() {
  active_ = false;
  source_ = Endpoint();
  frameId_ = 0;
  totalLength_ = 0;
  receivedBytes_ = 0;
  lastActivityMs_ = 0;
  memset(received_, 0, sizeof(received_));
}

void FragmentReassembler::markReceived(uint16_t offset, uint16_t length) {
  for (uint16_t i = 0; i < length; ++i) {
    const uint16_t index = static_cast<uint16_t>(offset + i);
    received_[index >> 3] |= static_cast<uint8_t>(1U << (index & 7));
  }
  receivedBytes_ = static_cast<uint16_t>(receivedBytes_ + length);
}

bool FragmentReassembler::expire(uint32_t nowMs, uint32_t timeoutMs) {
  if (active_ && static_cast<uint32_t>(nowMs - lastActivityMs_) >= timeoutMs) {
    clear();
    return true;
  }
  return false;
}

AdaptationResult FragmentReassembler::accept(
    const Endpoint &source, const uint8_t *datagram, size_t datagramLength,
    uint32_t nowMs, uint32_t timeoutMs, SessionIdKnownFn sessionKnown,
    void *sessionContext, AdaptedFrameView *frame) {
  if (frame == nullptr || datagram == nullptr || datagramLength == 0) {
    ++rejectedCount_;
    return kAdaptationMalformed;
  }
  frame->data = nullptr;
  frame->length = 0;
  expire(nowMs, timeoutMs);
  if (datagram[0] == kAdaptationFull) {
    const uint8_t *complete = datagram + 1;
    const size_t completeLength = datagramLength - 1;
    if (!validProtectedFrame(complete, completeLength)) {
      ++rejectedCount_;
      return kAdaptationMalformed;
    }
    frame->data = complete;
    frame->length = completeLength;
    return kAdaptationComplete;
  }
  if (datagram[0] != kAdaptationFragment ||
      datagramLength <= kFragmentHeaderSize) {
    ++rejectedCount_;
    return kAdaptationMalformed;
  }

  const uint32_t incomingFrameId = getUint32(datagram + 1);
  const uint16_t incomingTotal = getUint16(datagram + 5);
  const uint16_t offset = getUint16(datagram + 7);
  const size_t dataLength = datagramLength - kFragmentHeaderSize;
  if (incomingTotal < kProtectedHeaderSize + kApplicationHeaderSize +
          kAeadTagSize || incomingTotal > sizeof(frame_) ||
      offset >= incomingTotal ||
      dataLength > static_cast<size_t>(incomingTotal - offset)) {
    ++rejectedCount_;
    return kAdaptationMalformed;
  }

  if (!active_) {
    if (offset != 0) {
      ++rejectedCount_;
      return kAdaptationDropped;
    }
    active_ = true;
    source_ = source;
    frameId_ = incomingFrameId;
    totalLength_ = incomingTotal;
    receivedBytes_ = 0;
    lastActivityMs_ = nowMs;
    memset(received_, 0, sizeof(received_));
  } else if (!sameSource(source) || frameId_ != incomingFrameId) {
    return kAdaptationCapacityExceeded;
  } else if (totalLength_ != incomingTotal) {
    clear();
    ++rejectedCount_;
    return kAdaptationMalformed;
  }

  bool exactDuplicate = true;
  bool overlap = false;
  for (size_t i = 0; i < dataLength; ++i) {
    const uint16_t index = static_cast<uint16_t>(offset + i);
    const bool alreadyReceived =
        (received_[index >> 3] & static_cast<uint8_t>(1U << (index & 7))) != 0;
    if (alreadyReceived) {
      overlap = true;
      if (frame_[index] != datagram[kFragmentHeaderSize + i]) {
        clear();
        ++rejectedCount_;
        return kAdaptationMalformed;
      }
    } else {
      exactDuplicate = false;
    }
  }
  if (overlap && !exactDuplicate) {
    clear();
    ++rejectedCount_;
    return kAdaptationMalformed;
  }
  if (!exactDuplicate) {
    memcpy(frame_ + offset, datagram + kFragmentHeaderSize, dataLength);
    markReceived(offset, static_cast<uint16_t>(dataLength));
    lastActivityMs_ = nowMs;
  }

  if (hasPrefix(kProtectedHeaderSize)) {
    if (frame_[0] != kVersion || frame_[1] != kFrameData ||
        getUint16(frame_ + 14) + kProtectedHeaderSize + kAeadTagSize !=
            totalLength_ ||
        getUint16(frame_ + 14) < kApplicationHeaderSize ||
        (sessionKnown != nullptr &&
         !sessionKnown(sessionContext, getUint64(frame_ + 2)))) {
      clear();
      ++rejectedCount_;
      return kAdaptationDropped;
    }
  }
  if (receivedBytes_ != totalLength_) {
    return kAdaptationPending;
  }
  if (!validProtectedFrame(frame_, totalLength_)) {
    clear();
    ++rejectedCount_;
    return kAdaptationMalformed;
  }
  frame->data = frame_;
  frame->length = totalLength_;
  clear();
  return kAdaptationComplete;
}

bool FragmentReassembler::active() const {
  return active_;
}

uint16_t FragmentReassembler::receivedBytes() const {
  return receivedBytes_;
}

uint32_t FragmentReassembler::rejectedCount() const {
  return rejectedCount_;
}

}  // namespace SupLan
}  // namespace Supla

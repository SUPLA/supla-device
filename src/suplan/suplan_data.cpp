// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "suplan_data.h"

#include <string.h>

#include "suplan_wire.h"

namespace Supla {
namespace SupLan {

ReplayWindow::ReplayWindow()
    : initialized_(false), highestSequence_(0), receivedBitmap_(0) {}

ReplayClassification ReplayWindow::classify(uint32_t sequence) const {
  if (!initialized_ || sequence > highestSequence_) {
    return kReplayUnseen;
  }
  const uint32_t distance = highestSequence_ - sequence;
  if (distance >= 64) {
    return kReplayTooOld;
  }
  return (receivedBitmap_ & (UINT64_C(1) << distance)) != 0
      ? kReplayDuplicate : kReplayUnseen;
}

bool ReplayWindow::commit(uint32_t sequence) {
  const ReplayClassification state = classify(sequence);
  if (state != kReplayUnseen) {
    return false;
  }
  if (!initialized_) {
    initialized_ = true;
    highestSequence_ = sequence;
    receivedBitmap_ = 1;
    return true;
  }
  if (sequence > highestSequence_) {
    const uint32_t distance = sequence - highestSequence_;
    receivedBitmap_ = distance >= 64
        ? UINT64_C(1) : (receivedBitmap_ << distance) | UINT64_C(1);
    highestSequence_ = sequence;
  } else {
    const uint32_t distance = highestSequence_ - sequence;
    receivedBitmap_ |= UINT64_C(1) << distance;
  }
  return true;
}

void ReplayWindow::reset() {
  initialized_ = false;
  highestSequence_ = 0;
  receivedBitmap_ = 0;
}

bool ReplayWindow::initialized() const {
  return initialized_;
}

uint32_t ReplayWindow::highestSequence() const {
  return highestSequence_;
}

uint64_t ReplayWindow::bitmap() const {
  return receivedBitmap_;
}

bool encodeApplicationData(uint8_t messageClass, uint32_t messageType,
                           uint8_t flags, const uint8_t *body,
                           size_t bodyLength, uint8_t *output,
                           size_t outputCapacity, size_t *outputLength) {
  if (output == nullptr || outputLength == nullptr ||
      (body == nullptr && bodyLength != 0) || messageType == 0 ||
      (messageClass != kMessageClassSuplaCall &&
       messageClass != kMessageClassNative) ||
      (flags & static_cast<uint8_t>(~kAckRequired)) != 0 ||
      bodyLength > SUPLAN_MAX_APPLICATION_BYTES - kApplicationHeaderSize ||
      outputCapacity < kApplicationHeaderSize + bodyLength) {
    return false;
  }
  output[0] = messageClass;
  putUint32(output + 1, messageType);
  output[5] = flags;
  if (bodyLength != 0) {
    memcpy(output + kApplicationHeaderSize, body, bodyLength);
  }
  *outputLength = kApplicationHeaderSize + bodyLength;
  return true;
}

bool decodeApplicationData(const uint8_t *input, size_t length,
                           ApplicationDataView *view) {
  if (input == nullptr || view == nullptr || length < kApplicationHeaderSize ||
      (input[0] != kMessageClassSuplaCall &&
       input[0] != kMessageClassNative) || getUint32(input + 1) == 0 ||
      (input[5] & static_cast<uint8_t>(~kAckRequired)) != 0) {
    return false;
  }
  view->messageClass = input[0];
  view->messageType = getUint32(input + 1);
  view->flags = input[5];
  view->body = input + kApplicationHeaderSize;
  view->bodyLength = length - kApplicationHeaderSize;
  return true;
}

bool encodeResourceId(uint8_t resourceType, uint32_t resourceId,
                      uint8_t output[5]) {
  if (output == nullptr || resourceType != kResourceTypeChannel) {
    return false;
  }
  output[0] = resourceType;
  putUint32(output + 1, resourceId);
  return true;
}

bool decodeResourceData(const ApplicationDataView *application,
                        ResourceDataView *view) {
  if (application == nullptr || view == nullptr ||
      application->body == nullptr ||
      application->bodyLength < kResourceHeaderSize ||
      application->body[0] != kResourceTypeChannel) {
    return false;
  }
  view->resource.type = application->body[0];
  view->resource.id = getUint32(application->body + 1);
  view->payload = application->body + kResourceHeaderSize;
  view->payloadLength = application->bodyLength - kResourceHeaderSize;
  return true;
}

static void makeNonce(const DirectionalKeys *keys, uint32_t sequence,
                      uint8_t nonce[12]) {
  memcpy(nonce, keys->noncePrefix, kNoncePrefixSize);
  putUint32(nonce + kNoncePrefixSize, sequence);
}

bool encodeProtectedData(CryptoPort *crypto, const DirectionalKeys *keys,
                         uint64_t sessionId, uint32_t sequence,
                         const uint8_t *applicationData,
                         size_t applicationLength, uint8_t *output,
                         size_t outputCapacity, size_t *outputLength) {
  if (crypto == nullptr || keys == nullptr || output == nullptr ||
      outputLength == nullptr || applicationData == nullptr ||
      applicationLength < kApplicationHeaderSize ||
      applicationLength > UINT16_MAX ||
      applicationLength > SUPLAN_MAX_APPLICATION_BYTES ||
      outputCapacity < kProtectedHeaderSize + applicationLength +
          kAeadTagSize) {
    return false;
  }
  ApplicationDataView view = {};
  if (!decodeApplicationData(applicationData, applicationLength, &view)) {
    return false;
  }
  output[0] = kVersion;
  output[1] = kFrameData;
  putUint64(output + 2, sessionId);
  putUint32(output + 10, sequence);
  putUint16(output + 14, static_cast<uint16_t>(applicationLength));
  uint8_t nonce[kAeadNonceSize];
  makeNonce(keys, sequence, nonce);
  const bool encrypted = crypto->aes128CcmEncrypt(
      keys->trafficKey, nonce, output, kProtectedHeaderSize,
      applicationData, applicationLength, output + kProtectedHeaderSize,
      output + kProtectedHeaderSize + applicationLength);
  memset(nonce, 0, sizeof(nonce));
  if (!encrypted) {
    return false;
  }
  *outputLength = kProtectedHeaderSize + applicationLength + kAeadTagSize;
  return true;
}

ProtectedDataResult decodeProtectedData(
    CryptoPort *crypto, const DirectionalKeys *keys, const uint8_t *frame,
    size_t frameLength, ReplayWindow *replay, uint8_t *plaintext,
    size_t plaintextCapacity, uint64_t *sessionId, uint32_t *sequence,
    size_t *plaintextLength) {
  if (crypto == nullptr || keys == nullptr || frame == nullptr ||
      replay == nullptr || plaintext == nullptr || sessionId == nullptr ||
      sequence == nullptr || plaintextLength == nullptr ||
      frameLength < kProtectedHeaderSize + kApplicationHeaderSize +
          kAeadTagSize || frame[0] != kVersion || frame[1] != kFrameData) {
    return kProtectedDataMalformed;
  }
  const uint16_t cipherLength = getUint16(frame + 14);
  const size_t expectedLength = kProtectedHeaderSize +
      static_cast<size_t>(cipherLength) + kAeadTagSize;
  if (cipherLength < kApplicationHeaderSize ||
      cipherLength > SUPLAN_MAX_APPLICATION_BYTES ||
      expectedLength > SUPLAN_RX_MAX_REASSEMBLED_FRAME ||
      frameLength != expectedLength) {
    return kProtectedDataMalformed;
  }
  if (cipherLength > plaintextCapacity) {
    return kProtectedDataCapacityExceeded;
  }
  const uint32_t incomingSequence = getUint32(frame + 10);
  const ReplayClassification candidate = replay->classify(incomingSequence);
  if (candidate == kReplayTooOld) {
    return kProtectedDataTooOld;
  }
  uint8_t nonce[kAeadNonceSize];
  makeNonce(keys, incomingSequence, nonce);
  const uint8_t *tag = frame + kProtectedHeaderSize + cipherLength;
  const bool authenticated = crypto->aes128CcmDecrypt(
      keys->trafficKey, nonce, frame, kProtectedHeaderSize,
      frame + kProtectedHeaderSize, cipherLength, tag, plaintext);
  memset(nonce, 0, sizeof(nonce));
  if (!authenticated) {
    memset(plaintext, 0, cipherLength);
    return kProtectedDataAuthenticationFailed;
  }
  if (candidate == kReplayDuplicate) {
    *sessionId = getUint64(frame + 2);
    *sequence = incomingSequence;
    *plaintextLength = cipherLength;
    return kProtectedDataDuplicate;
  }
  if (!replay->commit(incomingSequence)) {
    memset(plaintext, 0, cipherLength);
    return kProtectedDataTooOld;
  }
  *sessionId = getUint64(frame + 2);
  *sequence = incomingSequence;
  *plaintextLength = cipherLength;
  ApplicationDataView app = {};
  if (!decodeApplicationData(plaintext, cipherLength, &app)) {
    memset(plaintext, 0, cipherLength);
    return kProtectedDataMalformed;
  }
  return kProtectedDataOk;
}

}  // namespace SupLan
}  // namespace Supla

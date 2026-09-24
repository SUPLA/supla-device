// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLAN_SUPLAN_DATA_H_
#define SRC_SUPLAN_SUPLAN_DATA_H_

#include <stddef.h>
#include <stdint.h>

#include "suplan_ports.h"
#include "suplan_types.h"

namespace Supla {
namespace SupLan {

struct ApplicationDataView {
  uint8_t messageClass;
  uint32_t messageType;
  uint8_t flags;
  const uint8_t *body;
  size_t bodyLength;
};

struct ResourceDataView {
  ResourceId resource;
  const uint8_t *payload;
  size_t payloadLength;
};

enum ReplayClassification : uint8_t {
  kReplayUnseen = 0,
  kReplayDuplicate = 1,
  kReplayTooOld = 2,
};

class ReplayWindow {
 public:
  ReplayWindow();
  ReplayClassification classify(uint32_t sequence) const;
  bool commit(uint32_t sequence);
  void reset();
  bool initialized() const;
  uint32_t highestSequence() const;
  uint64_t bitmap() const;

 private:
  bool initialized_;
  uint32_t highestSequence_;
  uint64_t receivedBitmap_;
};

enum ProtectedDataResult : uint8_t {
  kProtectedDataOk = 0,
  kProtectedDataDuplicate = 1,
  kProtectedDataMalformed = 2,
  kProtectedDataTooOld = 3,
  kProtectedDataAuthenticationFailed = 4,
  kProtectedDataCapacityExceeded = 5,
};

bool encodeApplicationData(uint8_t messageClass, uint32_t messageType,
                           uint8_t flags, const uint8_t *body,
                           size_t bodyLength, uint8_t *output,
                           size_t outputCapacity, size_t *outputLength);
bool decodeApplicationData(const uint8_t *input, size_t length,
                           ApplicationDataView *view);
bool encodeResourceId(uint8_t resourceType, uint32_t resourceId,
                      uint8_t output[5]);
bool decodeResourceData(const ApplicationDataView *application,
                        ResourceDataView *view);

bool encodeProtectedData(CryptoPort *crypto, const DirectionalKeys *keys,
                         uint64_t sessionId, uint32_t sequence,
                         const uint8_t *applicationData,
                         size_t applicationLength, uint8_t *output,
                         size_t outputCapacity, size_t *outputLength);
ProtectedDataResult decodeProtectedData(
    CryptoPort *crypto, const DirectionalKeys *keys, const uint8_t *frame,
    size_t frameLength, ReplayWindow *replay, uint8_t *plaintext,
    size_t plaintextCapacity, uint64_t *sessionId, uint32_t *sequence,
    size_t *plaintextLength);

}  // namespace SupLan
}  // namespace Supla

#endif  // SRC_SUPLAN_SUPLAN_DATA_H_

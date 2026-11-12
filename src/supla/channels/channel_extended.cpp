// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "channel_extended.h"

#include <string.h>

#include <supla-common/proto.h>
#include <supla/log_wrapper.h>

namespace Supla {
namespace {

size_t utf8_sequence_size(unsigned char firstByte) {
  if (firstByte <= 0x7f) {
    return 1;
  }
  if (firstByte >= 0xc2 && firstByte <= 0xdf) {
    return 2;
  }
  if (firstByte >= 0xe0 && firstByte <= 0xef) {
    return 3;
  }
  if (firstByte >= 0xf0 && firstByte <= 0xf4) {
    return 4;
  }
  return 0;
}

bool is_valid_utf8_sequence(const char *text, size_t offset, size_t size) {
  const unsigned char firstByte =
      static_cast<unsigned char>(text[offset]);
  const unsigned char secondByte =
      static_cast<unsigned char>(text[offset + 1]);

  if ((firstByte == 0xe0 && secondByte < 0xa0) ||
      (firstByte == 0xed && secondByte >= 0xa0) ||
      (firstByte == 0xf0 && secondByte < 0x90) ||
      (firstByte == 0xf4 && secondByte > 0x8f)) {
    return false;
  }

  for (size_t i = 1; i < size; i++) {
    const unsigned char continuation =
        static_cast<unsigned char>(text[offset + i]);
    if (continuation < 0x80 || continuation > 0xbf) {
      return false;
    }
  }

  return true;
}

size_t utf8_prefix_size(const char *text, size_t maxSize,
                        bool *isValidUtf8) {
  const size_t textSize = strnlen(text, maxSize + 1);
  size_t prefixSize = 0;
  *isValidUtf8 = true;

  while (prefixSize < textSize && prefixSize < maxSize) {
    const unsigned char firstByte =
        static_cast<unsigned char>(text[prefixSize]);
    const size_t sequenceSize = utf8_sequence_size(firstByte);
    if (sequenceSize == 0) {
      *isValidUtf8 = false;
      return prefixSize;
    }

    if (prefixSize + sequenceSize > maxSize) {
      return prefixSize;
    }
    if (prefixSize + sequenceSize > textSize) {
      *isValidUtf8 = false;
      return prefixSize;
    }
    if (sequenceSize > 1 &&
        !is_valid_utf8_sequence(text, prefixSize, sequenceSize)) {
      *isValidUtf8 = false;
      return prefixSize;
    }

    prefixSize += sequenceSize;
  }

  return prefixSize;
}

}  // namespace

bool ChannelExtended::isExtended() const {
  return true;
}

TSuplaChannelExtendedValue *ChannelExtended::getExtValue() {
  return &extValue;
}

bool ChannelExtended::setNewTextValue(const char *text) {
  if (text == nullptr) {
    text = "";
  }

  bool isValidUtf8 = true;
  const size_t textSize = utf8_prefix_size(
      text, SUPLA_GENERAL_PURPOSE_TEXT_MAX_SIZE, &isValidUtf8);
  if (!isValidUtf8) {
    SUPLA_LOG_WARNING("General purpose text contains invalid UTF-8");
  }

  const size_t payloadSize =
      SUPLA_GENERAL_PURPOSE_TEXT_PAYLOAD_HEADER_SIZE + textSize;
  if (extValue.type == EV_TYPE_GENERAL_PURPOSE_TEXT &&
      extValue.size == payloadSize &&
      (textSize == 0 ||
       memcmp(extValue.value + SUPLA_GENERAL_PURPOSE_TEXT_PAYLOAD_HEADER_SIZE,
              text, textSize) == 0)) {
    return false;
  }

  extValue.type = EV_TYPE_GENERAL_PURPOSE_TEXT;
  extValue.size = static_cast<unsigned int>(payloadSize);
  memset(extValue.value, 0, SUPLA_GENERAL_PURPOSE_TEXT_PAYLOAD_HEADER_SIZE);
  if (textSize > 0) {
    memmove(extValue.value + SUPLA_GENERAL_PURPOSE_TEXT_PAYLOAD_HEADER_SIZE,
            text, textSize);
  }
  if (payloadSize < sizeof(extValue.value)) {
    memset(extValue.value + payloadSize, 0,
           sizeof(extValue.value) - payloadSize);
  }

  // Reuse the channel's existing 64-bit value as the text sequence counter.
  setNewValue(getValueInt64() + 1);
  return true;
}

bool ChannelExtended::refreshTextValue() {
  if (extValue.type != EV_TYPE_GENERAL_PURPOSE_TEXT) {
    return false;
  }

  setNewValue(getValueInt64() + 1);
  return true;
}

};  // namespace Supla

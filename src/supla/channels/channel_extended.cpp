// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "channel_extended.h"

#include <string.h>

#include <supla-common/proto.h>

namespace Supla {
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

  size_t size = strnlen(text, SUPLA_GENERAL_PURPOSE_TEXT_MAX_SIZE);
  auto textSize = static_cast<unsigned int>(size);
  if (extValue.type == EV_TYPE_GENERAL_PURPOSE_TEXT &&
      extValue.size == textSize &&
      (size == 0 || memcmp(extValue.value, text, size) == 0)) {
    return false;
  }

  extValue.type = EV_TYPE_GENERAL_PURPOSE_TEXT;
  extValue.size = textSize;
  if (size > 0) {
    memmove(extValue.value, text, size);
  }
  if (size < sizeof(extValue.value)) {
    memset(extValue.value + size, 0, sizeof(extValue.value) - size);
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

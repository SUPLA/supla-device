// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "general_purpose_text.h"

#include <supla-common/proto.h>
#include <supla/time.h>

namespace Supla {
namespace Sensor {
namespace {

constexpr uint32_t kDefaultTextRefreshIntervalMs = 5000;

}  // namespace

GeneralPurposeText::GeneralPurposeText() {
  channel.setType(SUPLA_CHANNELTYPE_GENERAL_PURPOSE_TEXT);
  channel.setDefaultFunction(SUPLA_CHANNELFNC_GENERAL_PURPOSE_TEXT);
}

void GeneralPurposeText::setValue(const char *text) {
  bool changed = channel.setNewTextValue(text);
  if (changed || !hasTextValue) {
    lastReadTime = millis();
  }
  hasTextValue = true;
}

Channel *GeneralPurposeText::getChannel() {
  return &channel;
}

const Channel *GeneralPurposeText::getChannel() const {
  return &channel;
}

void GeneralPurposeText::iterateAlways() {
  if (!hasTextValue) {
    return;
  }

  uint32_t now = millis();
  if (now - lastReadTime < kDefaultTextRefreshIntervalMs) {
    return;
  }

  if (channel.refreshTextValue()) {
    lastReadTime = now;
  }
}

ApplyConfigResult GeneralPurposeText::applyChannelConfig(
    TSD_ChannelConfig *newConfig, bool local) {
  (void)local;
  (void)newConfig;
  return ApplyConfigResult::NotSupported;
}

void GeneralPurposeText::fillChannelConfig(void *channelConfig, int *size,
                                           uint8_t configType) {
  (void)channelConfig;
  (void)configType;
  if (size != nullptr) {
    *size = 0;
  }
}

};  // namespace Sensor
};  // namespace Supla

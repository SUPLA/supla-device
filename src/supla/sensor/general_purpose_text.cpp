/*
   Copyright (C) AC SOFTWARE SP. Z O.O

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
   */

#include "general_purpose_text.h"

#include <string.h>
#include <supla-common/proto.h>
#include <supla/log_wrapper.h>

namespace Supla {
namespace Sensor {

GeneralPurposeText::GeneralPurposeText() {
  channel.setType(SUPLA_CHANNELTYPE_GENERAL_PURPOSE_TEXT);
  channel.setDefault(SUPLA_CHANNELFNC_GENERAL_PURPOSE_TEXT);
}

void GeneralPurposeText::setValue(const char *text) {
  if (text == nullptr) {
    text = "";
  }
  size_t len = strnlen(text, SUPLA_GENERAL_PURPOSE_TEXT_MAX_SIZE);
  bool same = (strncmp(this->text, text, SUPLA_GENERAL_PURPOSE_TEXT_MAX_SIZE) == 0);
  if (!same) {
    memset(this->text, 0, sizeof(this->text));
    memcpy(this->text, text, len);
    valueChanged = true;
  }
}

Channel *GeneralPurposeText::getChannel() {
  return &channel;
}

const Channel *GeneralPurposeText::getChannel() const {
  return &channel;
}

void GeneralPurposeText::onInit() {
  channel.setType(SUPLA_CHANNELTYPE_GENERAL_PURPOSE_TEXT);
  channel.setDefault(SUPLA_CHANNELFNC_GENERAL_PURPOSE_TEXT);
}

void GeneralPurposeText::iterateAlways() {
  if (!valueChanged) {
    return;
  }
  valueChanged = false;

  // Build extended value carrying the text
  TSuplaChannelExtendedValue ev = {};
  ev.type = EV_TYPE_GENERAL_PURPOSE_TEXT;
  size_t len = strnlen(text, SUPLA_GENERAL_PURPOSE_TEXT_MAX_SIZE);
  ev.size = static_cast<unsigned int>(len);
  if (len > 0) {
    memcpy(ev.value, text, len);
  }
  // Write directly into the ChannelExtended's extValue field.
  auto *dest = channel.getExtValue();
  if (dest) {
    *dest = ev;
  }

  // Increment a persistent 64-bit sequence counter stored in the 8-byte
  // channel value so the server always sees a changed value and sends it.
  ++seqCounter;
  channel.setNewValue(reinterpret_cast<const char *>(&seqCounter));
}

ApplyConfigResult GeneralPurposeText::applyChannelConfig(
    TSD_ChannelConfig *newConfig, bool local) {
  (void)local;
  if (!newConfig) {
    return ApplyConfigResult::NotSupported;
  }
  if (newConfig->ConfigType != SUPLA_CONFIG_TYPE_DEFAULT) {
    return ApplyConfigResult::NotSupported;
  }
  if (newConfig->ConfigSize < sizeof(TChannelConfig_GeneralPurposeText)) {
    SUPLA_LOG_WARNING("GPT[%d]: config too short", getChannelNumber());
    return ApplyConfigResult::NotSupported;
  }
  auto cfg = reinterpret_cast<TChannelConfig_GeneralPurposeText *>(
      newConfig->Config);
  keepHistory = cfg->KeepHistory;
  refreshIntervalMs = cfg->RefreshIntervalMs;
  SUPLA_LOG_INFO(
      "GPT[%d]: config applied: keepHistory=%u refreshIntervalMs=%u",
      getChannelNumber(), keepHistory, refreshIntervalMs);
  return ApplyConfigResult::Success;
}

};  // namespace Sensor
};  // namespace Supla

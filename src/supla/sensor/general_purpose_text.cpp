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

#include <supla-common/proto.h>
#include <supla/log_wrapper.h>
#include <supla/time.h>

namespace Supla {
namespace Sensor {

GeneralPurposeText::GeneralPurposeText()
    : GeneralPurposeChannelBase(static_cast<Channel &>(*this), nullptr,
                                false) {
  ChannelExtended::setType(SUPLA_CHANNELTYPE_GENERAL_PURPOSE_TEXT);
  ChannelExtended::setDefaultFunction(SUPLA_CHANNELFNC_GENERAL_PURPOSE_TEXT);
}

void GeneralPurposeText::setValue(const char *text) {
  bool changed = ChannelExtended::setNewTextValue(text);
  if (changed || !hasTextValue) {
    lastReadTime = millis();
  }
  hasTextValue = true;
}

Channel *GeneralPurposeText::getChannel() {
  return static_cast<ChannelExtended *>(this);
}

const Channel *GeneralPurposeText::getChannel() const {
  return static_cast<const ChannelExtended *>(this);
}

void GeneralPurposeText::onInit() {
  initDone = true;
  ChannelExtended::setType(SUPLA_CHANNELTYPE_GENERAL_PURPOSE_TEXT);
  ChannelExtended::setDefaultFunction(SUPLA_CHANNELFNC_GENERAL_PURPOSE_TEXT);
}

void GeneralPurposeText::onLoadConfig(SuplaDeviceClass *sdc) {
  (void)sdc;
}

void GeneralPurposeText::iterateAlways() {
  if (!hasTextValue) {
    return;
  }

  uint32_t now = millis();
  if (now - lastReadTime < refreshIntervalMs) {
    return;
  }

  if (ChannelExtended::refreshTextValue()) {
    lastReadTime = now;
  }
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
    SUPLA_LOG_WARNING("GPT[%d]: config too short",
                      GeneralPurposeChannelBase::getChannelNumber());
    return ApplyConfigResult::NotSupported;
  }
  auto cfg = reinterpret_cast<TChannelConfig_GeneralPurposeText *>(
      newConfig->Config);
  GeneralPurposeChannelBase::setRefreshIntervalMs(cfg->RefreshIntervalMs,
                                                  false);
  SUPLA_LOG_INFO("GPT[%d]: config applied: refreshIntervalMs=%u",
                 GeneralPurposeChannelBase::getChannelNumber(),
                 GeneralPurposeChannelBase::getRefreshIntervalMs());
  return ApplyConfigResult::Success;
}

void GeneralPurposeText::fillChannelConfig(void *channelConfig, int *size,
                                           uint8_t configType) {
  if (size == nullptr) {
    return;
  }
  *size = 0;
  if (channelConfig == nullptr ||
      configType != SUPLA_CONFIG_TYPE_DEFAULT) {
    return;
  }

  auto config =
      reinterpret_cast<TChannelConfig_GeneralPurposeText *>(channelConfig);
  *config = {};
  config->RefreshIntervalMs = GeneralPurposeChannelBase::getRefreshIntervalMs();
  *size = sizeof(*config);
}

};  // namespace Sensor
};  // namespace Supla

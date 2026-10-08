// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "channel_state.h"

#include <math.h>
#include <string.h>
#include <supla/channels/channel.h>
#include <supla/element.h>
#include <supla/sensor/general_purpose_channel_base.h>
#include <supla/sensor/thermometer.h>
#include <supla/tools.h>

static_assert(sizeof(TDS_SuplaDeviceChannel_E) == 36,
              "SupLAN Channel snapshot format changed");

namespace Supla {
bool buildChannelSnapshot(Channel *channel, TDS_SuplaDeviceChannel_E *output) {
  if (!channel || !output) return false;
  *output = {};
  output->Number = 0xff;
  output->Type = channel->getChannelType();
  output->FuncList = channel->getFuncList();
  output->Default = channel->getDefaultFunction();
  output->Flags = channel->getFlags();
  output->DefaultIcon = channel->getDefaultIcon();
  output->SubDeviceId = channel->getSubDeviceId();
  output->ValueValidityTimeSec = channel->getValidityTimeSec();
  if (channel->isStateFirmwareUpdateOngoing()) {
    output->Offline = SUPLA_CHANNEL_OFFLINE_FLAG_FIRMWARE_UPDATE_ONGOING;
  } else if (channel->isStateOfflineRemoteWakeupNotSupported()) {
    output->Offline =
        SUPLA_CHANNEL_OFFLINE_FLAG_OFFLINE_REMOTE_WAKEUP_NOT_SUPPORTED;
  } else if (channel->isStateOnlineAndNotAvailable()) {
    output->Offline = SUPLA_CHANNEL_OFFLINE_FLAG_ONLINE_BUT_NOT_AVAILABLE;
  } else {
    output->Offline = channel->isStateOnline()
                          ? SUPLA_CHANNEL_OFFLINE_FLAG_ONLINE
                          : SUPLA_CHANNEL_OFFLINE_FLAG_OFFLINE;
  }
  channel->fillRawValue(output->value);
  if (output->Type == SUPLA_CHANNELTYPE_BINARYSENSOR) {
    output->value[0] = channel->getValueBool() ? 1 : 0;
  } else if (output->Type == SUPLA_CHANNELTYPE_GENERAL_PURPOSE_MEASUREMENT ||
             output->Type == SUPLA_CHANNELTYPE_GENERAL_PURPOSE_METER) {
    auto *element =
        Element::getElementByChannelNumber(channel->getChannelNumber());
    if (!element) return false;
    auto *measurement = element->getGeneralPurposeChannel();
    if (!measurement) return false;
    const double effective = measurement->getCalculatedValue();
    if (sizeof(effective) == 8) {
      memcpy(output->value, &effective, sizeof(effective));
    } else {
      float2DoublePacked(effective, reinterpret_cast<uint8_t *>(output->value));
    }
  }
  return true;
}

bool ChannelState::available() const {
  if (local_) {
    return local_->isStateOnline() &&
           !local_->isStateOnlineAndNotAvailable() &&
           !local_->isStateFirmwareUpdateOngoing();
  }
  return snapshot_ && usable_ &&
         snapshot_->Offline == SUPLA_CHANNEL_OFFLINE_FLAG_ONLINE;
}
bool ChannelState::offline() const { return !available(); }
uint32_t ChannelState::type() const {
  return local_ ? local_->getChannelType() : snapshot_ ? snapshot_->Type : 0;
}
uint32_t ChannelState::function() const {
  return local_      ? local_->getDefaultFunction()
         : snapshot_ ? snapshot_->Default
                     : 0;
}
uint32_t ChannelState::validityTimeSec() const {
  return local_      ? local_->getValidityTimeSec()
         : snapshot_ ? snapshot_->ValueValidityTimeSec
                     : 0;
}
bool ChannelState::value(uint8_t output[8]) const {
  if (!output || !available()) return false;
  if (local_) {
    TDS_SuplaDeviceChannel_E snapshot;
    if (!buildChannelSnapshot(local_, &snapshot)) return false;
    memcpy(output, snapshot.value, 8);
  } else {
    memcpy(output, snapshot_->value, 8);
  }
  return true;
}
double ChannelState::temperature() const {
  if (local_) return local_->getLastTemperature();
  if (!available()) return TEMPERATURE_NOT_AVAILABLE;
  if (type() == SUPLA_CHANNELTYPE_THERMOMETER) {
    double result;
    if (sizeof(result) == 8) {
      memcpy(&result, snapshot_->value, sizeof(result));
    } else {
      uint8_t packed[8];
      memcpy(packed, snapshot_->value, sizeof(packed));
      result = doublePacked2float(packed);
    }
    return isfinite(result) ? result : TEMPERATURE_NOT_AVAILABLE;
  }
  if (type() == SUPLA_CHANNELTYPE_HUMIDITYANDTEMPSENSOR) {
    int32_t result;
    memcpy(&result, snapshot_->value, sizeof(result));
    return result / 1000.0;
  }
  return TEMPERATURE_NOT_AVAILABLE;
}
}  // namespace Supla

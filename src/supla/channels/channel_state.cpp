// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "channel_state.h"

#include <math.h>
#include <string.h>
#include <supla/channels/channel.h>
#include <supla/control/hvac_base.h>
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
  auto element =
      Element::getElementByChannelNumber(channel->getChannelNumber());
  auto hvac = element ? element->getHvacBase() : nullptr;
  if (hvac) output->Default = hvac->supLanFunction();
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
bool ChannelState::availableFor(ChannelCapability capability) const {
  if (!available()) return false;
  const auto t = type();
  const auto f = function();
  // Unassigned local channels retain their historical type-based behavior.
  // A remote NONE is explicit authoritative unavailability for every role.
  if (!local_ && f == 0) return false;
  switch (capability) {
    case ChannelCapability::Temperature:
      return (t == SUPLA_CHANNELTYPE_THERMOMETER ||
              t == SUPLA_CHANNELTYPE_HUMIDITYANDTEMPSENSOR) &&
          (f == 0 || f == SUPLA_CHANNELFNC_THERMOMETER ||
           f == SUPLA_CHANNELFNC_HUMIDITYANDTEMPERATURE);
    case ChannelCapability::BinaryState:
    case ChannelCapability::FloodDetection:
    case ChannelCapability::ContainerLevel:
      // Legacy SDK accepts type-based binary inputs, including unassigned
      // sensors. Preserve those functions for every local/remote role.
      return t == SUPLA_CHANNELTYPE_BINARYSENSOR &&
          (f == 0 || f == SUPLA_CHANNELFNC_BINARY_SENSOR ||
           f == SUPLA_CHANNELFNC_FLOOD_SENSOR ||
           f == SUPLA_CHANNELFNC_CONTAINER_LEVEL_SENSOR ||
           f == SUPLA_CHANNELFNC_NOLIQUIDSENSOR ||
           f == SUPLA_CHANNELFNC_OPENINGSENSOR_WINDOW ||
           f == SUPLA_CHANNELFNC_OPENINGSENSOR_DOOR ||
           f == SUPLA_CHANNELFNC_HOTELCARDSENSOR ||
           f == SUPLA_CHANNELFNC_ALARMARMAMENTSENSOR ||
           f == SUPLA_CHANNELFNC_MAILSENSOR ||
           f == SUPLA_CHANNELFNC_OPENINGSENSOR_ROLLERSHUTTER ||
           f == SUPLA_CHANNELFNC_OPENINGSENSOR_ROOFWINDOW ||
           f == SUPLA_CHANNELFNC_OPENINGSENSOR_GARAGEDOOR ||
           f == SUPLA_CHANNELFNC_OPENINGSENSOR_GATE ||
           f == SUPLA_CHANNELFNC_OPENINGSENSOR_GATEWAY ||
           f == SUPLA_CHANNELFNC_MOTION_SENSOR);
    case ChannelCapability::HvacMaster:
    case ChannelCapability::HvacDemand:
      return t == SUPLA_CHANNELTYPE_HVAC &&
          (f == 0 || f == SUPLA_CHANNELFNC_HVAC_THERMOSTAT ||
           f == SUPLA_CHANNELFNC_HVAC_DRYER ||
           f == SUPLA_CHANNELFNC_HVAC_FAN || f == SUPLA_CHANNELFNC_HVAC_HRV ||
           f == SUPLA_CHANNELFNC_HVAC_THERMOSTAT_HEAT_COOL ||
           f == SUPLA_CHANNELFNC_HVAC_THERMOSTAT_DIFFERENTIAL ||
           f == SUPLA_CHANNELFNC_HVAC_DOMESTIC_HOT_WATER);
  }
  return false;
}
bool ChannelState::binary(bool *result) const {
  uint8_t bytes[8];
  if (!result || !availableFor(ChannelCapability::BinaryState) ||
      !value(bytes)) return false;
  *result = bytes[0] != 0;
  return true;
}
bool ChannelState::hvac(THVACValue *result) const {
  uint8_t bytes[8];
  if (!result || !availableFor(ChannelCapability::HvacMaster) ||
      !value(bytes)) return false;
  memcpy(result, bytes, sizeof(*result));
  return true;
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
  if (!availableFor(ChannelCapability::Temperature))
    return TEMPERATURE_NOT_AVAILABLE;
  if (local_) return local_->getLastTemperature();
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

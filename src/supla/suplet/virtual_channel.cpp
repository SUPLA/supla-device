// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <supla/suplet/config.h>

#if SUPLA_SUPLET_ENABLED

#include <supla/suplet/virtual_channel.h>

namespace Supla {
namespace Suplet {

VirtualRelay::VirtualRelay(uint8_t subDeviceId,
                           int channelNumber,
                           _supla_int_t functions)
    : Supla::Control::VirtualRelay(functions), subDeviceId(subDeviceId) {
  getChannel()->setSubDeviceId(subDeviceId);
  getChannel()->setChannelNumber(channelNumber);
}

bool VirtualRelay::isOwnerOfSubDeviceId(int subDeviceId) const {
  return this->subDeviceId == subDeviceId;
}

VirtualBinarySensor::VirtualBinarySensor(uint8_t subDeviceId,
                                         int channelNumber,
                                         bool keepStateInStorage)
    : Supla::Sensor::VirtualBinary(keepStateInStorage),
      subDeviceId(subDeviceId) {
  getChannel()->setSubDeviceId(subDeviceId);
  getChannel()->setChannelNumber(channelNumber);
}

bool VirtualBinarySensor::isOwnerOfSubDeviceId(int subDeviceId) const {
  return this->subDeviceId == subDeviceId;
}

VirtualThermometer::VirtualThermometer(uint8_t subDeviceId, int channelNumber)
    : Supla::Sensor::VirtualThermometer(), subDeviceId(subDeviceId) {
  getChannel()->setSubDeviceId(subDeviceId);
  getChannel()->setChannelNumber(channelNumber);
}

bool VirtualThermometer::isOwnerOfSubDeviceId(int subDeviceId) const {
  return this->subDeviceId == subDeviceId;
}

}  // namespace Suplet
}  // namespace Supla

#endif  // SUPLA_SUPLET_ENABLED

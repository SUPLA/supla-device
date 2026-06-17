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

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

#ifndef SRC_SUPLA_SUPLET_VIRTUAL_CHANNEL_H_
#define SRC_SUPLA_SUPLET_VIRTUAL_CHANNEL_H_

#include <supla/control/virtual_relay.h>
#include <supla/sensor/virtual_binary.h>
#include <supla/sensor/virtual_thermometer.h>

namespace Supla {
namespace Suplet {

class VirtualRelay : public Supla::Control::VirtualRelay {
 public:
  VirtualRelay(uint8_t subDeviceId,
               int channelNumber,
               _supla_int_t functions =
                   (0xFF ^ SUPLA_BIT_FUNC_CONTROLLINGTHEROLLERSHUTTER));

  bool isOwnerOfSubDeviceId(int subDeviceId) const override;

 private:
  uint8_t subDeviceId = 0;
};

class VirtualBinarySensor : public Supla::Sensor::VirtualBinary {
 public:
  VirtualBinarySensor(uint8_t subDeviceId,
                      int channelNumber,
                      bool keepStateInStorage = false);

  bool isOwnerOfSubDeviceId(int subDeviceId) const override;

 private:
  uint8_t subDeviceId = 0;
};

class VirtualThermometer : public Supla::Sensor::VirtualThermometer {
 public:
  VirtualThermometer(uint8_t subDeviceId, int channelNumber);

  bool isOwnerOfSubDeviceId(int subDeviceId) const override;

 private:
  uint8_t subDeviceId = 0;
};

}  // namespace Suplet
}  // namespace Supla

#endif  // SRC_SUPLA_SUPLET_VIRTUAL_CHANNEL_H_

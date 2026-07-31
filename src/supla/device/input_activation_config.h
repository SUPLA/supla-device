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

#ifndef SRC_SUPLA_DEVICE_INPUT_ACTIVATION_CONFIG_H_
#define SRC_SUPLA_DEVICE_INPUT_ACTIVATION_CONFIG_H_

#include <stdint.h>

namespace Supla {
namespace Device {

#pragma pack(push, 1)

struct InputActivationConfig {
  uint8_t mode = 0;
  uint8_t reserved[7] = {};

  bool operator==(const InputActivationConfig &other) const {
    return mode == other.mode;
  }

  bool operator!=(const InputActivationConfig &other) const {
    return !(*this == other);
  }
};

struct InputActivationProperties {
  uint8_t availableModes = 0;
  uint8_t defaultMode = 0;

  bool operator==(const InputActivationProperties &other) const {
    return availableModes == other.availableModes &&
           defaultMode == other.defaultMode;
  }

  bool operator!=(const InputActivationProperties &other) const {
    return !(*this == other);
  }
};

#pragma pack(pop)

static_assert(sizeof(InputActivationConfig) == 8);

}  // namespace Device
}  // namespace Supla

#endif  // SRC_SUPLA_DEVICE_INPUT_ACTIVATION_CONFIG_H_

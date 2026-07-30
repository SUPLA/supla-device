/*
 Copyright (C) AC SOFTWARE SP. Z O.O.

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

#ifndef SRC_SUPLA_DEVICE_THERMAL_PROTECTION_CONFIG_H_
#define SRC_SUPLA_DEVICE_THERMAL_PROTECTION_CONFIG_H_

#include <stdint.h>

namespace Supla {
namespace Device {

#pragma pack(push, 1)

struct ThermalProtectionConfig {
  int16_t threshold = 0;  // 0.1 deg C
  uint8_t enabled = 0;
  uint8_t reserved[5] = {};

  bool operator==(const ThermalProtectionConfig &other) const {
    return threshold == other.threshold && enabled == other.enabled;
  }

  bool operator!=(const ThermalProtectionConfig &other) const {
    return !(*this == other);
  }
};

struct ThermalProtectionProperties {
  int16_t minThreshold = 0;  // 0.1 deg C
  int16_t maxThreshold = 0;  // 0.1 deg C
  uint8_t disableAllowed = 0;

  bool operator==(const ThermalProtectionProperties &other) const {
    return minThreshold == other.minThreshold &&
           maxThreshold == other.maxThreshold &&
           disableAllowed == other.disableAllowed;
  }

  bool operator!=(const ThermalProtectionProperties &other) const {
    return !(*this == other);
  }
};

#pragma pack(pop)

static_assert(sizeof(ThermalProtectionConfig) == 8);

}  // namespace Device
}  // namespace Supla

#endif  // SRC_SUPLA_DEVICE_THERMAL_PROTECTION_CONFIG_H_

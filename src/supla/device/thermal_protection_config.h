// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

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

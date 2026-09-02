// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

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

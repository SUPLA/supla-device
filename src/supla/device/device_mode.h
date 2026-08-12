// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_DEVICE_DEVICE_MODE_H_
#define SRC_SUPLA_DEVICE_DEVICE_MODE_H_

#include <stdint.h>

namespace Supla {

enum DeviceMode : uint8_t {
  DEVICE_MODE_NOT_SET = 0,
  DEVICE_MODE_TEST = 1,
  DEVICE_MODE_NORMAL = 2,
  DEVICE_MODE_CONFIG = 3,
  DEVICE_MODE_SW_UPDATE = 4,
  DEVICE_MODE_OFFLINE = 5,
  DEVICE_MODE_NOT_CONFIGURED = 6
};

}  // namespace Supla

#endif  // SRC_SUPLA_DEVICE_DEVICE_MODE_H_

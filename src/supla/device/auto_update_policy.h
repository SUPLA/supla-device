// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_DEVICE_AUTO_UPDATE_POLICY_H_
#define SRC_SUPLA_DEVICE_AUTO_UPDATE_POLICY_H_

#include <stdint.h>

namespace Supla {
enum class AutoUpdatePolicy: uint8_t {
  ForcedOff = 0,
  Disabled = 1,
  SecurityOnly = 2,  // default
  AllUpdates = 3
};

}  // namespace Supla

#endif  // SRC_SUPLA_DEVICE_AUTO_UPDATE_POLICY_H_

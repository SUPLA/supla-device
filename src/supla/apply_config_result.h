// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_APPLY_CONFIG_RESULT_H_
#define SRC_SUPLA_APPLY_CONFIG_RESULT_H_

#include <stdint.h>

namespace Supla {

enum class ApplyConfigResult : uint8_t {
  NotSupported,
  Success,
  DataError,
  SetChannelConfigNeeded,
};

}  // namespace Supla

#endif  // SRC_SUPLA_APPLY_CONFIG_RESULT_H_

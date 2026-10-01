// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_EXAMPLES_SUPLAN_POC1_COMMON_SUPLAN_POC1_PROFILE_H_
#define EXTRAS_EXAMPLES_SUPLAN_POC1_COMMON_SUPLAN_POC1_PROFILE_H_

#include <supla/control/virtual_relay.h>
#include "suplan_poc1_credentials.h"

namespace Supla {
namespace SupLan {
namespace Poc1 {

inline void initializeRelay(Supla::Control::VirtualRelay *relay) {
  if (relay != nullptr) {
    relay->setDefaultStateOff();
    relay->onInit();
  }
}

}  // namespace Poc1
}  // namespace SupLan
}  // namespace Supla

#endif  // EXTRAS_EXAMPLES_SUPLAN_POC1_COMMON_SUPLAN_POC1_PROFILE_H_

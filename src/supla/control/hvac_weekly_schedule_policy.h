// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_CONTROL_HVAC_WEEKLY_SCHEDULE_POLICY_H_
#define SRC_SUPLA_CONTROL_HVAC_WEEKLY_SCHEDULE_POLICY_H_

#include <supla-common/proto.h>

namespace Supla {
namespace Control {

class HvacBase;
class HvacWeeklySchedulePolicy {
 public:
  bool isProgramValid(const HvacBase &owner,
                      const TWeeklyScheduleProgram &program,
                      bool isAltWeeklySchedule) const;
};

}  // namespace Control
}  // namespace Supla

#endif  // SRC_SUPLA_CONTROL_HVAC_WEEKLY_SCHEDULE_POLICY_H_

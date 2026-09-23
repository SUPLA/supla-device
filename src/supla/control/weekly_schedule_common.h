// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_CONTROL_WEEKLY_SCHEDULE_COMMON_H_
#define SRC_SUPLA_CONTROL_WEEKLY_SCHEDULE_COMMON_H_

#include <supla-common/proto.h>

namespace Supla {

enum DayOfWeek {
  DayOfWeek_Sunday = 0,
  DayOfWeek_Monday = 1,
  DayOfWeek_Tuesday = 2,
  DayOfWeek_Wednesday = 3,
  DayOfWeek_Thursday = 4,
  DayOfWeek_Friday = 5,
  DayOfWeek_Saturday = 6
};

namespace Control {

int calculateWeeklyScheduleIndex(enum DayOfWeek dayOfWeek, int hour,
                                 int quarter);

int getWeeklyScheduleProgramId(const TChannelConfig_WeeklySchedule *schedule,
                               int index);
bool setWeeklyScheduleProgramId(TChannelConfig_WeeklySchedule *schedule,
                                int index,
                                int programId);

}  // namespace Control
}  // namespace Supla

#endif  // SRC_SUPLA_CONTROL_WEEKLY_SCHEDULE_COMMON_H_

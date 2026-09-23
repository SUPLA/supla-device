// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "weekly_schedule_common.h"

namespace Supla {
namespace Control {

int calculateWeeklyScheduleIndex(enum DayOfWeek dayOfWeek, int hour,
                                 int quarter) {
  if (dayOfWeek < DayOfWeek_Sunday || dayOfWeek > DayOfWeek_Saturday) {
    return -1;
  }
  if (hour < 0 || hour > 23) {
    return -1;
  }
  if (quarter < 0 || quarter > 3) {
    return -1;
  }

  return (dayOfWeek * 24 + hour) * 4 + quarter;
}

int getWeeklyScheduleProgramId(const TChannelConfig_WeeklySchedule *schedule,
                               int index) {
  if (schedule == nullptr) {
    return 0;
  }
  if (index < 0 || index >= SUPLA_WEEKLY_SCHEDULE_VALUES_SIZE) {
    return 0;
  }

  return (schedule->Quarters[index / 2] >> (index % 2 * 4)) & 0xF;
}

bool setWeeklyScheduleProgramId(TChannelConfig_WeeklySchedule *schedule,
                                int index,
                                int programId) {
  if (schedule == nullptr || index < 0 ||
      index >= SUPLA_WEEKLY_SCHEDULE_VALUES_SIZE || programId < 0 ||
      programId > SUPLA_WEEKLY_SCHEDULE_PROGRAMS_MAX_SIZE) {
    return false;
  }

  if (index % 2) {
    schedule->Quarters[index / 2] =
        (schedule->Quarters[index / 2] & 0x0F) | (programId << 4);
  } else {
    schedule->Quarters[index / 2] =
        (schedule->Quarters[index / 2] & 0xF0) | programId;
  }
  return true;
}

}  // namespace Control
}  // namespace Supla

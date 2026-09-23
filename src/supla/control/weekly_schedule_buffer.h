// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_CONTROL_WEEKLY_SCHEDULE_BUFFER_H_
#define SRC_SUPLA_CONTROL_WEEKLY_SCHEDULE_BUFFER_H_

#include <supla-common/proto.h>

#include "weekly_schedule_common.h"

namespace Supla {
namespace Control {

class WeeklyScheduleBuffer {
 public:
  WeeklyScheduleBuffer();
  ~WeeklyScheduleBuffer();

  TChannelConfig_WeeklySchedule *get(bool alt);
  const TChannelConfig_WeeklySchedule *get(bool alt) const;
  void set(bool alt, TChannelConfig_WeeklySchedule *schedule);

  void clear(bool alt);
  void clearAll();

  int calculateIndex(enum DayOfWeek dayOfWeek, int hour, int quarter) const;
  int getProgramId(const TChannelConfig_WeeklySchedule *schedule, int index)
      const;
  bool setWeeklySchedule(TChannelConfig_WeeklySchedule *schedule,
                         int index,
                         int programId) const;
  bool setWeeklySchedule(TChannelConfig_WeeklySchedule *schedule,
                         enum DayOfWeek dayOfWeek,
                         int hour,
                         int quarter,
                         int programId) const;
  TWeeklyScheduleProgram getProgramById(
      const TChannelConfig_WeeklySchedule *schedule, int programId) const;
  TWeeklyScheduleProgram getProgramAt(
      const TChannelConfig_WeeklySchedule *schedule, int quarterIndex) const;
  int getCurrentQuarter() const;
  bool resolveCurrentProgram(const TChannelConfig_WeeklySchedule *schedule,
                             TWeeklyScheduleProgram *program,
                             int *programId) const;
  bool resolveProgramAt(const TChannelConfig_WeeklySchedule *schedule,
                        int quarterIndex,
                        TWeeklyScheduleProgram *program,
                        int *programId) const;

 private:
  TChannelConfig_WeeklySchedule *weeklySchedule_ = nullptr;
  TChannelConfig_WeeklySchedule *altWeeklySchedule_ = nullptr;
};

}  // namespace Control
}  // namespace Supla

#endif  // SRC_SUPLA_CONTROL_WEEKLY_SCHEDULE_BUFFER_H_

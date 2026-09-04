/*
 Copyright (C) AC SOFTWARE SP. Z O.O.

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

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

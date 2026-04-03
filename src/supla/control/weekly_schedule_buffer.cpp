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

#include "weekly_schedule_buffer.h"

#include <string.h>
#include <supla/clock/clock.h>

namespace Supla {
namespace Control {

WeeklyScheduleBuffer::WeeklyScheduleBuffer() {
}

WeeklyScheduleBuffer::~WeeklyScheduleBuffer() {
  delete weeklySchedule_;
  delete altWeeklySchedule_;
}

TChannelConfig_WeeklySchedule *WeeklyScheduleBuffer::get(bool alt) {
  return alt ? altWeeklySchedule_ : weeklySchedule_;
}

const TChannelConfig_WeeklySchedule *WeeklyScheduleBuffer::get(bool alt) const {
  return alt ? altWeeklySchedule_ : weeklySchedule_;
}

void WeeklyScheduleBuffer::set(bool alt,
                               TChannelConfig_WeeklySchedule *schedule) {
  auto &slot = alt ? altWeeklySchedule_ : weeklySchedule_;
  delete slot;
  slot = schedule;
}

void WeeklyScheduleBuffer::clear(bool alt) {
  auto &schedule = alt ? altWeeklySchedule_ : weeklySchedule_;
  delete schedule;
  schedule = nullptr;
}

void WeeklyScheduleBuffer::clearAll() {
  clear(false);
  clear(true);
}

int WeeklyScheduleBuffer::calculateIndex(enum DayOfWeek dayOfWeek,
                                         int hour,
                                         int quarter) const {
  return calculateWeeklyScheduleIndex(dayOfWeek, hour, quarter);
}

int WeeklyScheduleBuffer::getProgramId(
    const TChannelConfig_WeeklySchedule *schedule, int index) const {
  return getWeeklyScheduleProgramId(schedule, index);
}

bool WeeklyScheduleBuffer::setWeeklySchedule(
    TChannelConfig_WeeklySchedule *schedule, int index, int programId) const {
  if (schedule == nullptr) {
    return false;
  }
  if (index < 0 || index >= SUPLA_WEEKLY_SCHEDULE_VALUES_SIZE) {
    return false;
  }
  if (programId < 0 || programId > SUPLA_WEEKLY_SCHEDULE_PROGRAMS_MAX_SIZE) {
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

bool WeeklyScheduleBuffer::setWeeklySchedule(
    TChannelConfig_WeeklySchedule *schedule,
    enum DayOfWeek dayOfWeek,
    int hour,
    int quarter,
    int programId) const {
  return setWeeklySchedule(
      schedule, calculateIndex(dayOfWeek, hour, quarter), programId);
}

TWeeklyScheduleProgram WeeklyScheduleBuffer::getProgramById(
    const TChannelConfig_WeeklySchedule *schedule, int programId) const {
  if (programId < 1 || programId > SUPLA_WEEKLY_SCHEDULE_PROGRAMS_MAX_SIZE ||
      schedule == nullptr) {
    return {};
  }

  return schedule->Program[programId - 1];
}

TWeeklyScheduleProgram WeeklyScheduleBuffer::getProgramAt(
    const TChannelConfig_WeeklySchedule *schedule, int quarterIndex) const {
  int programId = 1;
  if (schedule == nullptr) {
    TWeeklyScheduleProgram program = {};
    program.SetpointTemperatureCool = INT16_MIN;
    program.SetpointTemperatureHeat = INT16_MIN;
    return program;
  }
  if (quarterIndex >= 0 && schedule != nullptr) {
    programId = getProgramId(schedule, quarterIndex);
  }

  TWeeklyScheduleProgram program = {};
  program.SetpointTemperatureCool = INT16_MIN;
  program.SetpointTemperatureHeat = INT16_MIN;

  if (programId == 0) {
    program.Mode = SUPLA_HVAC_MODE_OFF;
    return program;
  }
  if (programId < 0 || programId > SUPLA_WEEKLY_SCHEDULE_PROGRAMS_MAX_SIZE) {
    return program;
  }

  return schedule->Program[programId - 1];
}

int WeeklyScheduleBuffer::getCurrentQuarter() const {
  if (Supla::Clock::IsReady()) {
    return calculateIndex(Supla::Clock::GetHvacDayOfWeek(),
                          Supla::Clock::GetHour(),
                          Supla::Clock::GetQuarter());
  }
  return -1;
}

int WeeklyScheduleBuffer::getCurrentProgramId(
    const TChannelConfig_WeeklySchedule *schedule) const {
  int quarterIndex = getCurrentQuarter();
  int programId = 1;
  if (quarterIndex >= 0 && schedule != nullptr) {
    programId = getProgramId(schedule, quarterIndex);
  }
  return programId;
}

TWeeklyScheduleProgram WeeklyScheduleBuffer::getCurrentProgram(
    const TChannelConfig_WeeklySchedule *schedule) const {
  return getProgramAt(schedule, getCurrentQuarter());
}

}  // namespace Control
}  // namespace Supla

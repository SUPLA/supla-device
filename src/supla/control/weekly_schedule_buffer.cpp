// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

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
  if (slot == schedule) {
    return;
  }
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
  return setWeeklyScheduleProgramId(schedule, index, programId);
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

bool WeeklyScheduleBuffer::resolveCurrentProgram(
    const TChannelConfig_WeeklySchedule *schedule,
    TWeeklyScheduleProgram *program,
    int *programId) const {
  return resolveProgramAt(schedule, getCurrentQuarter(), program, programId);
}

bool WeeklyScheduleBuffer::resolveProgramAt(
    const TChannelConfig_WeeklySchedule *schedule,
    int quarterIndex,
    TWeeklyScheduleProgram *program,
    int *programId) const {
  if (schedule == nullptr || program == nullptr || programId == nullptr) {
    return false;
  }

  *program = {};
  program->SetpointTemperatureCool = INT16_MIN;
  program->SetpointTemperatureHeat = INT16_MIN;

  int resolvedProgramId = 1;
  if (quarterIndex >= 0) {
    resolvedProgramId = getProgramId(schedule, quarterIndex);
  }
  *programId = resolvedProgramId;

  if (resolvedProgramId == 0) {
    return true;
  }
  if (resolvedProgramId < 1 ||
      resolvedProgramId > SUPLA_WEEKLY_SCHEDULE_PROGRAMS_MAX_SIZE) {
    return false;
  }

  *program = schedule->Program[resolvedProgramId - 1];
  return true;
}

}  // namespace Control
}  // namespace Supla

// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "weekly_schedule_component.h"

#include <supla/clock/clock.h>
#include <supla/time.h>

namespace Supla {
namespace Control {

static_assert(DayOfWeek_Sunday == 0 && DayOfWeek_Monday == 1 &&
                  DayOfWeek_Tuesday == 2 && DayOfWeek_Wednesday == 3 &&
                  DayOfWeek_Thursday == 4 && DayOfWeek_Friday == 5 &&
                  DayOfWeek_Saturday == 6,
              "DayOfWeek must match struct tm::tm_wday");

WeeklyScheduleClockState WeeklyScheduleController::getClockState() const {
  return getClockState(millis() <= 30000);
}

WeeklyScheduleClockState WeeklyScheduleController::getClockState(
    bool startupDelay) const {
  if (Supla::Clock::IsReady()) {
    return WeeklyScheduleClockState::Ready;
  }
  return startupDelay ? WeeklyScheduleClockState::Waiting
                      : WeeklyScheduleClockState::TimedOut;
}

WeeklyScheduleTimeSnapshot
WeeklyScheduleController::getWeeklyScheduleTimeSnapshot(
    bool startupDelay) const {
  WeeklyScheduleTimeSnapshot time;
  time.state = getClockState(startupDelay);
  if (time.state == WeeklyScheduleClockState::Ready) {
    auto *clock = Supla::Clock::GetInstance();
    struct tm timeInfo = {};
    if (clock == nullptr || !clock->getLocalTime(&timeInfo)) {
      time.state = startupDelay ? WeeklyScheduleClockState::Waiting
                                : WeeklyScheduleClockState::TimedOut;
      return time;
    }
    time.dayOfWeek = static_cast<enum DayOfWeek>(timeInfo.tm_wday);
    time.hour = timeInfo.tm_hour;
    time.quarter = timeInfo.tm_min / 15;
  }
  return time;
}

bool WeeklyScheduleController::processCurrentProgram(bool startupDelay) {
  auto time = getWeeklyScheduleTimeSnapshot(startupDelay);
  onWeeklyScheduleClockState(time.state);
  if (time.state == WeeklyScheduleClockState::Waiting) {
    return false;
  }

  TWeeklyScheduleProgram program = {};
  int programId = -1;
  if (!resolveWeeklyScheduleProgram(time, &program, &programId)) {
    return false;
  }

  bool programChanged = updateCurrentProgramId(programId);
  return applyResolvedWeeklyScheduleProgram(
      program, programId, programChanged);
}

bool WeeklyScheduleController::resolveWeeklyScheduleProgram(
    const WeeklyScheduleTimeSnapshot &time,
    TWeeklyScheduleProgram *program,
    int *programId) {
  if (programSource_ == nullptr) {
    return false;
  }
  return programSource_->resolveProgram(
      time, shouldUseAltWeeklySchedule(), program, programId);
}

bool WeeklyScheduleController::applyResolvedWeeklyScheduleProgram(
    const TWeeklyScheduleProgram &program,
    int programId,
    bool programChanged) {
  (void)(program);
  (void)(programId);
  (void)(programChanged);
  return false;
}

bool WeeklyScheduleController::updateCurrentProgramId(int programId) {
  bool changed = currentProgramId_ != programId;
  currentProgramId_ = programId;
  return changed;
}

void WeeklyScheduleController::resetCurrentProgramId() {
  currentProgramId_ = -1;
}

void WeeklyScheduleController::setWeeklyScheduleProgramSource(
    WeeklyScheduleProgramSource *source) {
  programSource_ = source;
}

bool ExternalManagedWeeklySchedule::canActivate() const {
  return true;
}

bool ExternalManagedWeeklySchedule::isActive() const {
  return active_;
}

bool ExternalManagedWeeklySchedule::switchToWeeklySchedule() {
  active_ = true;
  return true;
}

void ExternalManagedWeeklySchedule::switchToManualMode() {
  active_ = false;
}

void ExternalManagedWeeklySchedule::restoreWeeklyScheduleMode(
    bool enabled) {
  active_ = enabled;
}

bool ExternalManagedWeeklySchedule::processWeeklySchedule() {
  return active_;
}

}  // namespace Control
}  // namespace Supla

// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "weekly_schedule_component.h"

#include <supla/clock/clock.h>
#include <supla/time.h>

namespace Supla {
namespace Control {

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

bool WeeklyScheduleController::updateCurrentProgramId(int programId) {
  bool changed = currentProgramId_ != programId;
  currentProgramId_ = programId;
  return changed;
}

void WeeklyScheduleController::resetCurrentProgramId() {
  currentProgramId_ = -1;
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

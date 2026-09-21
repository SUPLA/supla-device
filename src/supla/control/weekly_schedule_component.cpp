// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "weekly_schedule_component.h"

#include <supla/clock/clock.h>
#include <supla/time.h>

namespace Supla {
namespace Control {

struct WeeklyScheduleComponents::State {
  enum class LifecycleState : uint8_t {
    Unassigned,
    Assigned,
    Started,
  };

  LifecycleState lifecycleState = LifecycleState::Unassigned;
  WeeklyScheduleController *controller = nullptr;
  WeeklyScheduleConfigHandler *configHandler = nullptr;
  WeeklyScheduleProgramSource *programSource = nullptr;
};

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
    time.secondOfQuarter = (timeInfo.tm_min % 15) * 60 + timeInfo.tm_sec;
    // Count Gregorian days before this year, then add the local day of year.
    const int32_t year = timeInfo.tm_year + 1899;
    time.dayNumber = 365 * year + year / 4 - year / 100 + year / 400 +
                     timeInfo.tm_yday;
  }
  return time;
}

bool WeeklyScheduleController::processCurrentProgram(bool startupDelay) {
  auto time = getWeeklyScheduleTimeSnapshot(startupDelay);
  onWeeklyScheduleClockState(time.state);
  if (time.state == WeeklyScheduleClockState::Waiting ||
      (requiresReadyClock() && time.state != WeeklyScheduleClockState::Ready)) {
    return false;
  }

  TWeeklyScheduleProgram program = {};
  int programId = -1;
  if (!resolveWeeklyScheduleProgram(time, &program, &programId)) {
    return false;
  }

  bool programChanged = updateCurrentProgramId(programId);
  return applyProgramAt(time,
      program, programId, programChanged);
}

bool WeeklyScheduleController::applyProgramAt(
    const WeeklyScheduleTimeSnapshot &time,
    const TWeeklyScheduleProgram &program, int programId, bool programChanged) {
  (void)(time);
  return applyResolvedWeeklyScheduleProgram(program, programId, programChanged);
}

bool WeeklyScheduleController::resolveProgramTiming(
    const WeeklyScheduleTimeSnapshot &time, int programId, int32_t *occurrence,
    uint32_t *elapsedSeconds) {
  return programSource_ != nullptr &&
         programSource_->resolveProgramTiming(
             time, shouldUseAltWeeklySchedule(), programId, occurrence,
             elapsedSeconds);
}

bool WeeklyScheduleController::resolveWeeklyScheduleProgram(
    const WeeklyScheduleTimeSnapshot &time, TWeeklyScheduleProgram *program,
    int *programId) {
  if (programSource_ == nullptr) {
    return false;
  }
  return programSource_->resolveProgram(time, shouldUseAltWeeklySchedule(),
                                        program, programId);
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

WeeklyScheduleComponents::~WeeklyScheduleComponents() {
  if (state_ != nullptr) {
    state_->configHandler = nullptr;
    state_->programSource = nullptr;
    delete state_->controller;
    state_->controller = nullptr;
    delete state_;
    state_ = nullptr;
  }
}

bool WeeklyScheduleComponents::set(
    WeeklyScheduleController *controller,
    WeeklyScheduleConfigHandler *configHandler,
    WeeklyScheduleProgramSource *programSource) {
  if (state_ == nullptr) {
    state_ = new State;
  }

  bool sameComponents = controller == state_->controller &&
                        configHandler == state_->configHandler &&
                        programSource == state_->programSource;
  if (state_->lifecycleState == State::LifecycleState::Started) {
    return sameComponents;
  }
  if (state_->lifecycleState != State::LifecycleState::Unassigned &&
      sameComponents) {
    return true;
  }
  if (controller != state_->controller) {
    state_->configHandler = nullptr;
    state_->programSource = nullptr;
    delete state_->controller;
    state_->controller = controller;
  }
  state_->configHandler = configHandler;
  state_->programSource = programSource;
  if (state_->controller != nullptr) {
    state_->controller->setWeeklyScheduleProgramSource(
        state_->programSource);
  }
  state_->lifecycleState = State::LifecycleState::Assigned;
  return true;
}

void WeeklyScheduleComponents::loadConfig() {
  if (state_ == nullptr) {
    return;
  }
  state_->lifecycleState = State::LifecycleState::Started;
  if (state_->configHandler != nullptr) {
    state_->configHandler->onLoadConfig();
  }
}

bool WeeklyScheduleComponents::isAssigned() const {
  return state_ != nullptr &&
         state_->lifecycleState != State::LifecycleState::Unassigned;
}

bool WeeklyScheduleComponents::isStarted() const {
  return state_ != nullptr &&
         state_->lifecycleState == State::LifecycleState::Started;
}

WeeklyScheduleController *WeeklyScheduleComponents::getController() const {
  return state_ == nullptr ? nullptr : state_->controller;
}

WeeklyScheduleConfigHandler *
WeeklyScheduleComponents::getConfigHandler() const {
  return state_ == nullptr ? nullptr : state_->configHandler;
}

WeeklyScheduleProgramSource *
WeeklyScheduleComponents::getProgramSource() const {
  return state_ == nullptr ? nullptr : state_->programSource;
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

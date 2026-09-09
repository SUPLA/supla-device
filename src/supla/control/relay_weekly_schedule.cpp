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

#include "relay_weekly_schedule.h"

#include <string.h>
#include <supla/log_wrapper.h>
#include <supla/protocol/protocol_layer.h>
#include <supla/storage/config.h>
#include <supla/storage/config_tags.h>
#include <supla/storage/storage.h>
#include <supla/time.h>

#include "relay.h"
#include "weekly_schedule_common.h"

namespace Supla {
namespace Control {

RelayWeeklySchedule::RelayWeeklySchedule(Relay *owner) : owner_(owner) {
}

RelayWeeklySchedule::~RelayWeeklySchedule() {
}

bool RelayWeeklySchedule::isConfigured() const {
  return isWeeklyScheduleConfigured();
}

bool RelayWeeklySchedule::canActivate() const {
  if (!NativeWeeklyScheduleController::canActivate()) {
    return false;
  }
  auto *schedule = getSchedule(false, true);
  return schedule != nullptr && isWeeklyScheduleValid(schedule);
}

bool RelayWeeklySchedule::switchToWeeklySchedule() {
  if (!canActivate()) {
    return false;
  }
  resetRuntimeOverride();
  return NativeWeeklyScheduleController::switchToWeeklySchedule();
}

void RelayWeeklySchedule::restoreWeeklyScheduleMode(bool enabled) {
  resetRuntimeOverride();
  NativeWeeklyScheduleController::restoreWeeklyScheduleMode(enabled);
}

Supla::Element *RelayWeeklySchedule::getScheduleOwner() const {
  return owner_;
}

const char *RelayWeeklySchedule::getDeviceLabel() const {
  return "Relay";
}

const char *RelayWeeklySchedule::getScheduleStorageTag(bool alt) const {
  (void)(alt);
  return Supla::ConfigTag::RelayWeeklyCfgTag;
}

bool RelayWeeklySchedule::validateSchedule(
    const TChannelConfig_WeeklySchedule *schedule, bool alt) const {
  (void)(alt);
  return isWeeklyScheduleValid(schedule);
}

void RelayWeeklySchedule::fillDefaultSchedule(
    TChannelConfig_WeeklySchedule *schedule, bool alt) {
  (void)(alt);
  if (owner_ != nullptr) {
    owner_->fillDefaultWeeklySchedule(schedule);
  }
}

bool RelayWeeklySchedule::isWeeklyScheduleEnabled() const {
  return isActive();
}

void RelayWeeklySchedule::syncWeeklyScheduleMode(uint8_t programMode) {
  if (owner_ == nullptr) {
    return;
  }
  owner_->getChannel()->setRelayWeeklyScheduleEnabled(isActive());
  owner_->getChannel()->setRelayMode(
      isActive() ? programMode : SUPLA_RELAY_MODE_NOT_SET);
}

void RelayWeeklySchedule::scheduleWeeklyScheduleStateSave() {
  Supla::Storage::ScheduleSave(Relay::relayStorageSaveDelay, 2000);
}

bool RelayWeeklySchedule::isManualActionAllowed(bool turnOn) const {
  if (!isActive()) {
    return true;
  }
  auto currentProgramMode = getCurrentProgramMode();
  if (turnOn) {
    return currentProgramMode != SUPLA_RELAY_MODE_FORCED_OFF;
  }
  return currentProgramMode != SUPLA_RELAY_MODE_FORCED_ON;
}

bool RelayWeeklySchedule::isProgramValid(
    const TWeeklyScheduleProgram &program) const {
  if (owner_ == nullptr ||
      !owner_->isWeeklyScheduleProgramModeSupported(program.Mode)) {
    return false;
  }
  if (program.RelayModeDurationS == 0 &&
      program.RelayOppositeModeDurationS == 0) {
    return true;
  }
  if (program.RelayModeDurationS == 0 ||
      (program.Mode != SUPLA_RELAY_MODE_ON_ONCE &&
       program.Mode != SUPLA_RELAY_MODE_OFF_ONCE)) {
    return false;
  }
  return program.RelayOppositeModeDurationS == 0 ||
         (!owner_->isStaircaseFunction() && !owner_->isImpulseFunction());
}

void RelayWeeklySchedule::resetRuntimeOverride() {
  phase_ = UINT32_MAX;
  timed_ = false;
  suppressed_ = false;
  pendingManualAction_ = false;
}

void RelayWeeklySchedule::onManualAction() {
  if (!isActive()) {
    return;
  }
  const auto time = getWeeklyScheduleTimeSnapshot(false);
  TWeeklyScheduleProgram program = {};
  int programId = -1;
  if (time.state != WeeklyScheduleClockState::Ready ||
      !resolveWeeklyScheduleProgram(time, &program, &programId)) {
    pendingManualAction_ = true;
    return;
  }
  // Resolve the occurrence at command time without driving the output. The
  // next iteration may already belong to another program.
  processProgramAt(time, program, programId,
                   updateCurrentProgramId(programId), true);
}

void RelayWeeklySchedule::onWeeklyScheduleClockState(
    WeeklyScheduleClockState state) {
  if (state != WeeklyScheduleClockState::Ready) {
    syncWeeklyScheduleMode(SUPLA_RELAY_MODE_NOT_SET);
    phase_ = UINT32_MAX;
  }
}

bool RelayWeeklySchedule::applyProgramAt(
    const WeeklyScheduleTimeSnapshot &time,
    const TWeeklyScheduleProgram &program, int programId, bool programChanged) {
  return processProgramAt(time, program, programId, programChanged, false);
}

bool RelayWeeklySchedule::processProgramAt(
    const WeeklyScheduleTimeSnapshot &time,
    const TWeeklyScheduleProgram &program, int programId, bool programChanged,
    bool manualAction) {
  if (programId > 0 && !isProgramValid(program)) {
    switchToManualMode();
    scheduleWeeklyScheduleStateSave();
    return false;
  }
  timed_ = programId > 0 && program.RelayModeDurationS > 0;
  const int32_t absoluteQuarter =
      time.dayNumber * 96 + time.hour * 4 + time.quarter;
  int32_t occurrence = occurrence_;
  uint32_t elapsed = 0;
  bool hasTiming = false;
  if (timed_ && !programChanged && occurrence >= 0 &&
      (absoluteQuarter == lastTimingQuarter_ ||
       absoluteQuarter == lastTimingQuarter_ + 1)) {
    elapsed = static_cast<uint32_t>(absoluteQuarter - occurrence) * 900 +
              time.secondOfQuarter;
    hasTiming = true;
  } else if (timed_) {
    hasTiming = WeeklyScheduleController::resolveProgramTiming(
        time, programId, &occurrence, &elapsed);
  }
  const bool changed = programChanged || (timed_ && occurrence != occurrence_);
  if (manualAction) {
    pendingManualAction_ = true;
  }
  if (changed) {
    occurrence_ = occurrence;
    phase_ = UINT32_MAX;
    suppressed_ = timed_ && hasTiming && pendingManualAction_;
  } else if (timed_ && hasTiming && pendingManualAction_) {
    suppressed_ = true;
  }
  if (!timed_ || hasTiming) {
    pendingManualAction_ = false;
  }
  if (!timed_) {
    occurrence_ = -1;
    lastTimingQuarter_ = -1;
    return !manualAction &&
        applyWeeklyScheduleMode(programId > 0 ? program.Mode : 0, changed);
  }
  if (hasTiming) {
    lastTimingQuarter_ = absoluteQuarter;
  }
  if (manualAction) {
    return false;
  }
  syncWeeklyScheduleMode(program.Mode);
  if (!hasTiming || suppressed_ || !owner_->isFullyInitialized()) {
    return false;
  }
  const uint32_t first = program.RelayModeDurationS;
  const uint32_t second = program.RelayOppositeModeDurationS;
  const uint32_t period = first + second;
  const uint32_t phase = second == 0 ? (elapsed >= first ? 1 : 0)
      : (elapsed / period) * 2 + (elapsed % period >= first ? 1 : 0);
  if (phase != phase_) {
    const bool on = (program.Mode == SUPLA_RELAY_MODE_ON_ONCE) !=
                    ((phase & 1) != 0);
    if (!owner_->applyWeeklyScheduleState(on)) {
      return false;
    }
    phase_ = phase;
  }
  return true;
}

bool RelayWeeklySchedule::isWeeklyScheduleValid(
    const TChannelConfig_WeeklySchedule *newSchedule) const {
  if (newSchedule == nullptr) {
    return false;
  }

  for (int i = 0; i < SUPLA_WEEKLY_SCHEDULE_PROGRAMS_MAX_SIZE; i++) {
    if (!isProgramValid(newSchedule->Program[i])) {
      SUPLA_LOG_WARNING("Relay[%d]: invalid weekly schedule program %d",
                        owner_->getChannelNumber(),
                        i);
      return false;
    }
  }

  for (int i = 0; i < SUPLA_WEEKLY_SCHEDULE_VALUES_SIZE; i++) {
    int programId = getProgramId(newSchedule, i);
    if (programId < 0 || programId > SUPLA_WEEKLY_SCHEDULE_PROGRAMS_MAX_SIZE) {
      SUPLA_LOG_WARNING(
          "Relay[%d]: weekly schedule references invalid program %d",
          owner_->getChannelNumber(),
          programId);
      return false;
    }
  }

  return true;
}

void RelayWeeklySchedule::onNativeScheduleApplied(
    bool alt, bool local, bool changed) {
  if (changed) {
    resetRuntimeOverride();
  }
  NativeWeeklyScheduleController::onNativeScheduleApplied(
      alt, local, changed);
}

bool RelayWeeklySchedule::iterateAlways() {
  return processWeeklySchedule();
}

bool RelayWeeklySchedule::applyWeeklyScheduleMode(
    uint8_t currentProgramMode,
    bool programChanged) {
  if (owner_ == nullptr ||
      !owner_->isWeeklyScheduleProgramModeSupported(currentProgramMode)) {
    switchToManualMode();
    scheduleWeeklyScheduleStateSave();
    return false;
  }
  syncWeeklyScheduleMode(currentProgramMode);

  if (currentProgramMode == SUPLA_RELAY_MODE_NOT_SET) {
    return true;
  }

  owner_->applyWeeklyScheduleProgram(currentProgramMode, programChanged);
  return true;
}

uint8_t RelayWeeklySchedule::getCurrentProgramMode() const {
  if (!isConfigured()) {
    return SUPLA_RELAY_MODE_NOT_SET;
  }
  TWeeklyScheduleProgram program = {};
  int currentProgramId = -1;
  auto time = getWeeklyScheduleTimeSnapshot(millis() <= 30000);
  auto *self = const_cast<RelayWeeklySchedule *>(this);
  if (time.state != WeeklyScheduleClockState::Ready ||
      !self->resolveWeeklyScheduleProgram(
          time, &program, &currentProgramId)) {
    return SUPLA_RELAY_MODE_NOT_SET;
  }

  if (currentProgramId <= 0) {
    return SUPLA_RELAY_MODE_NOT_SET;
  }

  return program.Mode;
}

}  // namespace Control
}  // namespace Supla

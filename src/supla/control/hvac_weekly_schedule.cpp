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

#include "hvac_weekly_schedule.h"

#include <string.h>
#include <supla/events.h>
#include <supla/log_wrapper.h>
#include <supla/protocol/protocol_layer.h>
#include <supla/storage/config.h>
#include <supla/storage/config_tags.h>
#include <supla/storage/storage.h>
#include <supla/time.h>

#include "hvac_base.h"
#include "weekly_schedule_common.h"

namespace Supla {
namespace Control {

HvacWeeklySchedule::HvacWeeklySchedule(HvacBase *owner) : owner_(owner) {
}

HvacWeeklySchedule::~HvacWeeklySchedule() {
}

bool HvacWeeklySchedule::canActivate() const {
  return isConfigured();
}

bool HvacWeeklySchedule::isConfigured() const {
  return NativeWeeklyScheduleConfigHandler::isConfigured();
}

Supla::Element *HvacWeeklySchedule::getScheduleOwner() const {
  return owner_;
}

const char *HvacWeeklySchedule::getDeviceLabel() const {
  return "HVAC";
}

bool HvacWeeklySchedule::supportsAltSchedule() const {
  return owner_ && owner_->isAltWeeklySchedulePossible();
}

bool HvacWeeklySchedule::hasPersistentDefaultSchedule(bool alt) const {
  return !alt || supportsAltSchedule();
}

void HvacWeeklySchedule::fillDefaultSchedule(
    TChannelConfig_WeeklySchedule *schedule, bool isAltWeeklySchedule) {
  if (owner_) {
    owner_->fillDefaultWeeklySchedule(schedule, isAltWeeklySchedule);
  }
}

const char *HvacWeeklySchedule::getScheduleStorageTag(
    bool isAltWeeklySchedule) const {
  return isAltWeeklySchedule ? Supla::ConfigTag::HvacAltWeeklyCfgTag
                             : Supla::ConfigTag::HvacWeeklyCfgTag;
}

bool HvacWeeklySchedule::validateSchedule(
    const TChannelConfig_WeeklySchedule *schedule,
    bool isAltWeeklySchedule) const {
  return isWeeklyScheduleValid(schedule, isAltWeeklySchedule);
}

void HvacWeeklySchedule::onNativeScheduleLoaded() {
  resetCurrentProgramId();
}

void HvacWeeklySchedule::onNativeScheduleSaved(bool alt, bool notify) {
  if (owner_ == nullptr) {
    return;
  }
  if (notify && owner_->initDone) {
    owner_->requestWeeklyScheduleResend(alt, true);
    owner_->syncWeeklyScheduleConfigTypes();
    owner_->persistChannelConfigChangeState();
  }
  touchCache(owner_->channel.isHvacFlagWeeklySchedule(), millis());
}

bool HvacWeeklySchedule::isActive() const {
  return owner_ && owner_->isWeeklyScheduleEnabled();
}

bool HvacWeeklySchedule::switchToWeeklySchedule() {
  return turnOnWeeklySchedule();
}

void HvacWeeklySchedule::switchToManualMode() {
  resetCurrentProgramId();
  if (owner_) {
    owner_->channel.setHvacFlagWeeklySchedule(false);
  }
}

void HvacWeeklySchedule::restoreWeeklyScheduleMode(bool enabled) {
  resetCurrentProgramId();
  if (owner_) {
    owner_->channel.setHvacFlagWeeklySchedule(enabled);
  }
}

bool HvacWeeklySchedule::isProgramValid(const TWeeklyScheduleProgram &program,
                                        bool isAltWeeklySchedule) const {
  return policy_.isProgramValid(*owner_, program, isAltWeeklySchedule);
}

void HvacWeeklySchedule::unloadSchedulesIfPossible() {
  if (owner_ == nullptr || owner_->channel.isHvacFlagWeeklySchedule()) {
    return;
  }

  SUPLA_LOG_DEBUG("HVAC[%d]: unloading weekly schedule cache",
                  owner_->getChannelNumber());
  unloadSchedule(false);
  unloadSchedule(true);
}

void HvacWeeklySchedule::processCacheRelease() {
  if (owner_ == nullptr) {
    return;
  }

  if (processCache(owner_->channel.isHvacFlagWeeklySchedule(), millis())) {
    unloadSchedulesIfPossible();
  }
}

void HvacWeeklySchedule::saveWeeklySchedule(bool requestResend) {
  if (owner_ == nullptr) {
    return;
  }

  if (getSchedule(false, false) != nullptr) {
    saveSchedule(false, requestResend);
  }
  if (getSchedule(true, false) != nullptr) {
    saveSchedule(true, requestResend);
  }
}

bool HvacWeeklySchedule::isWeeklyScheduleValid(
    const TChannelConfig_WeeklySchedule *newSchedule,
    bool isAltWeeklySchedule) const {
  bool programIsUsed[SUPLA_WEEKLY_SCHEDULE_PROGRAMS_MAX_SIZE] = {};

  for (int i = 0; i < SUPLA_WEEKLY_SCHEDULE_PROGRAMS_MAX_SIZE; i++) {
    HvacBase::debugPrintProgram(&(newSchedule->Program[i]), i);
    if (!isProgramValid(newSchedule->Program[i], isAltWeeklySchedule)) {
      SUPLA_LOG_WARNING(
          "HVAC[%d]: weekly schedule validation failed: invalid program %d",
          owner_->getChannelNumber(),
          i);
      return false;
    }
    if (newSchedule->Program[i].Mode != SUPLA_HVAC_MODE_NOT_SET) {
      programIsUsed[i] = true;
    }
  }

  for (int i = 0; i < SUPLA_WEEKLY_SCHEDULE_VALUES_SIZE; i++) {
    int programId = getWeeklyScheduleProgramId(newSchedule, i);
    if (programId < 0 ||
        programId > SUPLA_WEEKLY_SCHEDULE_PROGRAMS_MAX_SIZE) {
      SUPLA_LOG_WARNING(
          "HVAC[%d]: weekly schedule validation failed: invalid program %d "
          "used in schedule %d",
          owner_->getChannelNumber(),
          programId,
          i);
      return false;
    }
    if (programId != 0 && !programIsUsed[programId - 1]) {
      SUPLA_LOG_WARNING(
          "HVAC[%d]: weekly schedule validation failed: not configured program "
          "used in schedule %d",
          owner_->getChannelNumber(),
          i);
      return false;
    }
  }

  return true;
}

int HvacWeeklySchedule::getWeeklyScheduleProgramId(
    const TChannelConfig_WeeklySchedule *schedule, int index) const {
  if (schedule == nullptr) {
    auto *self = const_cast<HvacWeeklySchedule *>(this);
    if (self->ensureScheduleForUse(false)) {
      schedule = self->getSchedule(false, false);
    }
  }
  return NativeWeeklyScheduleConfigHandler::getProgramId(schedule, index);
}

int HvacWeeklySchedule::calculateIndex(enum DayOfWeek dayOfWeek,
                                       int hour,
                                       int quarter) const {
  return NativeWeeklyScheduleConfigHandler::calculateIndex(
      dayOfWeek, hour, quarter);
}

bool HvacWeeklySchedule::setWeeklySchedule(int index,
                                           int programId,
                                           bool isAltWeeklySchedule) {
  if (index < 0 || index >= SUPLA_WEEKLY_SCHEDULE_VALUES_SIZE) {
    SUPLA_LOG_DEBUG(
        "HVAC[%d]: invalid index %d", owner_->getChannelNumber(), index);
    return false;
  }
  if (programId < 0 || programId > SUPLA_WEEKLY_SCHEDULE_PROGRAMS_MAX_SIZE) {
    SUPLA_LOG_DEBUG("HVAC[%d]: invalid programId %d",
                    owner_->getChannelNumber(),
                    programId);
    return false;
  }

  if (!ensureScheduleForUse(isAltWeeklySchedule)) {
    return false;
  }
  auto schedule = getSchedule(isAltWeeklySchedule, false);

  if (programId > 0 &&
      (schedule->Program[programId - 1].Mode == SUPLA_HVAC_MODE_NOT_SET ||
       schedule->Program[programId - 1].Mode > SUPLA_HVAC_MODE_DRY)) {
    SUPLA_LOG_DEBUG("HVAC[%d]: invalid mode %d for programId %d",
                    owner_->getChannelNumber(),
                    schedule->Program[programId - 1].Mode,
                    programId);
    return false;
  }

  NativeWeeklyScheduleConfigHandler::setWeeklySchedule(
      schedule, index, programId);

  if (owner_->initDone) {
    saveSchedule(isAltWeeklySchedule, true);
  }

  return true;
}

bool HvacWeeklySchedule::setWeeklySchedule(enum DayOfWeek dayOfWeek,
                                           int hour,
                                           int quarter,
                                           int programId,
                                           bool isAltWeeklySchedule) {
  return setWeeklySchedule(
      calculateIndex(dayOfWeek, hour, quarter), programId, isAltWeeklySchedule);
}

bool HvacWeeklySchedule::setProgram(int programId,
                                    unsigned char mode,
                                    _supla_int16_t tHeat,
                                    _supla_int16_t tCool,
                                    bool isAltWeeklySchedule) {
  SUPLA_LOG_DEBUG("HVAC[%d]: set %s program(%d, %d, %d, %d)",
                  owner_->getChannelNumber(),
                  isAltWeeklySchedule ? "Alt" : "Main",
                  programId,
                  mode,
                  tHeat,
                  tCool);

  TWeeklyScheduleProgram program = {mode, {tHeat}, {tCool}};
  if (!policy_.isProgramValid(*owner_, program, isAltWeeklySchedule)) {
    return false;
  }

  if (!ensureScheduleForUse(isAltWeeklySchedule)) {
    return false;
  }
  auto schedule = getSchedule(isAltWeeklySchedule, false);

  schedule->Program[programId - 1].Mode = mode;
  schedule->Program[programId - 1].SetpointTemperatureHeat = tHeat;
  schedule->Program[programId - 1].SetpointTemperatureCool = tCool;

  if (owner_->initDone) {
    saveSchedule(isAltWeeklySchedule, true);
  }
  return true;
}

TWeeklyScheduleProgram HvacWeeklySchedule::getProgramById(
    int programId, bool isAltWeeklySchedule) const {
  if (programId < 1 || programId > SUPLA_WEEKLY_SCHEDULE_PROGRAMS_MAX_SIZE) {
    return {};
  }

  auto *self = const_cast<HvacWeeklySchedule *>(this);
  if (!self->ensureScheduleForUse(isAltWeeklySchedule)) {
    return {};
  }
  auto schedule = self->getSchedule(isAltWeeklySchedule, false);

  return NativeWeeklyScheduleConfigHandler::getProgramById(
      schedule, programId);
}

TWeeklyScheduleProgram HvacWeeklySchedule::getProgramAt(
    int quarterIndex) const {
  bool useAlt = shouldUseAltSchedule();
  auto *self = const_cast<HvacWeeklySchedule *>(this);
  if (!self->ensureScheduleForUse(useAlt)) {
    return NativeWeeklyScheduleConfigHandler::getProgramAt(
        nullptr, quarterIndex);
  }
  auto schedule = self->getSchedule(useAlt, false);
  return NativeWeeklyScheduleConfigHandler::getProgramAt(
      schedule, quarterIndex);
}

int HvacWeeklySchedule::getCurrentQuarter() const {
  return NativeWeeklyScheduleConfigHandler::getCurrentQuarter();
}

TWeeklyScheduleProgram HvacWeeklySchedule::getCurrentProgram() const {
  TWeeklyScheduleProgram program = {};
  int programId = 1;
  if (!resolveCurrentHvacProgram(&program, &programId)) {
    program.SetpointTemperatureCool = INT16_MIN;
    program.SetpointTemperatureHeat = INT16_MIN;
  }
  return program;
}

int HvacWeeklySchedule::getCurrentProgramId() const {
  TWeeklyScheduleProgram program = {};
  int programId = 1;
  if (!resolveCurrentHvacProgram(&program, &programId)) {
    return 1;
  }
  return programId;
}

bool HvacWeeklySchedule::shouldUseAltSchedule() const {
  return owner_ != nullptr &&
         owner_->getChannel()->getDefaultFunction() ==
             SUPLA_CHANNELFNC_HVAC_THERMOSTAT &&
         owner_->config.Subfunction == SUPLA_HVAC_SUBFUNCTION_COOL;
}

bool HvacWeeklySchedule::resolveCurrentHvacProgram(
    TWeeklyScheduleProgram *program, int *programId) const {
  auto time = getWeeklyScheduleTimeSnapshot(false);
  return const_cast<HvacWeeklySchedule *>(this)->resolveCurrentHvacProgram(
      time, program, programId);
}

bool HvacWeeklySchedule::resolveCurrentHvacProgram(
    const WeeklyScheduleTimeSnapshot &time,
    TWeeklyScheduleProgram *program,
    int *programId) {
  if (!resolveWeeklyScheduleProgram(time, program, programId)) {
    return false;
  }
  if (*programId == 0) {
    program->Mode = SUPLA_HVAC_MODE_OFF;
  }
  return true;
}

bool HvacWeeklySchedule::shouldUseAltWeeklySchedule() const {
  return shouldUseAltSchedule();
}

bool HvacWeeklySchedule::applyResolvedWeeklyScheduleProgram(
    const TWeeklyScheduleProgram &program,
    int programId,
    bool programChanged) {
  (void)(programChanged);
  TWeeklyScheduleProgram hvacProgram = program;
  if (programId == 0) {
    hvacProgram.Mode = SUPLA_HVAC_MODE_OFF;
  }
  return owner_->applyWeeklyScheduleProgram(hvacProgram, programId);
}

void HvacWeeklySchedule::onWeeklyScheduleClockState(
    WeeklyScheduleClockState state) {
  if (state == WeeklyScheduleClockState::Waiting) {
    SUPLA_LOG_DEBUG(
        "HVAC[%d]: Weekly schedule enabled, clock not ready -> startup "
        "delay...",
        owner_->getChannelNumber());
    return;
  }
  if (state == WeeklyScheduleClockState::TimedOut) {
    if (!owner_->isWeeklyScheduleClockError()) {
      SUPLA_LOG_WARNING(
          "HVAC[%d]: processs weekly schedule failed - clock is not ready",
          owner_->getChannelNumber());
    }
    owner_->setWeeklyScheduleClockError(true);
    return;
  }
  owner_->setWeeklyScheduleClockError(false);
}

bool HvacWeeklySchedule::turnOnWeeklySchedule() {
  bool useAlt = shouldUseAltSchedule();
  if (!ensureScheduleForUse(useAlt)) {
    return false;
  }
  touchCache(true, millis());
  owner_->setWeeklyScheduleEnabled(true);
  resetCurrentProgramId();
  return processWeeklySchedule();
}

bool HvacWeeklySchedule::processWeeklySchedule() {
  bool useAlt = shouldUseAltSchedule();
  if (!ensureScheduleForUse(useAlt)) {
    return false;
  }
  touchCache(true, millis());
  if (!owner_->isWeeklyScheduleEnabled()) {
    SUPLA_LOG_WARNING(
        "HVAC[%d]: processs weekly schedule failed - it is not enabled",
        owner_->getChannelNumber());
    return false;
  }

  return processCurrentProgram(owner_->isWeeklyScheduleStartupDelay());
}

void HvacWeeklySchedule::initDefaultWeeklySchedule(bool requestResend) {
  clearSchedules();
  initDefaultWeeklyScheduleForType(false, requestResend);
  if (owner_ != nullptr && owner_->getChannel()->getDefaultFunction() ==
                               SUPLA_CHANNELFNC_HVAC_THERMOSTAT) {
    initDefaultWeeklyScheduleForType(true, requestResend);
  }
}

void HvacWeeklySchedule::initDefaultWeeklyScheduleForType(
    bool isAltWeeklySchedule, bool requestResend) {
  bool shouldInitialize =
      getSchedule(isAltWeeklySchedule, false) == nullptr &&
      (!isAltWeeklySchedule ||
       owner_->getChannel()->getDefaultFunction() ==
           SUPLA_CHANNELFNC_HVAC_THERMOSTAT);
  if (shouldInitialize) {
    auto *schedule = ensureSchedule(isAltWeeklySchedule);
    owner_->fillDefaultWeeklySchedule(schedule, isAltWeeklySchedule);
  }
  saveSchedule(isAltWeeklySchedule, requestResend);
}

}  // namespace Control
}  // namespace Supla

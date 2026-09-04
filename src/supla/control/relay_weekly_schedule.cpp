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

bool RelayWeeklySchedule::canActivate() const {
  return isConfigured();
}

bool RelayWeeklySchedule::isConfigured() const {
  return NativeWeeklyScheduleConfigHandler::isConfigured(false);
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
  return weeklyScheduleEnabled_;
}

bool RelayWeeklySchedule::isActive() const {
  return isWeeklyScheduleEnabled();
}

void RelayWeeklySchedule::setWeeklyScheduleEnabled(bool enabled) {
  if (weeklyScheduleEnabled_ == enabled) {
    return;
  }
  weeklyScheduleEnabled_ = enabled;
  if (owner_ != nullptr) {
    owner_->getChannel()->setRelayWeeklyScheduleEnabled(enabled);
    if (!enabled) {
      owner_->getChannel()->setRelayMode(SUPLA_RELAY_MODE_NOT_SET);
    }
  }
  touchCache(enabled, millis());
}

void RelayWeeklySchedule::syncRelayMode(uint8_t programMode) {
  if (owner_ == nullptr) {
    return;
  }
  owner_->getChannel()->setRelayWeeklyScheduleEnabled(weeklyScheduleEnabled_);
  owner_->getChannel()->setRelayMode(
      weeklyScheduleEnabled_ ? programMode : SUPLA_RELAY_MODE_NOT_SET);
}

bool RelayWeeklySchedule::isManualActionAllowed(bool turnOn) const {
  if (!weeklyScheduleEnabled_ || isWaitingForClock()) {
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
  return owner_ != nullptr &&
         owner_->isWeeklyScheduleProgramModeSupported(program.Mode);
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

void RelayWeeklySchedule::onNativeScheduleLoaded() {
  weeklyScheduleEnabled_ = false;
  resetCurrentProgramId();
  syncRelayMode(SUPLA_RELAY_MODE_NOT_SET);
}

void RelayWeeklySchedule::onNativeScheduleLoadFailed(bool alt) {
  (void)(alt);
  onNativeScheduleLoaded();
  Supla::Storage::ScheduleSave(Relay::relayStorageSaveDelay, 2000);
}

void RelayWeeklySchedule::onNativeScheduleApplied(bool alt,
                                                  bool local,
                                                  bool changed) {
  (void)(alt);
  (void)(local);
  if (changed) {
    resetCurrentProgramId();
    if (weeklyScheduleEnabled_) {
      applyCurrentState();
    }
  }
  touchCache(weeklyScheduleEnabled_, millis());
}

void RelayWeeklySchedule::onNativeScheduleSaved(bool alt, bool notify) {
  (void)(alt);
  (void)(notify);
  touchCache(weeklyScheduleEnabled_, millis());
}

const TChannelConfig_WeeklySchedule *RelayWeeklySchedule::getSchedule(
    bool loadIfMissing) const {
  return NativeWeeklyScheduleConfigHandler::getSchedule(false, loadIfMissing);
}

TChannelConfig_WeeklySchedule *RelayWeeklySchedule::getSchedule(
    bool loadIfMissing) {
  return NativeWeeklyScheduleConfigHandler::getSchedule(false, loadIfMissing);
}

bool RelayWeeklySchedule::switchToWeeklySchedule() {
  if (!isConfigured() || getSchedule(true) == nullptr) {
    return false;
  }
  weeklyScheduleEnabled_ = true;
  resetCurrentProgramId();
  touchCache(true, millis());
  syncRelayMode(SUPLA_RELAY_MODE_NOT_SET);
  if (!isWaitingForClock()) {
    applyCurrentState();
  }
  return true;
}

void RelayWeeklySchedule::switchToManualMode() {
  weeklyScheduleEnabled_ = false;
  resetCurrentProgramId();
  if (isConfigured()) {
    touchCache(false, millis());
  } else {
    resetCache();
  }
  syncRelayMode(SUPLA_RELAY_MODE_NOT_SET);
}

void RelayWeeklySchedule::restoreWeeklyScheduleMode(bool enabled) {
  weeklyScheduleEnabled_ = enabled && isConfigured();
  resetCurrentProgramId();
  if (weeklyScheduleEnabled_) {
    touchCache(true, millis());
  }
  syncRelayMode(SUPLA_RELAY_MODE_NOT_SET);
}

void RelayWeeklySchedule::onNativeSchedulePurged() {
  weeklyScheduleEnabled_ = false;
  resetCurrentProgramId();
  syncRelayMode(SUPLA_RELAY_MODE_NOT_SET);
  Supla::Storage::ScheduleSave(Relay::relayStorageSaveDelay, 2000);
}

bool RelayWeeklySchedule::iterateAlways() {
  if (!owner_ || !isConfigured()) {
    return false;
  }

  if (!weeklyScheduleEnabled_) {
    processCacheRelease();
    return false;
  }

  touchCache(true, millis());

  if (isWaitingForClock()) {
    return false;
  }

  applyCurrentState();
  return true;
}

bool RelayWeeklySchedule::processWeeklySchedule() {
  return iterateAlways();
}

void RelayWeeklySchedule::applyCurrentState() {
  if (!weeklyScheduleEnabled_ || isWaitingForClock()) {
    return;
  }
  TWeeklyScheduleProgram program = {};
  int currentProgramId = -1;
  if (!resolveCurrentProgram(false, &program, &currentProgramId)) {
    return;
  }

  uint8_t currentProgramMode = SUPLA_RELAY_MODE_NOT_SET;
  if (currentProgramId > 0) {
    currentProgramMode = program.Mode;
  }

  bool programChanged = updateCurrentProgramId(currentProgramId);

  syncRelayMode(currentProgramMode);

  if (currentProgramMode == SUPLA_RELAY_MODE_NOT_SET) {
    return;
  }

  owner_->applyWeeklyScheduleProgram(currentProgramMode, programChanged);
}

uint8_t RelayWeeklySchedule::getCurrentProgramMode() const {
  if (isWaitingForClock()) {
    return SUPLA_RELAY_MODE_NOT_SET;
  }
  TWeeklyScheduleProgram program = {};
  int currentProgramId = -1;
  if (!isConfigured() ||
      !resolveCurrentProgram(false, &program, &currentProgramId)) {
    return SUPLA_RELAY_MODE_NOT_SET;
  }

  if (currentProgramId <= 0) {
    return SUPLA_RELAY_MODE_NOT_SET;
  }

  return program.Mode;
}

bool RelayWeeklySchedule::isWaitingForClock() const {
  return getClockState() == WeeklyScheduleClockState::Waiting;
}

void RelayWeeklySchedule::unloadScheduleIfPossible() {
  if (owner_ == nullptr || weeklyScheduleEnabled_) {
    return;
  }

  SUPLA_LOG_DEBUG("Relay[%d]: unloading weekly schedule cache",
                  owner_->getChannelNumber());
  unloadSchedule(false);
}

void RelayWeeklySchedule::processCacheRelease() {
  if (owner_ == nullptr) {
    return;
  }

  if (processCache(weeklyScheduleEnabled_, millis())) {
    unloadScheduleIfPossible();
  }
}

}  // namespace Control
}  // namespace Supla

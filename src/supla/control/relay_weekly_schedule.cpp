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
  return nativeStorage_.isConfigured();
}

bool RelayWeeklySchedule::supportsConfigType(uint8_t configType) const {
  return configType == SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE;
}

const char *RelayWeeklySchedule::getScheduleLabel(bool alt) const {
  (void)(alt);
  return "weekly schedule";
}

const char *RelayWeeklySchedule::getScheduleStorageTag(bool alt) const {
  (void)(alt);
  return Supla::ConfigTag::RelayWeeklyCfgTag;
}

bool RelayWeeklySchedule::validateNativeSchedule(
    const TChannelConfig_WeeklySchedule *schedule, bool alt) const {
  (void)(alt);
  return isWeeklyScheduleValid(schedule);
}

bool RelayWeeklySchedule::validateNativeScheduleCallback(
    void *context,
    const TChannelConfig_WeeklySchedule *schedule,
    bool alt) {
  return static_cast<RelayWeeklySchedule *>(context)->validateNativeSchedule(
      schedule, alt);
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
  nativeStorage_.touchCache(enabled, millis());
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
  switch (program.Mode) {
    case SUPLA_RELAY_MODE_NOT_SET:
    case SUPLA_RELAY_MODE_OFF_ONCE:
    case SUPLA_RELAY_MODE_ON_ONCE:
    case SUPLA_RELAY_MODE_FORCED_ON:
    case SUPLA_RELAY_MODE_FORCED_OFF: {
      return true;
    }
    default: {
      return false;
    }
  }
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
    int programId = nativeStorage_.getProgramId(newSchedule, i);
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

void RelayWeeklySchedule::clearSchedule(bool eraseStorage) {
  const bool eraseStoredSchedule = eraseStorage && isConfigured();
  weeklyScheduleEnabled_ = false;
  resetCurrentProgramId();
  nativeStorage_.reset();
  syncRelayMode(SUPLA_RELAY_MODE_NOT_SET);
  Supla::Storage::ScheduleSave(Relay::relayStorageSaveDelay, 2000);

  auto cfg = Supla::Storage::ConfigInstance();
  if (eraseStoredSchedule && cfg) {
    char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    owner_->generateKey(key, getScheduleStorageTag(false));
    cfg->eraseKey(key);
    owner_->generateKey(key, Supla::ConfigTag::WeeklyScheduleChangedFlagTag);
    cfg->eraseKey(key);
    cfg->saveWithDelay(5000);
  }
}

bool RelayWeeklySchedule::loadSchedule() {
  if (owner_ == nullptr) {
    return false;
  }

  if (!nativeStorage_.load(false,
                           *owner_,
                           "Relay",
                           getScheduleLabel(false),
                           getScheduleStorageTag(false),
                           this,
                           validateNativeScheduleCallback)) {
    weeklyScheduleEnabled_ = false;
    resetCurrentProgramId();
    nativeStorage_.reset();
    syncRelayMode(SUPLA_RELAY_MODE_NOT_SET);
    Supla::Storage::ScheduleSave(Relay::relayStorageSaveDelay, 2000);
    return false;
  }

  nativeStorage_.touchCache(weeklyScheduleEnabled_, millis());
  return true;
}

const TChannelConfig_WeeklySchedule *RelayWeeklySchedule::getSchedule(
    bool loadIfMissing) const {
  return const_cast<RelayWeeklySchedule *>(this)->getSchedule(loadIfMissing);
}

TChannelConfig_WeeklySchedule *RelayWeeklySchedule::getSchedule(
    bool loadIfMissing) {
  auto *schedule = nativeStorage_.getSchedule(false);
  if (schedule == nullptr && loadIfMissing) {
    if (!loadSchedule()) {
      return nullptr;
    }
    schedule = nativeStorage_.getSchedule(false);
  }
  return schedule;
}

void RelayWeeklySchedule::saveWeeklySchedule() {
  if (owner_ == nullptr) {
    return;
  }

  auto *schedule = const_cast<RelayWeeklySchedule *>(this)->getSchedule(true);
  if (schedule == nullptr) {
    return;
  }

  nativeStorage_.save(false,
                      *owner_,
                      "Relay",
                      getScheduleLabel(false),
                      getScheduleStorageTag(false));
  auto cfg = Supla::Storage::ConfigInstance();
  if (!cfg) {
    return;
  }
  cfg->saveWithDelay(5000);
  nativeStorage_.touchCache(weeklyScheduleEnabled_, millis());
}

bool RelayWeeklySchedule::switchToWeeklySchedule() {
  if (!isConfigured() || getSchedule(true) == nullptr) {
    return false;
  }
  weeklyScheduleEnabled_ = true;
  resetCurrentProgramId();
  nativeStorage_.touchCache(true, millis());
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
    nativeStorage_.touchCache(false, millis());
  } else {
    nativeStorage_.resetCache();
  }
  syncRelayMode(SUPLA_RELAY_MODE_NOT_SET);
}

void RelayWeeklySchedule::restoreWeeklyScheduleMode(bool enabled) {
  weeklyScheduleEnabled_ = enabled && isConfigured();
  resetCurrentProgramId();
  if (weeklyScheduleEnabled_) {
    nativeStorage_.touchCache(true, millis());
  }
  syncRelayMode(SUPLA_RELAY_MODE_NOT_SET);
}

void RelayWeeklySchedule::onLoadConfig() {
  weeklyScheduleEnabled_ = false;
  resetCurrentProgramId();

  bool configured = false;
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg && owner_ != nullptr) {
    char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    owner_->generateKey(key, getScheduleStorageTag(false));
    configured =
        cfg->getBlobSize(key) == sizeof(TChannelConfig_WeeklySchedule);
  }
  nativeStorage_.reset(configured);
  syncRelayMode(SUPLA_RELAY_MODE_NOT_SET);
}

void RelayWeeklySchedule::purgeConfig() {
  if (owner_ == nullptr) {
    return;
  }
  nativeStorage_.erase(false, *owner_, getScheduleStorageTag(false));
  clearSchedule(false);
}

bool RelayWeeklySchedule::iterateAlways() {
  if (!owner_ || !isConfigured()) {
    return false;
  }

  if (!weeklyScheduleEnabled_) {
    processCacheRelease();
    return false;
  }

  nativeStorage_.touchCache(true, millis());

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
  auto *schedule = getSchedule(true);
  if (schedule == nullptr) {
    return;
  }

  int currentProgramId = nativeStorage_.getCurrentProgramId(schedule);
  if (currentProgramId < 0) {
    return;
  }

  uint8_t currentProgramMode = SUPLA_RELAY_MODE_NOT_SET;
  if (currentProgramId > 0) {
    currentProgramMode =
        nativeStorage_.getProgramById(schedule, currentProgramId).Mode;
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
  auto *schedule = getSchedule(true);
  if (schedule == nullptr || !isConfigured()) {
    return SUPLA_RELAY_MODE_NOT_SET;
  }

  int currentProgramId = nativeStorage_.getCurrentProgramId(schedule);
  if (currentProgramId <= 0) {
    return SUPLA_RELAY_MODE_NOT_SET;
  }

  return nativeStorage_.getProgramById(schedule, currentProgramId).Mode;
}

bool RelayWeeklySchedule::isWaitingForClock() const {
  return getClockState() == WeeklyScheduleClockState::Waiting;
}

Supla::ApplyConfigResult RelayWeeklySchedule::applyChannelConfig(
    TSD_ChannelConfig *result, bool local) {
  (void)(local);

  if (result == nullptr) {
    return Supla::ApplyConfigResult::DataError;
  }

  if (result->ConfigType != SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE) {
    return Supla::ApplyConfigResult::NotSupported;
  }

  if (result->ConfigSize == 0) {
    return Supla::ApplyConfigResult::SetChannelConfigNeeded;
  }

  if (result->ConfigSize < sizeof(TChannelConfig_WeeklySchedule)) {
    return Supla::ApplyConfigResult::DataError;
  }

  auto newSchedule =
      reinterpret_cast<TChannelConfig_WeeklySchedule *>(result->Config);
  if (!isWeeklyScheduleValid(newSchedule)) {
    return Supla::ApplyConfigResult::DataError;
  }
  if (nativeStorage_.updateSchedule(false, *newSchedule)) {
    resetCurrentProgramId();
    saveWeeklySchedule();
    if (weeklyScheduleEnabled_) {
      applyCurrentState();
    }
  }

  nativeStorage_.touchCache(weeklyScheduleEnabled_, millis());
  return Supla::ApplyConfigResult::Success;
}

void RelayWeeklySchedule::fillChannelConfig(void *channelConfig,
                                            int *size,
                                            uint8_t configType) {
  if (size == nullptr) {
    return;
  }
  *size = 0;

  if (channelConfig == nullptr ||
      configType != SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE) {
    return;
  }

  auto *schedule = getSchedule(isConfigured());
  if (schedule == nullptr) {
    memset(channelConfig, 0, sizeof(TChannelConfig_WeeklySchedule));
    if (owner_ != nullptr) {
      owner_->fillDefaultWeeklySchedule(
          reinterpret_cast<TChannelConfig_WeeklySchedule *>(channelConfig));
    }
    *size = sizeof(TChannelConfig_WeeklySchedule);
    return;
  }

  *reinterpret_cast<TChannelConfig_WeeklySchedule *>(channelConfig) = *schedule;
  *size = sizeof(TChannelConfig_WeeklySchedule);
  nativeStorage_.touchCache(weeklyScheduleEnabled_, millis());
}

void RelayWeeklySchedule::unloadScheduleIfPossible() {
  if (owner_ == nullptr || weeklyScheduleEnabled_) {
    return;
  }

  SUPLA_LOG_DEBUG("Relay[%d]: unloading weekly schedule cache",
                  owner_->getChannelNumber());
  nativeStorage_.clearSchedules();
}

void RelayWeeklySchedule::processCacheRelease() {
  if (owner_ == nullptr) {
    return;
  }

  if (nativeStorage_.processCache(weeklyScheduleEnabled_, millis())) {
    unloadScheduleIfPossible();
  }
}

}  // namespace Control
}  // namespace Supla

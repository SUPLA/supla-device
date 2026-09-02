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
#include <supla/clock/clock.h>
#include <supla/log_wrapper.h>
#include <supla/protocol/protocol_layer.h>
#include <supla/storage/config.h>
#include <supla/storage/config_tags.h>
#include <supla/storage/storage.h>
#include <supla/time.h>

#include "relay.h"
#include "weekly_schedule_common.h"
#include "weekly_schedule_storage.h"

namespace Supla {
namespace Control {

RelayWeeklySchedule::RelayWeeklySchedule(Relay *owner) : owner_(owner) {
}

RelayWeeklySchedule::~RelayWeeklySchedule() {
}

const char *RelayWeeklySchedule::getStorageTag() {
  return Supla::ConfigTag::RelayWeeklyCfgTag;
}

bool RelayWeeklySchedule::isConfigured() const {
  return isWeeklyScheduleConfigured_;
}

bool RelayWeeklySchedule::isWeeklyScheduleEnabled() const {
  return weeklyScheduleEnabled_;
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
  cacheRuntime_.touch(enabled, millis());
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
  if (!weeklyScheduleEnabled_) {
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
    int programId = weeklyScheduleBuffer_.getProgramId(newSchedule, i);
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

bool RelayWeeklySchedule::isNoOpSchedule(
    const TChannelConfig_WeeklySchedule *newSchedule) const {
  if (newSchedule == nullptr) {
    return true;
  }
  const auto *data = reinterpret_cast<const uint8_t *>(newSchedule);
  for (size_t i = 0; i < sizeof(TChannelConfig_WeeklySchedule); i++) {
    if (data[i] != 0) {
      return false;
    }
  }
  return true;
}

void RelayWeeklySchedule::clearSchedule() {
  const bool eraseStoredSchedule = isWeeklyScheduleConfigured_;
  isWeeklyScheduleConfigured_ = false;
  weeklyScheduleEnabled_ = false;
  weeklyScheduleChangedOffline_ = 0;
  lastCurrentProgramId_ = -1;
  weeklyScheduleBuffer_.clearAll();
  cacheRuntime_.reset();
  syncRelayMode(SUPLA_RELAY_MODE_NOT_SET);

  auto cfg = Supla::Storage::ConfigInstance();
  if (eraseStoredSchedule && cfg) {
    char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    owner_->generateKey(key, getStorageTag());
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

  auto *schedule = weeklyScheduleBuffer_.get(false);
  if (!WeeklyScheduleStorage::load(
          owner_->getChannelNumber(),
          "Relay",
          "weekly schedule",
          getStorageTag(),
          false,
          schedule,
          [this](char *key, const char *storageTag) {
            owner_->generateKey(key, storageTag);
          },
          [this](const TChannelConfig_WeeklySchedule *loadedSchedule) {
            return isWeeklyScheduleValid(loadedSchedule);
          })) {
    weeklyScheduleBuffer_.set(false, schedule);
    weeklyScheduleBuffer_.clear(false);
    isWeeklyScheduleConfigured_ = false;
    weeklyScheduleEnabled_ = false;
    lastCurrentProgramId_ = -1;
    cacheRuntime_.reset();
    syncRelayMode(SUPLA_RELAY_MODE_NOT_SET);
    return false;
  }

  weeklyScheduleBuffer_.set(false, schedule);
  isWeeklyScheduleConfigured_ = true;
  cacheRuntime_.touch(weeklyScheduleEnabled_, millis());
  return true;
}

const TChannelConfig_WeeklySchedule *RelayWeeklySchedule::getSchedule(
    bool loadIfMissing) const {
  return const_cast<RelayWeeklySchedule *>(this)->getSchedule(loadIfMissing);
}

TChannelConfig_WeeklySchedule *RelayWeeklySchedule::getSchedule(
    bool loadIfMissing) {
  auto *schedule = weeklyScheduleBuffer_.get(false);
  if (schedule == nullptr && loadIfMissing) {
    if (!loadSchedule()) {
      return nullptr;
    }
    schedule = weeklyScheduleBuffer_.get(false);
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

  WeeklyScheduleStorage::save(
      owner_->getChannelNumber(),
      "Relay",
      "weekly schedule",
      getStorageTag(),
      schedule,
      [this](char *key, const char *storageTag) {
        owner_->generateKey(key, storageTag);
      });
  auto cfg = Supla::Storage::ConfigInstance();
  if (!cfg) {
    return;
  }
  char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
  owner_->generateKey(key, Supla::ConfigTag::WeeklyScheduleChangedFlagTag);
  cfg->setUInt8(key, weeklyScheduleChangedOffline_ ? 1 : 0);
  cfg->saveWithDelay(5000);
  cacheRuntime_.touch(weeklyScheduleEnabled_, millis());
}

bool RelayWeeklySchedule::switchToWeeklySchedule() {
  if (!isConfigured()) {
    return false;
  }
  weeklyScheduleEnabled_ = true;
  lastCurrentProgramId_ = -1;
  cacheRuntime_.touch(true, millis());
  syncRelayMode(getCurrentProgramMode());
  applyCurrentState();
  return true;
}

void RelayWeeklySchedule::switchToManualMode() {
  weeklyScheduleEnabled_ = false;
  lastCurrentProgramId_ = -1;
  if (isWeeklyScheduleConfigured_) {
    cacheRuntime_.touch(false, millis());
  } else {
    cacheRuntime_.reset();
  }
  syncRelayMode(SUPLA_RELAY_MODE_NOT_SET);
}

void RelayWeeklySchedule::onLoadConfig() {
  isWeeklyScheduleConfigured_ = false;
  weeklyScheduleEnabled_ = false;
  lastCurrentProgramId_ = -1;
  cacheRuntime_.reset();
  weeklyScheduleBuffer_.clearAll();

  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg && owner_ != nullptr) {
    char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    owner_->generateKey(key, getStorageTag());
    isWeeklyScheduleConfigured_ =
        cfg->getBlobSize(key) == sizeof(TChannelConfig_WeeklySchedule);
  }
  syncRelayMode(SUPLA_RELAY_MODE_NOT_SET);
}

bool RelayWeeklySchedule::iterateAlways() {
  if (!owner_ || !isWeeklyScheduleConfigured_) {
    return false;
  }

  if (!weeklyScheduleEnabled_) {
    processCacheRelease();
    return false;
  }

  cacheRuntime_.touch(true, millis());

  if (!Supla::Clock::IsReady()) {
    return false;
  }

  applyCurrentState();
  return true;
}

void RelayWeeklySchedule::applyCurrentState() {
  if (!weeklyScheduleEnabled_ || !Supla::Clock::IsReady()) {
    return;
  }
  auto *schedule = getSchedule(true);
  if (schedule == nullptr) {
    return;
  }

  int currentProgramId = weeklyScheduleBuffer_.getCurrentProgramId(schedule);
  if (currentProgramId < 0) {
    return;
  }

  uint8_t currentProgramMode = SUPLA_RELAY_MODE_NOT_SET;
  if (currentProgramId > 0) {
    currentProgramMode =
        weeklyScheduleBuffer_.getProgramById(schedule, currentProgramId).Mode;
  }

  bool programChanged = currentProgramId != lastCurrentProgramId_;
  lastCurrentProgramId_ = currentProgramId;

  syncRelayMode(currentProgramMode);

  if (currentProgramMode == SUPLA_RELAY_MODE_NOT_SET) {
    return;
  }

  switch (currentProgramMode) {
    case SUPLA_RELAY_MODE_ON_ONCE: {
      if (programChanged && !owner_->isOn() &&
          !owner_->getChannel()->isRelayOvercurrentCutOff()) {
        owner_->turnOn();
      }
      return;
    }
    case SUPLA_RELAY_MODE_OFF_ONCE: {
      if (programChanged && owner_->isOn()) {
        owner_->turnOff();
      }
      return;
    }
    case SUPLA_RELAY_MODE_FORCED_ON: {
      if (!owner_->getChannel()->isRelayOvercurrentCutOff() &&
          !owner_->isOn()) {
        owner_->turnOn();
      }
      return;
    }
    case SUPLA_RELAY_MODE_FORCED_OFF: {
      if (owner_->isOn()) {
        owner_->turnOff();
      }
      return;
    }
    default: {
      return;
    }
  }
}

uint8_t RelayWeeklySchedule::getCurrentProgramMode() const {
  if (!Supla::Clock::IsReady()) {
    return SUPLA_RELAY_MODE_NOT_SET;
  }
  auto *schedule = getSchedule(true);
  if (schedule == nullptr || !isConfigured()) {
    return SUPLA_RELAY_MODE_NOT_SET;
  }

  int currentProgramId = weeklyScheduleBuffer_.getCurrentProgramId(schedule);
  if (currentProgramId <= 0) {
    return SUPLA_RELAY_MODE_NOT_SET;
  }

  return weeklyScheduleBuffer_.getProgramById(schedule, currentProgramId).Mode;
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
    if (isConfigured()) {
      weeklyScheduleChangedOffline_ = 1;
    }
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
  if (isNoOpSchedule(newSchedule)) {
    clearSchedule();
    return Supla::ApplyConfigResult::Success;
  }

  auto *schedule = weeklyScheduleBuffer_.get(false);
  if (schedule == nullptr) {
    schedule = new TChannelConfig_WeeklySchedule();
    weeklyScheduleBuffer_.set(false, schedule);
    memset(schedule, 0, sizeof(TChannelConfig_WeeklySchedule));
  }

  if (!isConfigured() ||
      memcmp(schedule, newSchedule, sizeof(TChannelConfig_WeeklySchedule)) !=
          0) {
    memcpy(schedule, newSchedule, sizeof(TChannelConfig_WeeklySchedule));
    isWeeklyScheduleConfigured_ = true;
    lastCurrentProgramId_ = -1;
    weeklyScheduleChangedOffline_ = 0;
    saveWeeklySchedule();
    if (weeklyScheduleEnabled_) {
      applyCurrentState();
    }
  }

  cacheRuntime_.touch(weeklyScheduleEnabled_, millis());
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
    *size = sizeof(TChannelConfig_WeeklySchedule);
    return;
  }

  *reinterpret_cast<TChannelConfig_WeeklySchedule *>(channelConfig) = *schedule;
  *size = sizeof(TChannelConfig_WeeklySchedule);
  cacheRuntime_.touch(weeklyScheduleEnabled_, millis());
}

void RelayWeeklySchedule::unloadScheduleIfPossible() {
  if (owner_ == nullptr || weeklyScheduleEnabled_) {
    return;
  }

  SUPLA_LOG_DEBUG("Relay[%d]: unloading weekly schedule cache",
                  owner_->getChannelNumber());
  weeklyScheduleBuffer_.clearAll();
}

void RelayWeeklySchedule::processCacheRelease() {
  if (owner_ == nullptr) {
    return;
  }

  if (cacheRuntime_.process(weeklyScheduleEnabled_, millis())) {
    unloadScheduleIfPossible();
  }
}

}  // namespace Control
}  // namespace Supla

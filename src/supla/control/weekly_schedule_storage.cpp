// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "weekly_schedule_storage.h"

#include <string.h>

#include <supla/element.h>
#include <supla/log_wrapper.h>
#include <supla/storage/config.h>
#include <supla/storage/storage.h>
#include <supla/time.h>

namespace Supla {
namespace Control {

bool NativeWeeklyScheduleConfigHandler::configTypeToAlt(uint8_t configType,
                                                        bool *alt) const {
  if (alt == nullptr) {
    return false;
  }
  if (configType == SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE) {
    *alt = false;
    return true;
  }
  if (configType == SUPLA_CONFIG_TYPE_ALT_WEEKLY_SCHEDULE &&
      supportsAltSchedule()) {
    *alt = true;
    return true;
  }
  return false;
}

bool NativeWeeklyScheduleConfigHandler::supportsConfigType(
    uint8_t configType) const {
  bool alt = false;
  return configTypeToAlt(configType, &alt);
}

const char *NativeWeeklyScheduleConfigHandler::getScheduleLabel(
    bool alt) const {
  return alt ? "alt weekly schedule" : "weekly schedule";
}

void NativeWeeklyScheduleConfigHandler::onLoadConfig() {
  clearSchedules();
  cacheRuntime_.reset();

  auto *owner = getScheduleOwner();
  auto cfg = Supla::Storage::ConfigInstance();
  for (int index = 0; index < 2; index++) {
    bool alt = index != 0;
    if (alt && !supportsAltSchedule()) {
      configured_[index] = false;
      persisted_[index] = false;
      loadAttempted_[index] = false;
      continue;
    }

    persisted_[index] = false;
    if (!hasPersistentDefaultSchedule(alt) && cfg != nullptr &&
        owner != nullptr) {
      char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
      owner->generateKey(key, getScheduleStorageTag(alt));
      persisted_[index] =
          cfg->getBlobSize(key) == sizeof(TChannelConfig_WeeklySchedule);
    }
    configured_[index] =
        persisted_[index] || hasPersistentDefaultSchedule(alt);
    loadAttempted_[index] = false;
  }
  onNativeScheduleLoaded();
}

Supla::ApplyConfigResult
NativeWeeklyScheduleConfigHandler::applyChannelConfig(TSD_ChannelConfig *config,
                                                       bool local) {
  if (config == nullptr) {
    return Supla::ApplyConfigResult::DataError;
  }

  bool alt = false;
  if (!configTypeToAlt(config->ConfigType, &alt)) {
    return Supla::ApplyConfigResult::NotSupported;
  }
  if (config->ConfigSize == 0) {
    if (hasPersistentDefaultSchedule(alt) && !ensureScheduleForUse(alt)) {
      return Supla::ApplyConfigResult::DataError;
    }
    return Supla::ApplyConfigResult::SetChannelConfigNeeded;
  }
  if (config->ConfigSize < sizeof(TChannelConfig_WeeklySchedule)) {
    return Supla::ApplyConfigResult::DataError;
  }

  auto *newSchedule =
      reinterpret_cast<TChannelConfig_WeeklySchedule *>(config->Config);
  if (!validateSchedule(newSchedule, alt)) {
    return Supla::ApplyConfigResult::DataError;
  }

  bool changed = updateSchedule(alt, *newSchedule);
  if (changed) {
    saveSchedule(alt, local);
  }
  onNativeScheduleApplied(alt, local, changed);
  return Supla::ApplyConfigResult::Success;
}

void NativeWeeklyScheduleConfigHandler::fillChannelConfig(
    void *config, int *size, uint8_t configType) {
  if (size == nullptr) {
    return;
  }
  *size = 0;

  bool alt = false;
  if (config == nullptr || !configTypeToAlt(configType, &alt)) {
    return;
  }

  auto *schedule = getSchedule(alt, configured_[alt ? 1 : 0]);
  if (schedule == nullptr && hasPersistentDefaultSchedule(alt)) {
    ensureScheduleForUse(alt);
    schedule = getSchedule(alt, false);
  }
  if (schedule == nullptr) {
    memset(config, 0, sizeof(TChannelConfig_WeeklySchedule));
    fillDefaultSchedule(
        reinterpret_cast<TChannelConfig_WeeklySchedule *>(config), alt);
  } else {
    *reinterpret_cast<TChannelConfig_WeeklySchedule *>(config) = *schedule;
  }
  *size = sizeof(TChannelConfig_WeeklySchedule);
}

void NativeWeeklyScheduleConfigHandler::purgeConfig() {
  eraseSchedule(false);
  if (hasAltScheduleStorage()) {
    eraseSchedule(true);
  }
  cacheRuntime_.reset();
  onNativeSchedulePurged();
}

bool NativeWeeklyScheduleConfigHandler::isConfigured() const {
  return configured_[0] || configured_[1];
}

bool NativeWeeklyScheduleConfigHandler::isConfigured(bool alt) const {
  return configured_[alt ? 1 : 0];
}

bool NativeWeeklyScheduleConfigHandler::isPersisted(bool alt) const {
  return persisted_[alt ? 1 : 0];
}

bool NativeWeeklyScheduleConfigHandler::loadSchedule(bool alt) {
  loadAttempted_[alt ? 1 : 0] = true;
  auto cfg = Supla::Storage::ConfigInstance();
  auto *owner = getScheduleOwner();
  if (cfg == nullptr || owner == nullptr) {
    return false;
  }

  char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
  owner->generateKey(key, getScheduleStorageTag(alt));
  SUPLA_LOG_DEBUG("%s[%d]: loading%s %s from storage",
                  getDeviceLabel(),
                  owner->getChannelNumber(),
                  alt ? " alt" : "",
                  getScheduleLabel(alt));

  auto *schedule = ensureSchedule(alt);
  if (!cfg->getBlob(key,
                    reinterpret_cast<char *>(schedule),
                    sizeof(TChannelConfig_WeeklySchedule))) {
    SUPLA_LOG_DEBUG("%s[%d]: %s%s not found in storage",
                    getDeviceLabel(),
                    owner->getChannelNumber(),
                    getScheduleLabel(alt),
                    alt ? " alt" : "");
    clearSchedule(alt);
    configured_[alt ? 1 : 0] = hasPersistentDefaultSchedule(alt);
    persisted_[alt ? 1 : 0] = false;
    onNativeScheduleLoadFailed(alt);
    return false;
  }

  if (!validateSchedule(schedule, alt)) {
    SUPLA_LOG_WARNING("%s[%d]: loaded%s %s is invalid",
                      getDeviceLabel(),
                      owner->getChannelNumber(),
                      alt ? " alt" : "",
                      getScheduleLabel(alt));
    clearSchedule(alt);
    configured_[alt ? 1 : 0] = hasPersistentDefaultSchedule(alt);
    persisted_[alt ? 1 : 0] = false;
    onNativeScheduleLoadFailed(alt);
    return false;
  }

  configured_[alt ? 1 : 0] = true;
  persisted_[alt ? 1 : 0] = true;
  touchCache(false, millis());
  SUPLA_LOG_DEBUG("%s[%d]: loaded%s %s successfully",
                  getDeviceLabel(),
                  owner->getChannelNumber(),
                  alt ? " alt" : "",
                  getScheduleLabel(alt));
  return true;
}

TChannelConfig_WeeklySchedule *NativeWeeklyScheduleConfigHandler::getSchedule(
    bool alt, bool loadIfMissing) {
  auto *schedule = buffer_.get(alt);
  int index = alt ? 1 : 0;
  if (schedule == nullptr && loadIfMissing &&
      (configured_[index] || hasPersistentDefaultSchedule(alt)) &&
      !loadAttempted_[index]) {
    if (!loadSchedule(alt)) {
      return nullptr;
    }
    schedule = buffer_.get(alt);
  }
  return schedule;
}

const TChannelConfig_WeeklySchedule *
NativeWeeklyScheduleConfigHandler::getSchedule(bool alt,
                                                bool loadIfMissing) const {
  return const_cast<NativeWeeklyScheduleConfigHandler *>(this)->getSchedule(
      alt, loadIfMissing);
}

TChannelConfig_WeeklySchedule *
NativeWeeklyScheduleConfigHandler::ensureSchedule(bool alt) {
  auto *schedule = buffer_.get(alt);
  if (schedule == nullptr) {
    schedule = new TChannelConfig_WeeklySchedule();
    memset(schedule, 0, sizeof(TChannelConfig_WeeklySchedule));
    buffer_.set(alt, schedule);
  }
  configured_[alt ? 1 : 0] = true;
  return schedule;
}

bool NativeWeeklyScheduleConfigHandler::ensureScheduleForUse(bool alt) {
  if (getSchedule(alt, true) != nullptr) {
    return true;
  }
  if (!hasPersistentDefaultSchedule(alt)) {
    return false;
  }
  auto *schedule = ensureSchedule(alt);
  fillDefaultSchedule(schedule, alt);
  saveSchedule(alt, false);
  return true;
}

bool NativeWeeklyScheduleConfigHandler::updateSchedule(
    bool alt, const TChannelConfig_WeeklySchedule &schedule) {
  auto *current = getSchedule(alt, false);
  bool changed = current == nullptr || !configured_[alt ? 1 : 0] ||
                 memcmp(current, &schedule, sizeof(schedule)) != 0;
  if (changed) {
    current = ensureSchedule(alt);
    *current = schedule;
  }
  configured_[alt ? 1 : 0] = true;
  loadAttempted_[alt ? 1 : 0] = true;
  return changed;
}

bool NativeWeeklyScheduleConfigHandler::saveSchedule(bool alt, bool notify) {
  auto cfg = Supla::Storage::ConfigInstance();
  auto *owner = getScheduleOwner();
  auto *schedule = getSchedule(alt, false);
  if (cfg == nullptr || owner == nullptr || schedule == nullptr) {
    return false;
  }

  char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
  owner->generateKey(key, getScheduleStorageTag(alt));
  persisted_[alt ? 1 : 0] =
      cfg->setBlob(key,
                   reinterpret_cast<const char *>(schedule),
                   sizeof(TChannelConfig_WeeklySchedule));
  if (persisted_[alt ? 1 : 0]) {
    SUPLA_LOG_INFO("%s[%d]: %s saved successfully",
                   getDeviceLabel(),
                   owner->getChannelNumber(),
                   getScheduleLabel(alt));
  } else {
    SUPLA_LOG_WARNING("%s[%d]: failed to save %s",
                      getDeviceLabel(),
                      owner->getChannelNumber(),
                      getScheduleLabel(alt));
  }
  cfg->saveWithDelay(5000);
  onNativeScheduleSaved(alt, notify);
  return persisted_[alt ? 1 : 0];
}

void NativeWeeklyScheduleConfigHandler::eraseSchedule(bool alt) {
  auto cfg = Supla::Storage::ConfigInstance();
  auto *owner = getScheduleOwner();
  if (cfg != nullptr && owner != nullptr) {
    char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    owner->generateKey(key, getScheduleStorageTag(alt));
    cfg->eraseKey(key);
  }
  clearSchedule(alt);
  configured_[alt ? 1 : 0] = hasPersistentDefaultSchedule(alt);
  persisted_[alt ? 1 : 0] = false;
  loadAttempted_[alt ? 1 : 0] = true;
}

void NativeWeeklyScheduleConfigHandler::clearSchedule(bool alt) {
  buffer_.clear(alt);
}

void NativeWeeklyScheduleConfigHandler::clearSchedules() {
  buffer_.clearAll();
}

void NativeWeeklyScheduleConfigHandler::unloadSchedule(bool alt) {
  buffer_.clear(alt);
  loadAttempted_[alt ? 1 : 0] = false;
}

int NativeWeeklyScheduleConfigHandler::calculateIndex(
    enum DayOfWeek dayOfWeek, int hour, int quarter) const {
  return buffer_.calculateIndex(dayOfWeek, hour, quarter);
}

int NativeWeeklyScheduleConfigHandler::getProgramId(
    const TChannelConfig_WeeklySchedule *schedule, int index) const {
  return buffer_.getProgramId(schedule, index);
}

bool NativeWeeklyScheduleConfigHandler::setWeeklySchedule(
    TChannelConfig_WeeklySchedule *schedule, int index, int programId) const {
  return buffer_.setWeeklySchedule(schedule, index, programId);
}

TWeeklyScheduleProgram NativeWeeklyScheduleConfigHandler::getProgramById(
    const TChannelConfig_WeeklySchedule *schedule, int programId) const {
  return buffer_.getProgramById(schedule, programId);
}

TWeeklyScheduleProgram NativeWeeklyScheduleConfigHandler::getProgramAt(
    const TChannelConfig_WeeklySchedule *schedule, int quarterIndex) const {
  return buffer_.getProgramAt(schedule, quarterIndex);
}

int NativeWeeklyScheduleConfigHandler::getCurrentQuarter() const {
  return buffer_.getCurrentQuarter();
}

bool NativeWeeklyScheduleConfigHandler::resolveCurrentProgram(
    bool alt, TWeeklyScheduleProgram *program, int *programId) {
  if (!ensureScheduleForUse(alt)) {
    return false;
  }
  return buffer_.resolveCurrentProgram(
      getSchedule(alt, false), program, programId);
}

bool NativeWeeklyScheduleConfigHandler::resolveCurrentProgram(
    bool alt, TWeeklyScheduleProgram *program, int *programId) const {
  return const_cast<NativeWeeklyScheduleConfigHandler *>(this)
      ->resolveCurrentProgram(alt, program, programId);
}

bool NativeWeeklyScheduleConfigHandler::resolveCurrentProgram(
    bool alt,
    const WeeklyScheduleTimeSnapshot &time,
    TWeeklyScheduleProgram *program,
    int *programId) {
  if (!ensureScheduleForUse(alt)) {
    return false;
  }
  int quarterIndex = -1;
  if (time.state == WeeklyScheduleClockState::Ready) {
    quarterIndex = buffer_.calculateIndex(
        time.dayOfWeek, time.hour, time.quarter);
  }
  return buffer_.resolveProgramAt(
      getSchedule(alt, false), quarterIndex, program, programId);
}

bool NativeWeeklyScheduleConfigHandler::resolveProgram(
    const WeeklyScheduleTimeSnapshot &time,
    bool alt,
    TWeeklyScheduleProgram *program,
    int *programId) {
  return resolveCurrentProgram(alt, time, program, programId);
}

void NativeWeeklyScheduleConfigHandler::touchCache(bool active,
                                                   uint32_t nowMs) {
  cacheRuntime_.touch(active, nowMs);
}

bool NativeWeeklyScheduleConfigHandler::processCache(bool active,
                                                     uint32_t nowMs) {
  return cacheRuntime_.process(active, nowMs);
}

void NativeWeeklyScheduleConfigHandler::resetCache() {
  cacheRuntime_.reset();
}

bool NativeWeeklyScheduleController::canActivate() const {
  return isConfigured(false);
}

bool NativeWeeklyScheduleController::isActive() const {
  return enabled_;
}

bool NativeWeeklyScheduleController::switchToWeeklySchedule() {
  if (!isConfigured(false) || getSchedule(false, true) == nullptr) {
    return false;
  }
  enabled_ = true;
  resetCurrentProgramId();
  touchCache(true, millis());
  syncWeeklyScheduleMode(0);
  processCurrentProgram(millis() <= 30000);
  return true;
}

void NativeWeeklyScheduleController::switchToManualMode() {
  enabled_ = false;
  resetCurrentProgramId();
  if (isConfigured(false)) {
    touchCache(false, millis());
  } else {
    resetCache();
  }
  syncWeeklyScheduleMode(0);
}

void NativeWeeklyScheduleController::restoreWeeklyScheduleMode(bool enabled) {
  enabled_ = enabled && isConfigured(false);
  resetCurrentProgramId();
  if (enabled_) {
    touchCache(true, millis());
  }
  syncWeeklyScheduleMode(0);
}

bool NativeWeeklyScheduleController::processWeeklySchedule() {
  if (!isConfigured(false)) {
    return false;
  }
  if (!enabled_) {
    if (processCache(false, millis())) {
      unloadSchedule(false);
    }
    return false;
  }
  touchCache(true, millis());
  return processCurrentProgram(millis() <= 30000);
}

bool NativeWeeklyScheduleController::isWeeklyScheduleConfigured() const {
  return isConfigured(false);
}

bool NativeWeeklyScheduleController::applyWeeklyScheduleMode(
    uint8_t mode, bool programChanged) {
  (void)(programChanged);
  syncWeeklyScheduleMode(mode);
  return true;
}

void NativeWeeklyScheduleController::onNativeScheduleLoaded() {
  enabled_ = false;
  resetCurrentProgramId();
  syncWeeklyScheduleMode(0);
}

void NativeWeeklyScheduleController::onNativeScheduleLoadFailed(bool alt) {
  (void)(alt);
  onNativeScheduleLoaded();
  scheduleWeeklyScheduleStateSave();
}

void NativeWeeklyScheduleController::onNativeScheduleApplied(
    bool alt, bool local, bool changed) {
  (void)(alt);
  (void)(local);
  if (changed) {
    resetCurrentProgramId();
    if (enabled_) {
      processCurrentProgram(millis() <= 30000);
    }
  }
  touchCache(enabled_, millis());
}

void NativeWeeklyScheduleController::onNativeScheduleSaved(bool alt,
                                                            bool notify) {
  (void)(alt);
  (void)(notify);
  touchCache(enabled_, millis());
}

void NativeWeeklyScheduleController::onNativeSchedulePurged() {
  enabled_ = false;
  resetCurrentProgramId();
  syncWeeklyScheduleMode(0);
  scheduleWeeklyScheduleStateSave();
}

bool NativeWeeklyScheduleController::applyResolvedWeeklyScheduleProgram(
    const TWeeklyScheduleProgram &program,
    int programId,
    bool programChanged) {
  return applyWeeklyScheduleMode(
      programId > 0 ? program.Mode : 0, programChanged);
}

}  // namespace Control
}  // namespace Supla

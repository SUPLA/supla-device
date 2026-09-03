// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "weekly_schedule_provider.h"

#include <string.h>

#include "weekly_schedule_storage.h"

namespace Supla {
namespace Control {

bool NativeWeeklyScheduleProvider::isConfigured() const {
  return isWeeklyScheduleConfigured_;
}

bool NativeWeeklyScheduleProvider::loadNativeSchedule(bool alt) {
  auto *schedule = weeklyScheduleBuffer_.get(alt);
  if (!WeeklyScheduleStorage::load(
          getScheduleOwnerChannelNumber(),
          getScheduleOwnerLabel(),
          getScheduleLabel(alt),
          getScheduleStorageTag(alt),
          alt,
          schedule,
          [this](char *key, const char *storageTag) {
            generateScheduleStorageKey(key, storageTag);
          },
          [this, alt](const TChannelConfig_WeeklySchedule *loadedSchedule) {
            return validateNativeSchedule(loadedSchedule, alt);
          })) {
    weeklyScheduleBuffer_.set(alt, schedule);
    weeklyScheduleBuffer_.clear(alt);
    schedulePersisted_[alt ? 1 : 0] = false;
    return false;
  }
  weeklyScheduleBuffer_.set(alt, schedule);
  schedulePersisted_[alt ? 1 : 0] = true;
  return true;
}

bool NativeWeeklyScheduleProvider::saveNativeSchedule(bool alt) {
  auto *schedule = weeklyScheduleBuffer_.get(alt);
  bool persisted = WeeklyScheduleStorage::save(
      getScheduleOwnerChannelNumber(),
      getScheduleOwnerLabel(),
      getScheduleLabel(alt),
      getScheduleStorageTag(alt),
      schedule,
      [this](char *key, const char *storageTag) {
        generateScheduleStorageKey(key, storageTag);
      });
  schedulePersisted_[alt ? 1 : 0] = persisted;
  return persisted;
}

void NativeWeeklyScheduleProvider::eraseNativeSchedule(bool alt) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    generateScheduleStorageKey(key, getScheduleStorageTag(alt));
    cfg->eraseKey(key);
  }
  weeklyScheduleBuffer_.clear(alt);
  schedulePersisted_[alt ? 1 : 0] = false;
}

TChannelConfig_WeeklySchedule *
NativeWeeklyScheduleProvider::getNativeSchedule(bool alt,
                                                bool loadIfMissing) {
  auto *schedule = weeklyScheduleBuffer_.get(alt);
  if (schedule == nullptr && loadIfMissing && !loadNativeSchedule(alt)) {
    return nullptr;
  }
  return weeklyScheduleBuffer_.get(alt);
}

const TChannelConfig_WeeklySchedule *
NativeWeeklyScheduleProvider::getNativeSchedule(bool alt,
                                                bool loadIfMissing) const {
  return const_cast<NativeWeeklyScheduleProvider *>(this)->getNativeSchedule(
      alt, loadIfMissing);
}

bool NativeWeeklyScheduleProvider::isNativeSchedulePersisted(bool alt) const {
  return schedulePersisted_[alt ? 1 : 0];
}

void NativeWeeklyScheduleProvider::resetNativeScheduleLifecycle() {
  weeklyScheduleBuffer_.clearAll();
  isWeeklyScheduleConfigured_ = false;
  schedulePersisted_[0] = false;
  schedulePersisted_[1] = false;
  cacheRuntime_.reset();
}

WeeklyScheduleProviderType
ExternalManagedWeeklyScheduleProvider::getProviderType() const {
  return WeeklyScheduleProviderType::ExternalManaged;
}

void ExternalManagedWeeklyScheduleProvider::onLoadConfig() {
}

bool ExternalManagedWeeklyScheduleProvider::supportsConfigType(
    uint8_t configType) const {
  (void)(configType);
  return false;
}

Supla::ApplyConfigResult
ExternalManagedWeeklyScheduleProvider::applyChannelConfig(
    TSD_ChannelConfig *config, bool local) {
  (void)(config);
  (void)(local);
  return Supla::ApplyConfigResult::NotSupported;
}

void ExternalManagedWeeklyScheduleProvider::fillChannelConfig(
    void *config, int *size, uint8_t configType) {
  (void)(config);
  (void)(configType);
  if (size) {
    *size = 0;
  }
}

void ExternalManagedWeeklyScheduleProvider::purgeConfig() {
}

void ExternalManagedWeeklyScheduleProvider::processCacheRelease() {
}

bool ExternalManagedWeeklyScheduleProvider::isConfigured() const {
  return true;
}

bool ExternalManagedWeeklyScheduleProvider::isActive() const {
  return active_;
}

bool ExternalManagedWeeklyScheduleProvider::switchToWeeklySchedule() {
  active_ = true;
  return true;
}

void ExternalManagedWeeklyScheduleProvider::switchToManualMode() {
  active_ = false;
}

void ExternalManagedWeeklyScheduleProvider::restoreWeeklyScheduleMode(
    bool enabled) {
  active_ = enabled;
}

bool ExternalManagedWeeklyScheduleProvider::processWeeklySchedule() {
  return active_;
}

bool ExternalManagedWeeklyScheduleProvider::isManualActionAllowed(
    bool turnOn) const {
  (void)(turnOn);
  return true;
}

bool ExternalManagedWeeklyScheduleProvider::getCurrentProgram(
    TWeeklyScheduleProgram *program, int *programId) const {
  if (program) {
    memset(program, 0, sizeof(*program));
  }
  if (programId) {
    *programId = -1;
  }
  return false;
}

}  // namespace Control
}  // namespace Supla

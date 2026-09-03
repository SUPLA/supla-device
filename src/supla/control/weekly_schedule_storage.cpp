// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "weekly_schedule_storage.h"

#include <string.h>

#include <supla/log_wrapper.h>
#include <supla/storage/config.h>
#include <supla/storage/storage.h>

namespace Supla {
namespace Control {

bool WeeklyScheduleStorage::load(
    int channelNumber,
    const char *deviceLabel,
    const char *scheduleLabel,
    const char *storageTag,
    bool alt,
    TChannelConfig_WeeklySchedule *&schedule,
    void *context,
    GenerateKeyFn generateKey,
    ValidateFn validate) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (!cfg || generateKey == nullptr || validate == nullptr) {
    return false;
  }

  char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
  generateKey(context, key, storageTag);
  SUPLA_LOG_DEBUG("%s[%d]: loading%s %s from storage",
                  deviceLabel,
                  channelNumber,
                  alt ? " alt" : "",
                  scheduleLabel);
  if (schedule == nullptr) {
    schedule = new TChannelConfig_WeeklySchedule();
    memset(schedule, 0, sizeof(TChannelConfig_WeeklySchedule));
  }

  if (!cfg->getBlob(key,
                    reinterpret_cast<char *>(schedule),
                    sizeof(TChannelConfig_WeeklySchedule))) {
    SUPLA_LOG_DEBUG("%s[%d]: %s%s not found in storage",
                    deviceLabel,
                    channelNumber,
                    scheduleLabel,
                    alt ? " alt" : "");
    return false;
  }

  if (!validate(context, schedule, alt)) {
    SUPLA_LOG_WARNING("%s[%d]: loaded%s %s is invalid",
                      deviceLabel,
                      channelNumber,
                      alt ? " alt" : "",
                      scheduleLabel);
    return false;
  }

  SUPLA_LOG_DEBUG("%s[%d]: loaded%s %s successfully",
                  deviceLabel,
                  channelNumber,
                  alt ? " alt" : "",
                  scheduleLabel);
  return true;
}

bool WeeklyScheduleStorage::save(
    int channelNumber,
    const char *deviceLabel,
    const char *scheduleLabel,
    const char *storageTag,
    const TChannelConfig_WeeklySchedule *schedule,
    void *context,
    GenerateKeyFn generateKey) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (!cfg || schedule == nullptr || generateKey == nullptr) {
    return false;
  }

  char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
  generateKey(context, key, storageTag);
  if (cfg->setBlob(key,
                   reinterpret_cast<const char *>(schedule),
                   sizeof(TChannelConfig_WeeklySchedule))) {
    SUPLA_LOG_INFO("%s[%d]: %s saved successfully",
                   deviceLabel,
                   channelNumber,
                   scheduleLabel);
    return true;
  }

  SUPLA_LOG_WARNING("%s[%d]: failed to save %s",
                    deviceLabel,
                    channelNumber,
                    scheduleLabel);
  return false;
}

bool NativeWeeklyScheduleStorage::load(
    bool alt, const NativeWeeklyScheduleStorageAccess &access) {
  auto *schedule = buffer.get(alt);
  if (!WeeklyScheduleStorage::load(access.channelNumber,
                                   access.deviceLabel,
                                   access.scheduleLabel,
                                   access.storageTag,
                                   alt,
                                   schedule,
                                   access.context,
                                   access.generateKey,
                                   access.validate)) {
    buffer.set(alt, schedule);
    buffer.clear(alt);
    schedulePersisted_[alt ? 1 : 0] = false;
    return false;
  }
  buffer.set(alt, schedule);
  schedulePersisted_[alt ? 1 : 0] = true;
  return true;
}

bool NativeWeeklyScheduleStorage::save(
    bool alt, const NativeWeeklyScheduleStorageAccess &access) {
  bool persisted = WeeklyScheduleStorage::save(access.channelNumber,
                                                access.deviceLabel,
                                                access.scheduleLabel,
                                                access.storageTag,
                                                buffer.get(alt),
                                                access.context,
                                                access.generateKey);
  schedulePersisted_[alt ? 1 : 0] = persisted;
  return persisted;
}

void NativeWeeklyScheduleStorage::erase(
    bool alt, const NativeWeeklyScheduleStorageAccess &access) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg && access.generateKey) {
    char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    access.generateKey(access.context, key, access.storageTag);
    cfg->eraseKey(key);
  }
  buffer.clear(alt);
  schedulePersisted_[alt ? 1 : 0] = false;
}

bool NativeWeeklyScheduleStorage::isPersisted(bool alt) const {
  return schedulePersisted_[alt ? 1 : 0];
}

void NativeWeeklyScheduleStorage::reset() {
  buffer.clearAll();
  configured = false;
  schedulePersisted_[0] = false;
  schedulePersisted_[1] = false;
  cacheRuntime.reset();
}

}  // namespace Control
}  // namespace Supla

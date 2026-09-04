// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "weekly_schedule_storage.h"

#include <string.h>

#include <supla/element.h>
#include <supla/log_wrapper.h>
#include <supla/storage/config.h>
#include <supla/storage/storage.h>

namespace Supla {
namespace Control {

bool WeeklyScheduleStorage::load(
    const Supla::Element &owner,
    const char *deviceLabel,
    const char *scheduleLabel,
    const char *storageTag,
    bool alt,
    TChannelConfig_WeeklySchedule *&schedule,
    void *context,
    ValidateFn validate) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (!cfg || validate == nullptr) {
    return false;
  }

  const int channelNumber = owner.getChannelNumber();
  char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
  owner.generateKey(key, storageTag);
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
    const Supla::Element &owner,
    const char *deviceLabel,
    const char *scheduleLabel,
    const char *storageTag,
    const TChannelConfig_WeeklySchedule *schedule) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (!cfg || schedule == nullptr) {
    return false;
  }

  const int channelNumber = owner.getChannelNumber();
  char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
  owner.generateKey(key, storageTag);
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
    bool alt,
    const Supla::Element &owner,
    const char *deviceLabel,
    const char *scheduleLabel,
    const char *storageTag,
    void *validationContext,
    WeeklyScheduleStorage::ValidateFn validate) {
  auto *schedule = buffer.get(alt);
  if (!WeeklyScheduleStorage::load(owner,
                                   deviceLabel,
                                   scheduleLabel,
                                   storageTag,
                                   alt,
                                   schedule,
                                   validationContext,
                                   validate)) {
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
    bool alt,
    const Supla::Element &owner,
    const char *deviceLabel,
    const char *scheduleLabel,
    const char *storageTag) {
  bool persisted = WeeklyScheduleStorage::save(
      owner, deviceLabel, scheduleLabel, storageTag, buffer.get(alt));
  schedulePersisted_[alt ? 1 : 0] = persisted;
  return persisted;
}

void NativeWeeklyScheduleStorage::erase(
    bool alt, const Supla::Element &owner, const char *storageTag) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    owner.generateKey(key, storageTag);
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

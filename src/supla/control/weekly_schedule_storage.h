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

#ifndef SRC_SUPLA_CONTROL_WEEKLY_SCHEDULE_STORAGE_H_
#define SRC_SUPLA_CONTROL_WEEKLY_SCHEDULE_STORAGE_H_

#include <string.h>

#include <supla-common/proto.h>
#include <supla/log_wrapper.h>
#include <supla/storage/config.h>
#include <supla/storage/storage.h>

#include "weekly_schedule_common.h"

namespace Supla {
namespace Control {

class WeeklyScheduleStorage {
 public:
  template <typename GenerateKeyFn, typename ValidateFn>
  static bool load(int channelNumber,
                   const char *deviceLabel,
                   const char *scheduleLabel,
                   const char *storageTag,
                   bool alt,
                   TChannelConfig_WeeklySchedule *&schedule,
                   GenerateKeyFn &&generateKey,
                   ValidateFn &&validate) {
    auto cfg = Supla::Storage::ConfigInstance();
    if (!cfg) {
      return false;
    }

    if (schedule == nullptr) {
      schedule = new TChannelConfig_WeeklySchedule();
      memset(schedule, 0, sizeof(TChannelConfig_WeeklySchedule));
    }

    char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    generateKey(key, storageTag);
    SUPLA_LOG_DEBUG("%s[%d]: loading%s %s from storage",
                    deviceLabel,
                    channelNumber,
                    alt ? " alt" : "",
                    scheduleLabel);
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

    if (!validate(schedule)) {
      SUPLA_LOG_WARNING("%s[%d]: loaded%s %s is invalid",
                        deviceLabel,
                        channelNumber,
                        alt ? " alt" : "",
                        scheduleLabel);
      delete schedule;
      schedule = nullptr;
      return false;
    }

    SUPLA_LOG_DEBUG("%s[%d]: loaded%s %s successfully",
                    deviceLabel,
                    channelNumber,
                    alt ? " alt" : "",
                    scheduleLabel);
    return true;
  }

  template <typename GenerateKeyFn>
  static bool save(int channelNumber,
                   const char *deviceLabel,
                   const char *scheduleLabel,
                   const char *storageTag,
                   const TChannelConfig_WeeklySchedule *schedule,
                   GenerateKeyFn &&generateKey) {
    auto cfg = Supla::Storage::ConfigInstance();
    if (!cfg || schedule == nullptr) {
      return false;
    }

    char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    generateKey(key, storageTag);
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
};

}  // namespace Control
}  // namespace Supla

#endif  // SRC_SUPLA_CONTROL_WEEKLY_SCHEDULE_STORAGE_H_

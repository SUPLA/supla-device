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

#include <supla-common/proto.h>

#include "weekly_schedule_common.h"
#include "weekly_schedule_buffer.h"
#include "weekly_schedule_cache_runtime.h"

namespace Supla {
class Element;

namespace Control {

class WeeklyScheduleStorage {
 public:
  using ValidateFn = bool (*)(
      void *context,
      const TChannelConfig_WeeklySchedule *schedule,
      bool alt);

  static bool load(const Supla::Element &owner,
                   const char *deviceLabel,
                   const char *scheduleLabel,
                   const char *storageTag,
                   bool alt,
                   TChannelConfig_WeeklySchedule *&schedule,
                   void *context,
                   ValidateFn validate);

  static bool save(const Supla::Element &owner,
                   const char *deviceLabel,
                   const char *scheduleLabel,
                   const char *storageTag,
                   const TChannelConfig_WeeklySchedule *schedule);
};

class NativeWeeklyScheduleStorage {
 public:
  bool load(bool alt,
            const Supla::Element &owner,
            const char *deviceLabel,
            const char *scheduleLabel,
            const char *storageTag,
            void *validationContext,
            WeeklyScheduleStorage::ValidateFn validate);
  bool save(bool alt,
            const Supla::Element &owner,
            const char *deviceLabel,
            const char *scheduleLabel,
            const char *storageTag);
  void erase(bool alt,
             const Supla::Element &owner,
             const char *storageTag);

  bool isConfigured() const {
    return configured_;
  }
  bool isPersisted(bool alt) const {
    return schedulePersisted_[alt ? 1 : 0];
  }
  void reset(bool configured = false);

  TChannelConfig_WeeklySchedule *getSchedule(bool alt) {
    return buffer_.get(alt);
  }
  const TChannelConfig_WeeklySchedule *getSchedule(bool alt) const {
    return buffer_.get(alt);
  }
  TChannelConfig_WeeklySchedule *ensureSchedule(bool alt);
  bool updateSchedule(bool alt,
                      const TChannelConfig_WeeklySchedule &schedule);
  void clearSchedule(bool alt) {
    buffer_.clear(alt);
  }
  void clearSchedules() {
    buffer_.clearAll();
  }

  int calculateIndex(enum DayOfWeek dayOfWeek, int hour, int quarter) const {
    return buffer_.calculateIndex(dayOfWeek, hour, quarter);
  }
  int getProgramId(const TChannelConfig_WeeklySchedule *schedule,
                   int index) const {
    return buffer_.getProgramId(schedule, index);
  }
  bool setWeeklySchedule(TChannelConfig_WeeklySchedule *schedule,
                         int index,
                         int programId) const {
    return buffer_.setWeeklySchedule(schedule, index, programId);
  }
  TWeeklyScheduleProgram getProgramById(
      const TChannelConfig_WeeklySchedule *schedule, int programId) const {
    return buffer_.getProgramById(schedule, programId);
  }
  TWeeklyScheduleProgram getProgramAt(
      const TChannelConfig_WeeklySchedule *schedule, int quarterIndex) const {
    return buffer_.getProgramAt(schedule, quarterIndex);
  }
  int getCurrentQuarter() const {
    return buffer_.getCurrentQuarter();
  }
  int getCurrentProgramId(
      const TChannelConfig_WeeklySchedule *schedule) const {
    return buffer_.getCurrentProgramId(schedule);
  }
  TWeeklyScheduleProgram getCurrentProgram(
      const TChannelConfig_WeeklySchedule *schedule) const {
    return buffer_.getCurrentProgram(schedule);
  }

  void touchCache(bool active, uint32_t nowMs) {
    cacheRuntime_.touch(active, nowMs);
  }
  bool processCache(bool active, uint32_t nowMs) {
    return cacheRuntime_.process(active, nowMs);
  }
  void resetCache() {
    cacheRuntime_.reset();
  }

 private:
  WeeklyScheduleBuffer buffer_;
  WeeklyScheduleCacheRuntime cacheRuntime_;
  bool configured_ = false;
  bool schedulePersisted_[2] = {false, false};
};

}  // namespace Control
}  // namespace Supla

#endif  // SRC_SUPLA_CONTROL_WEEKLY_SCHEDULE_STORAGE_H_

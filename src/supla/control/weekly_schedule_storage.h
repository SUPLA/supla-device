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

  bool isPersisted(bool alt) const;
  void reset();

  WeeklyScheduleBuffer buffer;
  WeeklyScheduleCacheRuntime cacheRuntime;
  bool configured = false;

 private:
  bool schedulePersisted_[2] = {false, false};
};

}  // namespace Control
}  // namespace Supla

#endif  // SRC_SUPLA_CONTROL_WEEKLY_SCHEDULE_STORAGE_H_

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

#include "weekly_schedule_buffer.h"
#include "weekly_schedule_cache_runtime.h"
#include "weekly_schedule_common.h"
#include "weekly_schedule_component.h"

namespace Supla {
class Element;

namespace Control {

class NativeWeeklyScheduleConfigHandler : public WeeklyScheduleConfigHandler {
 public:
  void onLoadConfig() override;
  Supla::ApplyConfigResult applyChannelConfig(TSD_ChannelConfig *config,
                                              bool local) override;
  void fillChannelConfig(void *config,
                         int *size,
                         uint8_t configType) override;
  void purgeConfig() override;
  bool supportsConfigType(uint8_t configType) const override;

 protected:
  virtual Supla::Element *getScheduleOwner() const = 0;
  virtual const char *getDeviceLabel() const = 0;
  virtual const char *getScheduleStorageTag(bool alt) const = 0;
  virtual bool validateSchedule(
      const TChannelConfig_WeeklySchedule *schedule, bool alt) const = 0;
  virtual void fillDefaultSchedule(TChannelConfig_WeeklySchedule *schedule,
                                   bool alt) = 0;
  virtual bool supportsAltSchedule() const {
    return false;
  }
  virtual bool hasAltScheduleStorage() const {
    return false;
  }
  virtual bool hasPersistentDefaultSchedule(bool alt) const {
    (void)(alt);
    return false;
  }
  virtual void onNativeScheduleLoaded() {
  }
  virtual void onNativeScheduleLoadFailed(bool alt) {
    (void)(alt);
  }
  virtual void onNativeScheduleApplied(bool alt, bool local, bool changed) {
    (void)(alt);
    (void)(local);
    (void)(changed);
  }
  virtual void onNativeScheduleSaved(bool alt, bool notify) {
    (void)(alt);
    (void)(notify);
  }
  virtual void onNativeSchedulePurged() {
  }

  bool isConfigured() const;
  bool isConfigured(bool alt) const;
  bool isPersisted(bool alt) const;

  TChannelConfig_WeeklySchedule *getSchedule(bool alt,
                                             bool loadIfMissing = true);
  const TChannelConfig_WeeklySchedule *getSchedule(
      bool alt, bool loadIfMissing = true) const;
  TChannelConfig_WeeklySchedule *ensureSchedule(bool alt);
  bool ensureScheduleForUse(bool alt);
  bool updateSchedule(bool alt,
                      const TChannelConfig_WeeklySchedule &schedule);
  bool saveSchedule(bool alt, bool notify = false);
  void clearSchedule(bool alt);
  void clearSchedules();
  void unloadSchedule(bool alt);

  int calculateIndex(enum DayOfWeek dayOfWeek, int hour, int quarter) const;
  int getProgramId(const TChannelConfig_WeeklySchedule *schedule,
                   int index) const;
  bool setWeeklySchedule(TChannelConfig_WeeklySchedule *schedule,
                         int index,
                         int programId) const;
  TWeeklyScheduleProgram getProgramById(
      const TChannelConfig_WeeklySchedule *schedule, int programId) const;
  TWeeklyScheduleProgram getProgramAt(
      const TChannelConfig_WeeklySchedule *schedule, int quarterIndex) const;
  int getCurrentQuarter() const;
  bool resolveCurrentProgram(bool alt,
                             TWeeklyScheduleProgram *program,
                             int *programId);
  bool resolveCurrentProgram(bool alt,
                             TWeeklyScheduleProgram *program,
                             int *programId) const;

  void touchCache(bool active, uint32_t nowMs);
  bool processCache(bool active, uint32_t nowMs);
  void resetCache();

 private:
  bool loadSchedule(bool alt);
  void eraseSchedule(bool alt);
  bool configTypeToAlt(uint8_t configType, bool *alt) const;
  const char *getScheduleLabel(bool alt) const;

  WeeklyScheduleBuffer buffer_;
  WeeklyScheduleCacheRuntime cacheRuntime_;
  bool configured_[2] = {false, false};
  bool persisted_[2] = {false, false};
  bool loadAttempted_[2] = {false, false};
};

}  // namespace Control
}  // namespace Supla

#endif  // SRC_SUPLA_CONTROL_WEEKLY_SCHEDULE_STORAGE_H_

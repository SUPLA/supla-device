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

#ifndef SRC_SUPLA_CONTROL_HVAC_WEEKLY_SCHEDULE_H_
#define SRC_SUPLA_CONTROL_HVAC_WEEKLY_SCHEDULE_H_

#include <stdint.h>
#include <supla-common/proto.h>

#include "hvac_weekly_schedule_policy.h"
#include "weekly_schedule_component.h"
#include "weekly_schedule_storage.h"

namespace Supla {

namespace Control {

class HvacWeeklySchedule : public WeeklyScheduleController,
                           public NativeWeeklyScheduleConfigHandler {
 public:
  explicit HvacWeeklySchedule(HvacBase *owner);
  ~HvacWeeklySchedule();

  WeeklyScheduleConfigHandler *getConfigHandler() override {
    return this;
  }
  bool canActivate() const override;
  bool isConfigured() const;
  void saveWeeklySchedule(bool requestResend = false);

  bool isActive() const override;
  bool switchToWeeklySchedule() override;
  void switchToManualMode() override;
  void restoreWeeklyScheduleMode(bool enabled) override;
  bool isWeeklyScheduleValid(const TChannelConfig_WeeklySchedule *newSchedule,
                             bool isAltWeeklySchedule = false) const;
  int getWeeklyScheduleProgramId(const TChannelConfig_WeeklySchedule *schedule,
                                 int index) const;
  int calculateIndex(enum DayOfWeek dayOfWeek, int hour, int quarter) const;
  bool isProgramValid(const TWeeklyScheduleProgram &program,
                      bool isAltWeeklySchedule) const;
  bool setProgram(int programId,
                  unsigned char mode,
                  _supla_int16_t tHeat,
                  _supla_int16_t tCool,
                  bool isAltWeeklySchedule = false);
  bool setWeeklySchedule(int index,
                         int programId,
                         bool isAltWeeklySchedule = false);
  bool setWeeklySchedule(enum DayOfWeek dayOfWeek,
                         int hour,
                         int quarter,
                         int programId,
                         bool isAltWeeklySchedule = false);
  int getCurrentQuarter() const;
  TWeeklyScheduleProgram getCurrentProgram() const;
  int getCurrentProgramId() const;
  TWeeklyScheduleProgram getProgramAt(int quarterIndex) const;
  TWeeklyScheduleProgram getProgramById(int programId,
                                        bool isAltWeeklySchedule = false) const;
  bool turnOnWeeklySchedule();
  bool processWeeklySchedule() override;
  void initDefaultWeeklySchedule(bool requestResend = true);
  void processCacheRelease();

 private:
  bool shouldUseAltSchedule() const;
  bool resolveCurrentHvacProgram(TWeeklyScheduleProgram *program,
                                 int *programId) const;
  bool resolveCurrentHvacProgram(const WeeklyScheduleTimeSnapshot &time,
                                 TWeeklyScheduleProgram *program,
                                 int *programId);
  bool resolveWeeklyScheduleProgram(const WeeklyScheduleTimeSnapshot &time,
                                    TWeeklyScheduleProgram *program,
                                    int *programId) override;
  bool applyResolvedWeeklyScheduleProgram(
      const TWeeklyScheduleProgram &program,
      int programId,
      bool programChanged) override;
  void onWeeklyScheduleClockState(WeeklyScheduleClockState state) override;
  void initDefaultWeeklyScheduleForType(bool isAltWeeklySchedule,
                                        bool requestResend);
  void unloadSchedulesIfPossible();
  Supla::Element *getScheduleOwner() const override;
  const char *getDeviceLabel() const override;
  const char *getScheduleStorageTag(bool isAltWeeklySchedule) const override;
  bool validateSchedule(
      const TChannelConfig_WeeklySchedule *schedule,
      bool isAltWeeklySchedule) const override;
  void fillDefaultSchedule(TChannelConfig_WeeklySchedule *schedule,
                           bool isAltWeeklySchedule) override;
  bool supportsAltSchedule() const override;
  bool hasAltScheduleStorage() const override {
    return true;
  }
  bool hasPersistentDefaultSchedule(bool alt) const override;
  void onNativeScheduleLoaded() override;
  void onNativeScheduleSaved(bool alt, bool notify) override;

  HvacBase *owner_ = nullptr;
  HvacWeeklySchedulePolicy policy_;
};

}  // namespace Control
}  // namespace Supla

#endif  // SRC_SUPLA_CONTROL_HVAC_WEEKLY_SCHEDULE_H_

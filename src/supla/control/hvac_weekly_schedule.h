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
#include "weekly_schedule_provider.h"

namespace Supla {

namespace Control {

class HvacWeeklySchedule : public NativeWeeklyScheduleProvider {
 public:
  explicit HvacWeeklySchedule(HvacBase *owner);
  ~HvacWeeklySchedule();

  WeeklyScheduleProviderType getProviderType() const override;
  void onLoadConfig() override;
  bool supportsConfigType(uint8_t configType) const override;
  Supla::ApplyConfigResult applyChannelConfig(TSD_ChannelConfig *config,
                                               bool local) override;
  void fillChannelConfig(void *channelConfig,
                         int *size,
                         uint8_t configType) override;
  void purgeConfig() override;
  void handleSetChannelConfigResult(TSDS_SetChannelConfigResult *result);
  void saveWeeklySchedule(bool requestResend = false);
  void clearWeeklyScheduleChangedFlag();

  bool isActive() const override;
  bool switchToWeeklySchedule() override;
  void switchToManualMode() override;
  void restoreWeeklyScheduleMode(bool enabled) override;
  bool isManualActionAllowed(bool turnOn) const override;
  bool getCurrentProgram(TWeeklyScheduleProgram *program,
                         int *programId) const override;
  bool isWeeklyScheduleChangedOffline() const;
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
  void fillChannelConfig(void *channelConfig,
                         int *size,
                         bool isAltWeeklySchedule);
  bool turnOnWeeklySchedule();
  bool processWeeklySchedule() override;
  void initDefaultWeeklySchedule(bool requestResend = true);
  void processCacheRelease() override;

  friend class HvacWeeklySchedulePolicy;

 private:
  bool ensureScheduleLoaded(bool isAltWeeklySchedule);
  TChannelConfig_WeeklySchedule *getSchedule(bool isAltWeeklySchedule,
                                             bool loadIfMissing = true);
  const TChannelConfig_WeeklySchedule *getSchedule(
      bool isAltWeeklySchedule, bool loadIfMissing = true) const;
  bool loadSchedule(bool isAltWeeklySchedule);
  bool ensureScheduleForUse(bool isAltWeeklySchedule);
  void saveWeeklyScheduleForType(bool isAltWeeklySchedule,
                                 bool requestResend);
  void initDefaultWeeklyScheduleForType(bool isAltWeeklySchedule,
                                        bool requestResend);
  void unloadSchedulesIfPossible();
  int getScheduleOwnerChannelNumber() const override;
  const char *getScheduleOwnerLabel() const override;
  const char *getScheduleLabel(bool isAltWeeklySchedule) const override;
  const char *getScheduleStorageTag(
      bool isAltWeeklySchedule) const override;
  void generateScheduleStorageKey(char *key,
                                  const char *storageTag) const override;
  bool validateNativeSchedule(
      const TChannelConfig_WeeklySchedule *schedule,
      bool isAltWeeklySchedule) const override;

  HvacBase *owner_ = nullptr;
  HvacWeeklySchedulePolicy policy_;
};

}  // namespace Control
}  // namespace Supla

#endif  // SRC_SUPLA_CONTROL_HVAC_WEEKLY_SCHEDULE_H_

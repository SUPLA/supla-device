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

#include <supla-common/proto.h>

#include "hvac_base.h"

namespace Supla {

namespace Control {

class HvacWeeklySchedule {
 public:
  explicit HvacWeeklySchedule(HvacBase *owner);
  ~HvacWeeklySchedule();

  void onLoadConfig();
  void onRegistered();
  void handleChannelConfigFinished();
  bool iterateConfigExchange();
  void releaseCacheIfPossible();
  uint8_t handleWeeklySchedule(TSD_ChannelConfig *newWeeklySchedule,
                               bool isAltWeeklySchedule,
                               bool local);
  void handleSetChannelConfigResult(TSDS_SetChannelConfigResult *result);
  void saveWeeklySchedule();
  void clearWeeklyScheduleChangedFlag();

  bool isConfigured() const;
  bool isWeeklyScheduleValid(TChannelConfig_WeeklySchedule *newSchedule,
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
  bool processWeeklySchedule();
  void initDefaultWeeklySchedule();

 private:
  TChannelConfig_WeeklySchedule *getSchedule(bool isAltWeeklySchedule,
                                             bool loadIfMissing = true);
  const TChannelConfig_WeeklySchedule *getSchedule(
      bool isAltWeeklySchedule, bool loadIfMissing = true) const;
  bool loadSchedule(bool isAltWeeklySchedule);
  void unloadSchedulesIfPossible();
  void markWeeklyScheduleReceived(bool isAltWeeklySchedule);
  void markWeeklyScheduleChangedOffline();
  bool isWeeklyScheduleChangedOffline() const;
  static const char *getStorageTag(bool isAltWeeklySchedule);

  HvacBase *owner_ = nullptr;
  TChannelConfig_WeeklySchedule *weeklySchedule_ = nullptr;
  TChannelConfig_WeeklySchedule *altWeeklySchedule_ = nullptr;
  bool isWeeklyScheduleConfigured_ = false;
  bool weeklyScheduleReceived_ = false;
  bool altWeeklyScheduleReceived_ = false;
  uint8_t weeklyScheduleChangedOffline_ = 0;
};

}  // namespace Control
}  // namespace Supla

#endif  // SRC_SUPLA_CONTROL_HVAC_WEEKLY_SCHEDULE_H_

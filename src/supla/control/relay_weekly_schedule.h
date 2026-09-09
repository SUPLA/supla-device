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

#ifndef SRC_SUPLA_CONTROL_RELAY_WEEKLY_SCHEDULE_H_
#define SRC_SUPLA_CONTROL_RELAY_WEEKLY_SCHEDULE_H_

#include <stdint.h>
#include <supla-common/proto.h>

#include "../element_with_channel_actions.h"
#include "weekly_schedule_component.h"
#include "weekly_schedule_storage.h"

namespace Supla {
namespace Control {

class Relay;

class RelayWeeklySchedule : public NativeWeeklyScheduleController {
 public:
  explicit RelayWeeklySchedule(Relay *owner);
  ~RelayWeeklySchedule();

  bool isConfigured() const;
  bool iterateAlways();

  bool canActivate() const override;
  bool switchToWeeklySchedule() override;
  void restoreWeeklyScheduleMode(bool enabled) override;
  bool isWeeklyScheduleEnabled() const;
  bool isManualActionAllowed(bool turnOn) const override;
  void onManualAction() override;

 private:
  bool requiresReadyClock() const override { return true; }
  void onWeeklyScheduleClockState(WeeklyScheduleClockState state) override;
  bool applyProgramAt(const WeeklyScheduleTimeSnapshot &time,
                      const TWeeklyScheduleProgram &program,
                      int programId, bool programChanged) override;
  void syncWeeklyScheduleMode(uint8_t programMode) override;
  void scheduleWeeklyScheduleStateSave() override;
  bool isWeeklyScheduleValid(
      const TChannelConfig_WeeklySchedule *newSchedule) const;
  bool isProgramValid(const TWeeklyScheduleProgram &program) const;
  bool applyWeeklyScheduleMode(uint8_t mode, bool programChanged) override;
  uint8_t getCurrentProgramMode() const;

  Supla::Element *getScheduleOwner() const override;
  const char *getDeviceLabel() const override;
  const char *getScheduleStorageTag(bool alt) const override;
  bool validateSchedule(const TChannelConfig_WeeklySchedule *schedule,
                        bool alt) const override;
  void fillDefaultSchedule(TChannelConfig_WeeklySchedule *schedule,
                           bool alt) override;
  void onNativeScheduleApplied(bool alt, bool local, bool changed) override;
  void resetRuntimeOverride();

  Relay *owner_ = nullptr;
  int32_t occurrence_ = -1;
  int32_t lastTimingQuarter_ = -1;
  uint32_t phase_ = UINT32_MAX;
  bool timed_ = false;
  bool suppressed_ = false;
  bool programResolved_ = false;
  bool pendingManualAction_ = false;
};

}  // namespace Control
}  // namespace Supla

#endif  // SRC_SUPLA_CONTROL_RELAY_WEEKLY_SCHEDULE_H_

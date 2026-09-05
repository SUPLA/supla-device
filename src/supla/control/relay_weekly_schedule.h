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

class RelayWeeklySchedule : public WeeklyScheduleController,
                            public NativeWeeklyScheduleConfigHandler {
 public:
  explicit RelayWeeklySchedule(Relay *owner);
  ~RelayWeeklySchedule();

  bool canActivate() const override;
  bool isConfigured() const;
  bool iterateAlways();
  bool switchToWeeklySchedule() override;
  void switchToManualMode() override;
  void restoreWeeklyScheduleMode(bool enabled) override;
  bool processWeeklySchedule() override;

  bool isWeeklyScheduleEnabled() const;
  bool isActive() const override;
  bool isManualActionAllowed(bool turnOn) const override;
  void processCacheRelease();

 private:
  void syncRelayMode(uint8_t programMode);
  void unloadScheduleIfPossible();
  void setWeeklyScheduleEnabled(bool enabled);
  const TChannelConfig_WeeklySchedule *getSchedule(bool loadIfMissing = true)
      const;
  TChannelConfig_WeeklySchedule *getSchedule(bool loadIfMissing = true);
  bool isWeeklyScheduleValid(
      const TChannelConfig_WeeklySchedule *newSchedule) const;
  bool isProgramValid(const TWeeklyScheduleProgram &program) const;
  bool applyResolvedWeeklyScheduleProgram(
      const TWeeklyScheduleProgram &program,
      int programId,
      bool programChanged) override;
  uint8_t getCurrentProgramMode() const;
  bool applyCurrentState();

  Supla::Element *getScheduleOwner() const override;
  const char *getDeviceLabel() const override;
  const char *getScheduleStorageTag(bool alt) const override;
  bool validateSchedule(const TChannelConfig_WeeklySchedule *schedule,
                        bool alt) const override;
  void fillDefaultSchedule(TChannelConfig_WeeklySchedule *schedule,
                           bool alt) override;
  void onNativeScheduleLoaded() override;
  void onNativeScheduleLoadFailed(bool alt) override;
  void onNativeScheduleApplied(bool alt,
                               bool local,
                               bool changed) override;
  void onNativeScheduleSaved(bool alt, bool notify) override;
  void onNativeSchedulePurged() override;

  Relay *owner_ = nullptr;
  bool weeklyScheduleEnabled_ = false;
};

}  // namespace Control
}  // namespace Supla

#endif  // SRC_SUPLA_CONTROL_RELAY_WEEKLY_SCHEDULE_H_

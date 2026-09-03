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
#include "weekly_schedule_provider.h"

namespace Supla {
namespace Control {

class Relay;

class RelayWeeklySchedule : public NativeWeeklyScheduleProvider {
 public:
  explicit RelayWeeklySchedule(Relay *owner);
  ~RelayWeeklySchedule();

  WeeklyScheduleProviderType getProviderType() const override;
  void onLoadConfig() override;
  bool iterateAlways();
  Supla::ApplyConfigResult applyChannelConfig(TSD_ChannelConfig *result,
                                              bool local = false) override;
  void fillChannelConfig(void *channelConfig,
                         int *size,
                         uint8_t configType) override;
  void purgeConfig() override;
  bool supportsConfigType(uint8_t configType) const override;
  bool switchToWeeklySchedule() override;
  void switchToManualMode() override;
  void restoreWeeklyScheduleMode(bool enabled) override;
  bool processWeeklySchedule() override;

  bool isWeeklyScheduleEnabled() const;
  bool isActive() const override;
  bool isManualActionAllowed(bool turnOn) const override;
  bool getCurrentProgram(TWeeklyScheduleProgram *program,
                         int *programId) const override;
  void processCacheRelease() override;

 private:
  bool loadSchedule();
  void saveWeeklySchedule();
  void syncRelayMode(uint8_t programMode);
  void unloadScheduleIfPossible();
  void setWeeklyScheduleEnabled(bool enabled);
  const TChannelConfig_WeeklySchedule *getSchedule(bool loadIfMissing = true)
      const;
  TChannelConfig_WeeklySchedule *getSchedule(bool loadIfMissing = true);
  bool isWeeklyScheduleValid(
      const TChannelConfig_WeeklySchedule *newSchedule) const;
  bool isNoOpSchedule(
      const TChannelConfig_WeeklySchedule *newSchedule) const;
  void clearSchedule(bool eraseStorage = true);
  bool isProgramValid(const TWeeklyScheduleProgram &program) const;
  uint8_t getCurrentProgramMode() const;
  void applyCurrentState();
  bool isWaitingForClock() const;

  int getScheduleOwnerChannelNumber() const override;
  const char *getScheduleOwnerLabel() const override;
  const char *getScheduleLabel(bool alt) const override;
  const char *getScheduleStorageTag(bool alt) const override;
  void generateScheduleStorageKey(char *key,
                                  const char *storageTag) const override;
  bool validateNativeSchedule(
      const TChannelConfig_WeeklySchedule *schedule, bool alt) const override;

  Relay *owner_ = nullptr;
  bool weeklyScheduleEnabled_ = false;
  uint8_t weeklyScheduleChangedOffline_ = 0;
  int lastCurrentProgramId_ = -1;
  bool startupDelay_ = true;
};

}  // namespace Control
}  // namespace Supla

#endif  // SRC_SUPLA_CONTROL_RELAY_WEEKLY_SCHEDULE_H_

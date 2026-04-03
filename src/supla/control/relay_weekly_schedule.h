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
#include "weekly_schedule_buffer.h"

namespace Supla {
namespace Control {

class Relay;

class RelayWeeklySchedule {
 public:
  explicit RelayWeeklySchedule(Relay *owner);
  ~RelayWeeklySchedule();

  void onLoadConfig();
  bool iterateAlways();
  Supla::ApplyConfigResult applyChannelConfig(TSD_ChannelConfig *result,
                                              bool local = false);
  void fillChannelConfig(void *channelConfig,
                         int *size,
                         uint8_t configType);
  bool switchToWeeklySchedule();
  void switchToManualMode();

  bool isConfigured() const;
  bool isWeeklyScheduleEnabled() const;
  bool isManualActionAllowed(bool turnOn) const;

 private:
  bool loadSchedule();
  void initDefaultWeeklySchedule();
  void saveWeeklySchedule();
  void syncRelayMode(uint8_t programMode);
  void setWeeklyScheduleEnabled(bool enabled);
  const TChannelConfig_WeeklySchedule *getSchedule(bool loadIfMissing = true)
      const;
  TChannelConfig_WeeklySchedule *getSchedule(bool loadIfMissing = true);
  bool isWeeklyScheduleValid(
      const TChannelConfig_WeeklySchedule *newSchedule) const;
  bool isProgramValid(const TWeeklyScheduleProgram &program) const;
  uint8_t getCurrentProgramMode() const;
  void applyCurrentState();

  static const char *getStorageTag();

  Relay *owner_ = nullptr;
  WeeklyScheduleBuffer weeklyScheduleBuffer_;
  bool isWeeklyScheduleConfigured_ = false;
  bool weeklyScheduleEnabled_ = false;
  uint8_t weeklyScheduleChangedOffline_ = 0;
  int lastCurrentProgramId_ = -1;
};

}  // namespace Control
}  // namespace Supla

#endif  // SRC_SUPLA_CONTROL_RELAY_WEEKLY_SCHEDULE_H_

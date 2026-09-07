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

#include "relay_weekly_schedule.h"

#include <string.h>
#include <supla/log_wrapper.h>
#include <supla/protocol/protocol_layer.h>
#include <supla/storage/config.h>
#include <supla/storage/config_tags.h>
#include <supla/storage/storage.h>
#include <supla/time.h>

#include "relay.h"
#include "weekly_schedule_common.h"

namespace Supla {
namespace Control {

RelayWeeklySchedule::RelayWeeklySchedule(Relay *owner) : owner_(owner) {
}

RelayWeeklySchedule::~RelayWeeklySchedule() {
}

bool RelayWeeklySchedule::isConfigured() const {
  return isWeeklyScheduleConfigured();
}

Supla::Element *RelayWeeklySchedule::getScheduleOwner() const {
  return owner_;
}

const char *RelayWeeklySchedule::getDeviceLabel() const {
  return "Relay";
}

const char *RelayWeeklySchedule::getScheduleStorageTag(bool alt) const {
  (void)(alt);
  return Supla::ConfigTag::RelayWeeklyCfgTag;
}

bool RelayWeeklySchedule::validateSchedule(
    const TChannelConfig_WeeklySchedule *schedule, bool alt) const {
  (void)(alt);
  return isWeeklyScheduleValid(schedule);
}

void RelayWeeklySchedule::fillDefaultSchedule(
    TChannelConfig_WeeklySchedule *schedule, bool alt) {
  (void)(alt);
  if (owner_ != nullptr) {
    owner_->fillDefaultWeeklySchedule(schedule);
  }
}

bool RelayWeeklySchedule::isWeeklyScheduleEnabled() const {
  return isActive();
}

void RelayWeeklySchedule::syncWeeklyScheduleMode(uint8_t programMode) {
  if (owner_ == nullptr) {
    return;
  }
  owner_->getChannel()->setRelayWeeklyScheduleEnabled(isActive());
  owner_->getChannel()->setRelayMode(
      isActive() ? programMode : SUPLA_RELAY_MODE_NOT_SET);
}

void RelayWeeklySchedule::scheduleWeeklyScheduleStateSave() {
  Supla::Storage::ScheduleSave(Relay::relayStorageSaveDelay, 2000);
}

bool RelayWeeklySchedule::isManualActionAllowed(bool turnOn) const {
  if (!isActive()) {
    return true;
  }
  auto currentProgramMode = getCurrentProgramMode();
  if (turnOn) {
    return currentProgramMode != SUPLA_RELAY_MODE_FORCED_OFF;
  }
  return currentProgramMode != SUPLA_RELAY_MODE_FORCED_ON;
}

bool RelayWeeklySchedule::isProgramValid(
    const TWeeklyScheduleProgram &program) const {
  return owner_ != nullptr &&
         owner_->isWeeklyScheduleProgramModeSupported(program.Mode);
}

bool RelayWeeklySchedule::isWeeklyScheduleValid(
    const TChannelConfig_WeeklySchedule *newSchedule) const {
  if (newSchedule == nullptr) {
    return false;
  }

  for (int i = 0; i < SUPLA_WEEKLY_SCHEDULE_PROGRAMS_MAX_SIZE; i++) {
    if (!isProgramValid(newSchedule->Program[i])) {
      SUPLA_LOG_WARNING("Relay[%d]: invalid weekly schedule program %d",
                        owner_->getChannelNumber(),
                        i);
      return false;
    }
  }

  for (int i = 0; i < SUPLA_WEEKLY_SCHEDULE_VALUES_SIZE; i++) {
    int programId = getProgramId(newSchedule, i);
    if (programId < 0 || programId > SUPLA_WEEKLY_SCHEDULE_PROGRAMS_MAX_SIZE) {
      SUPLA_LOG_WARNING(
          "Relay[%d]: weekly schedule references invalid program %d",
          owner_->getChannelNumber(),
          programId);
      return false;
    }
  }

  return true;
}

bool RelayWeeklySchedule::iterateAlways() {
  return processWeeklySchedule();
}

bool RelayWeeklySchedule::applyWeeklyScheduleMode(
    uint8_t currentProgramMode,
    bool programChanged) {
  syncWeeklyScheduleMode(currentProgramMode);

  if (currentProgramMode == SUPLA_RELAY_MODE_NOT_SET) {
    return true;
  }

  owner_->applyWeeklyScheduleProgram(currentProgramMode, programChanged);
  return true;
}

uint8_t RelayWeeklySchedule::getCurrentProgramMode() const {
  if (!isConfigured()) {
    return SUPLA_RELAY_MODE_NOT_SET;
  }
  TWeeklyScheduleProgram program = {};
  int currentProgramId = -1;
  auto time = getWeeklyScheduleTimeSnapshot(millis() <= 30000);
  auto *self = const_cast<RelayWeeklySchedule *>(this);
  if (time.state == WeeklyScheduleClockState::Waiting ||
      !self->resolveWeeklyScheduleProgram(
          time, &program, &currentProgramId)) {
    return SUPLA_RELAY_MODE_NOT_SET;
  }

  if (currentProgramId <= 0) {
    return SUPLA_RELAY_MODE_NOT_SET;
  }

  return program.Mode;
}

}  // namespace Control
}  // namespace Supla

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

#include "hvac_weekly_schedule_policy.h"

#include <string.h>
#include <supla/channel_function_string.h>
#include <supla/clock/clock.h>
#include <supla/log_wrapper.h>

#include "hvac_base.h"
#include "hvac_weekly_schedule.h"

namespace Supla {
namespace Control {

bool HvacWeeklySchedulePolicy::isProgramValid(
    const HvacBase &owner,
    const TWeeklyScheduleProgram &program,
    bool isAltWeeklySchedule) const {
  if (program.Mode == SUPLA_HVAC_MODE_NOT_SET) {
    return true;
  }

  if (program.Mode != SUPLA_HVAC_MODE_COOL &&
      program.Mode != SUPLA_HVAC_MODE_HEAT &&
      program.Mode != SUPLA_HVAC_MODE_HEAT_COOL) {
    return false;
  }

  auto channelFunction = owner.getChannel()->getDefaultFunction();
  if (channelFunction == SUPLA_CHANNELFNC_HVAC_THERMOSTAT) {
    if (program.Mode == SUPLA_HVAC_MODE_HEAT) {
      if (isAltWeeklySchedule) {
        return false;
      }
      if (!owner.isHeatingAndCoolingSupported()) {
        return false;
      }
    } else if (program.Mode == SUPLA_HVAC_MODE_COOL) {
      if (!isAltWeeklySchedule) {
        return false;
      }
      if (!owner.isHeatingAndCoolingSupported()) {
        return false;
      }
    } else if (program.Mode != SUPLA_HVAC_MODE_NOT_SET &&
               program.Mode != SUPLA_HVAC_MODE_OFF) {
      return false;
    }
  } else if (!owner.isModeSupported(program.Mode)) {
    return false;
  }

  switch (program.Mode) {
    case SUPLA_HVAC_MODE_HEAT: {
      return owner.isTemperatureInMainConstrain(
          program.SetpointTemperatureHeat);
    }
    case SUPLA_HVAC_MODE_COOL: {
      return owner.isTemperatureInMainConstrain(
          program.SetpointTemperatureCool);
    }
    case SUPLA_HVAC_MODE_HEAT_COOL: {
      return owner.isTemperatureInHeatCoolConstrain(
          program.SetpointTemperatureHeat, program.SetpointTemperatureCool);
    }
    default: {
      return false;
    }
  }
}

bool HvacWeeklySchedulePolicy::turnOnWeeklySchedule(
    HvacWeeklySchedule &weeklySchedule) const {
  if (!weeklySchedule.isConfigured()) {
    return false;
  }

  weeklySchedule.owner_->setWeeklyScheduleEnabled(true);
  return processWeeklySchedule(weeklySchedule);
}

bool HvacWeeklySchedulePolicy::processWeeklySchedule(
    HvacWeeklySchedule &weeklySchedule) const {
  auto *owner = weeklySchedule.owner_;
  if (!owner->isWeeklyScheduleEnabled()) {
    SUPLA_LOG_WARNING(
        "HVAC[%d]: processs weekly schedule failed - it is not "
        "enabled",
        owner->getChannelNumber());
    return false;
  }

  if (!Supla::Clock::IsReady()) {
    if (owner->isWeeklyScheduleStartupDelay()) {
      SUPLA_LOG_DEBUG(
          "HVAC[%d]: Weekly schedule enabled, clock not ready -> "
          "startup delay...",
          owner->getChannelNumber());
      return false;
    }

    if (!owner->isWeeklyScheduleClockError()) {
      SUPLA_LOG_WARNING(
          "HVAC[%d]: processs weekly schedule failed - clock is "
          "not ready",
          owner->getChannelNumber());
    }
    owner->setWeeklyScheduleClockError(true);
  } else {
    owner->setWeeklyScheduleClockError(false);
  }

  TWeeklyScheduleProgram program = weeklySchedule.getCurrentProgram();
  if (program.Mode == SUPLA_HVAC_MODE_NOT_SET) {
    SUPLA_LOG_INFO("HVAC[%d]: Invalid program mode. Disabling schedule.",
                   owner->getChannelNumber());
    owner->setTargetMode(SUPLA_HVAC_MODE_OFF, false);
    return false;
  }

  if (owner->isWeelkySchedulManualOverrideMode()) {
    int currentProgramId = weeklySchedule.getCurrentProgramId();
    if (currentProgramId !=
        owner->getWeeklyScheduleLastProgramManualOverride()) {
      SUPLA_LOG_DEBUG("HVAC[%d]: leaving manual override mode",
                      owner->getChannelNumber());
      owner->setWeeklyScheduleLastProgramManualOverride(-1);
    } else {
      if (owner->getMode() == SUPLA_HVAC_MODE_OFF) {
        int mode = owner->getWeeklyScheduleLastManualMode();
        if (mode == 0) {
          mode = owner->getDefaultManualMode();
        }
        SUPLA_LOG_DEBUG("HVAC[%d]: Manual override mode %d",
                        owner->getChannelNumber(),
                        mode);
        owner->setTargetMode(mode, true);
      }
      if (!owner->isWeeklyScheduleTemporalOverride()) {
        SUPLA_LOG_DEBUG("HVAC[%d]: Manual override mode",
                        owner->getChannelNumber());
      }
      owner->setWeeklyScheduleTemporalOverride(true);
      return true;
    }
  }
  owner->setWeeklyScheduleTemporalOverride(false);
  owner->setTargetMode(program.Mode, true);
  int16_t tHeat = program.SetpointTemperatureHeat;
  int16_t tCool = program.SetpointTemperatureCool;
  if (program.Mode == SUPLA_HVAC_MODE_HEAT) {
    tCool = INT16_MIN;
  }
  if (program.Mode == SUPLA_HVAC_MODE_COOL) {
    tHeat = INT16_MIN;
  }
  owner->applyWeeklyScheduleSetpoints(tHeat, tCool);
  return true;
}

void HvacWeeklySchedulePolicy::initDefaultWeeklySchedule(
    HvacWeeklySchedule &weeklySchedule) const {
  auto *owner = weeklySchedule.owner_;
  weeklySchedule.isWeeklyScheduleConfigured_ = true;
  auto prevInitDone = owner->isInitDone();
  if (owner->isInitDone()) {
    weeklySchedule.weeklyScheduleChangedOffline_ = 1;
    owner->setInitDone(false);
  }

  weeklySchedule.weeklyScheduleBuffer_.clearAll();
  weeklySchedule.weeklyScheduleBuffer_.set(false,
                                           new TChannelConfig_WeeklySchedule());
  weeklySchedule.weeklyScheduleBuffer_.set(true,
                                           new TChannelConfig_WeeklySchedule());
  memset(weeklySchedule.weeklyScheduleBuffer_.get(false),
         0,
         sizeof(TChannelConfig_WeeklySchedule));
  memset(weeklySchedule.weeklyScheduleBuffer_.get(true),
         0,
         sizeof(TChannelConfig_WeeklySchedule));

  switch (owner->getChannel()->getDefaultFunction()) {
    default: {
      SUPLA_LOG_WARNING(
          "HVAC[%d]: no default weekly schedule defined for "
          "function %s (%d)",
          owner->getChannelNumber(),
          Supla::channelFunctionToString(
              owner->getChannel()->getDefaultFunction()),
          owner->getChannel()->getDefaultFunction());
      break;
    }
    case SUPLA_CHANNELFNC_HVAC_THERMOSTAT: {
      weeklySchedule.setProgram(1, SUPLA_HVAC_MODE_HEAT, 1900, 0);
      weeklySchedule.setProgram(2, SUPLA_HVAC_MODE_HEAT, 2100, 0);
      weeklySchedule.setProgram(3, SUPLA_HVAC_MODE_HEAT, 3000, 0);
      weeklySchedule.setProgram(4, SUPLA_HVAC_MODE_HEAT, 1200, 0);

      weeklySchedule.setProgram(1, SUPLA_HVAC_MODE_COOL, 0, 2400, true);
      weeklySchedule.setProgram(2, SUPLA_HVAC_MODE_COOL, 0, 2100, true);
      weeklySchedule.setProgram(3, SUPLA_HVAC_MODE_COOL, 0, 1800, true);
      weeklySchedule.setProgram(4, SUPLA_HVAC_MODE_COOL, 0, 2800, true);
      break;
    }
    case SUPLA_CHANNELFNC_HVAC_THERMOSTAT_HEAT_COOL: {
      weeklySchedule.setProgram(1, SUPLA_HVAC_MODE_HEAT_COOL, 1800, 2500);
      weeklySchedule.setProgram(2, SUPLA_HVAC_MODE_HEAT_COOL, 2100, 2400);
      weeklySchedule.setProgram(3, SUPLA_HVAC_MODE_HEAT, 2300, 0);
      weeklySchedule.setProgram(4, SUPLA_HVAC_MODE_COOL, 0, 2400);
      break;
    }
    case SUPLA_CHANNELFNC_HVAC_THERMOSTAT_DIFFERENTIAL: {
      weeklySchedule.setProgram(1, SUPLA_HVAC_MODE_HEAT, -500, 0);
      weeklySchedule.setProgram(2, SUPLA_HVAC_MODE_HEAT, -200, 0);
      weeklySchedule.setProgram(3, SUPLA_HVAC_MODE_HEAT, -1000, 0);
      weeklySchedule.setProgram(4, SUPLA_HVAC_MODE_HEAT, -1500, 0);
      break;
    }
    case SUPLA_CHANNELFNC_HVAC_DOMESTIC_HOT_WATER: {
      weeklySchedule.setProgram(1, SUPLA_HVAC_MODE_HEAT, 4000, 0);
      weeklySchedule.setProgram(2, SUPLA_HVAC_MODE_HEAT, 5000, 0);
      weeklySchedule.setProgram(3, SUPLA_HVAC_MODE_HEAT, 3000, 0);
      weeklySchedule.setProgram(4, SUPLA_HVAC_MODE_HEAT, 6000, 0);
      break;
    }
  }

  auto channelFunction = owner->getChannel()->getDefaultFunction();
  if (channelFunction == SUPLA_CHANNELFNC_HVAC_THERMOSTAT ||
      channelFunction == SUPLA_CHANNELFNC_HVAC_DOMESTIC_HOT_WATER ||
      channelFunction == SUPLA_CHANNELFNC_HVAC_THERMOSTAT_DIFFERENTIAL) {
    for (int dayOfAWeek = 0; dayOfAWeek < 7; dayOfAWeek++) {
      int program = 1;
      for (int hour = 0; hour < 24; hour++) {
        if (hour >= 6 && hour < 21) {
          program = 2;
        } else {
          program = 1;
        }
        for (int quarter = 0; quarter < 4; quarter++) {
          weeklySchedule.setWeeklySchedule(
              static_cast<enum DayOfWeek>(dayOfAWeek), hour, quarter, program);
        }
      }
    }
  }
  if (channelFunction == SUPLA_CHANNELFNC_HVAC_THERMOSTAT) {
    for (int dayOfAWeek = 0; dayOfAWeek < 7; dayOfAWeek++) {
      int program = 0;
      for (int hour = 0; hour < 24; hour++) {
        if (hour >= 6 && hour < 21) {
          program = 1;
        } else {
          program = 0;
        }
        for (int quarter = 0; quarter < 4; quarter++) {
          weeklySchedule.setWeeklySchedule(
              static_cast<enum DayOfWeek>(dayOfAWeek),
              hour,
              quarter,
              program,
              true);
        }
      }
    }
  }
  if (channelFunction == SUPLA_CHANNELFNC_HVAC_THERMOSTAT_HEAT_COOL) {
    for (int dayOfAWeek = 0; dayOfAWeek < 7; dayOfAWeek++) {
      int program = 1;
      for (int hour = 0; hour < 24; hour++) {
        if (hour >= 6 && hour < 21) {
          program = 2;
        } else {
          program = 1;
        }
        for (int quarter = 0; quarter < 4; quarter++) {
          weeklySchedule.setWeeklySchedule(
              static_cast<enum DayOfWeek>(dayOfAWeek), hour, quarter, program);
        }
      }
    }
  }

  owner->setInitDone(prevInitDone);
  weeklySchedule.saveWeeklySchedule();
}

}  // namespace Control
}  // namespace Supla

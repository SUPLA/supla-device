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

#include "hvac_base.h"

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

}  // namespace Control
}  // namespace Supla

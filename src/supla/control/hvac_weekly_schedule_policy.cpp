// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

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

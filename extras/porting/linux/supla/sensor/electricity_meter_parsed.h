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

#ifndef EXTRAS_PORTING_LINUX_SUPLA_SENSOR_ELECTRICITY_METER_PARSED_H_
#define EXTRAS_PORTING_LINUX_SUPLA_SENSOR_ELECTRICITY_METER_PARSED_H_

#include <supla/parser/parser.h>
#include <supla/sensor/electricity_meter.h>

#include <string>

#include "sensor_parsed.h"

namespace Supla {
namespace Parser {
// Units kWh
const char FwdActEnergy1[] = "fwd_act_energy_1";
const char FwdActEnergy2[] = "fwd_act_energy_2";
const char FwdActEnergy3[] = "fwd_act_energy_3";

// Units kWh
const char RvrActEnergy1[] = "rvr_act_energy_1";
const char RvrActEnergy2[] = "rvr_act_energy_2";
const char RvrActEnergy3[] = "rvr_act_energy_3";

// Units kVA
const char FwdReactEnergy1[] = "fwd_react_energy_1";
const char FwdReactEnergy2[] = "fwd_react_energy_2";
const char FwdReactEnergy3[] = "fwd_react_energy_3";

// Units kVA
const char RvrReactEnergy1[] = "rvr_react_energy_1";
const char RvrReactEnergy2[] = "rvr_react_energy_2";
const char RvrReactEnergy3[] = "rvr_react_energy_3";

// Units V
const char Voltage1[] = "voltage_1";
const char Voltage2[] = "voltage_2";
const char Voltage3[] = "voltage_3";

// Units A
const char Current1[] = "current_1";
const char Current2[] = "current_2";
const char Current3[] = "current_3";

// Units Hz
const char Frequency[] = "frequency";

// Units W
const char PowerActive1[] = "power_active_1";
const char PowerActive2[] = "power_active_2";
const char PowerActive3[] = "power_active_3";

// Units W
const char RvrPowerActive1[] = "rvr_power_active_1";
const char RvrPowerActive2[] = "rvr_power_active_2";
const char RvrPowerActive3[] = "rvr_power_active_3";

// Units VAR
const char PowerReactive1[] = "power_reactive_1";
const char PowerReactive2[] = "power_reactive_2";
const char PowerReactive3[] = "power_reactive_3";

// Units VA
const char PowerApparent1[] = "power_apparent_1";
const char PowerApparent2[] = "power_apparent_2";
const char PowerApparent3[] = "power_apparent_3";

// Units 1
const char PhaseAngle1[] = "phase_angle_1";
const char PhaseAngle2[] = "phase_angle_2";
const char PhaseAngle3[] = "phase_angle_3";

// Units 1
const char PowerFactor1[] = "power_factor_1";
const char PowerFactor2[] = "power_factor_2";
const char PowerFactor3[] = "power_factor_3";
};  // namespace Parser

namespace Sensor {

class ElectricityMeterParsed : public SensorParsed<ElectricityMeter> {
 public:
  explicit ElectricityMeterParsed(Supla::Parser::Parser *);

  void readValuesFromDevice() override;
  void onInit() override;

 protected:
  bool isDataErrorLogged = false;
};
};  // namespace Sensor
};  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_SUPLA_SENSOR_ELECTRICITY_METER_PARSED_H_

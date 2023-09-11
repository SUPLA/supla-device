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

#include <string.h>
#include <supla/time.h>
#include <supla/log_wrapper.h>
#include <supla/actions.h>

#include "../condition.h"
#include "../events.h"
#include "electricity_meter.h"

Supla::Sensor::ElectricityMeter::ElectricityMeter()
    : valueChanged(false), lastReadTime(0), refreshRateSec(5) {
  extChannel.setType(SUPLA_CHANNELTYPE_ELECTRICITY_METER);
  extChannel.setDefault(SUPLA_CHANNELFNC_ELECTRICITY_METER);
  extChannel.setFlag(SUPLA_CHANNEL_FLAG_CALCFG_RESET_COUNTERS);

  memset(&emValue, 0, sizeof(emValue));
  emValue.period = 5;
  for (int i = 0; i < MAX_PHASES; i++) {
    rawCurrent[i] = 0;
  }
  currentMeasurementAvailable = false;
}

void Supla::Sensor::ElectricityMeter::updateChannelValues() {
  if (!valueChanged) {
    return;
  }
  valueChanged = false;

  emValue.m_count = 1;
  // Update current messurement precision based on last updates
  if (currentMeasurementAvailable) {
    bool over65A = false;
    for (int i = 0; i < MAX_PHASES; i++) {
      if (rawCurrent[i] > 65000) {
        over65A = true;
      }
    }

    for (int i = 0; i < MAX_PHASES; i++) {
      if (over65A) {
        emValue.m[0].current[i] = rawCurrent[i] / 10;
      } else {
        emValue.m[0].current[i] = rawCurrent[i];
      }
    }

    if (over65A) {
      emValue.measured_values &= (~EM_VAR_CURRENT);
      emValue.measured_values |= EM_VAR_CURRENT_OVER_65A;
    } else {
      emValue.measured_values &= (~EM_VAR_CURRENT_OVER_65A);
      emValue.measured_values |= EM_VAR_CURRENT;
    }
  }

  // Prepare extended channel value
  srpc_evtool_v2_emextended2extended(&emValue, extChannel.getExtValue());
  extChannel.setNewValue(emValue);
  runAction(Supla::ON_CHANGE);
}

// energy in 0.00001 kWh
void Supla::Sensor::ElectricityMeter::setFwdActEnergy(
    int phase, unsigned _supla_int64_t energy) {
  if (phase >= 0 && phase < MAX_PHASES) {
    if (emValue.total_forward_active_energy[phase] != energy) {
      valueChanged = true;
    }
    emValue.total_forward_active_energy[phase] = energy;
    emValue.measured_values |= EM_VAR_FORWARD_ACTIVE_ENERGY;
  }
}

// energy in 0.00001 kWh
void Supla::Sensor::ElectricityMeter::setRvrActEnergy(
    int phase, unsigned _supla_int64_t energy) {
  if (phase >= 0 && phase < MAX_PHASES) {
    if (emValue.total_reverse_active_energy[phase] != energy) {
      valueChanged = true;
    }
    emValue.total_reverse_active_energy[phase] = energy;
    emValue.measured_values |= EM_VAR_REVERSE_ACTIVE_ENERGY;
  }
}

// energy in 0.00001 kWh
void Supla::Sensor::ElectricityMeter::setFwdReactEnergy(
    int phase, unsigned _supla_int64_t energy) {
  if (phase >= 0 && phase < MAX_PHASES) {
    if (emValue.total_forward_reactive_energy[phase] != energy) {
      valueChanged = true;
    }
    emValue.total_forward_reactive_energy[phase] = energy;
    emValue.measured_values |= EM_VAR_FORWARD_REACTIVE_ENERGY;
  }
}

// energy in 0.00001 kWh
void Supla::Sensor::ElectricityMeter::setRvrReactEnergy(
    int phase, unsigned _supla_int64_t energy) {
  if (phase >= 0 && phase < MAX_PHASES) {
    if (emValue.total_reverse_reactive_energy[phase] != energy) {
      valueChanged = true;
    }
    emValue.total_reverse_reactive_energy[phase] = energy;
    emValue.measured_values |= EM_VAR_REVERSE_REACTIVE_ENERGY;
  }
}

// voltage in 0.01 V
void Supla::Sensor::ElectricityMeter::setVoltage(
    int phase, unsigned _supla_int16_t voltage) {
  if (phase >= 0 && phase < MAX_PHASES) {
    if (emValue.m[0].voltage[phase] != voltage) {
      valueChanged = true;
    }
    emValue.m[0].voltage[phase] = voltage;
    emValue.measured_values |= EM_VAR_VOLTAGE;
  }
}

// current in 0.001 A
void Supla::Sensor::ElectricityMeter::setCurrent(
    int phase, unsigned _supla_int_t current) {
  if (phase >= 0 && phase < MAX_PHASES) {
    if (rawCurrent[phase] != current) {
      valueChanged = true;
    }
    rawCurrent[phase] = current;
    currentMeasurementAvailable = true;
  }
}

// Frequency in 0.01 Hz
void Supla::Sensor::ElectricityMeter::setFreq(unsigned _supla_int16_t freq) {
  if (emValue.m[0].freq != freq) {
    valueChanged = true;
  }
  emValue.m[0].freq = freq;
  emValue.measured_values |= EM_VAR_FREQ;
}

// power in 0.00001 W
void Supla::Sensor::ElectricityMeter::setPowerActive(int phase,
    _supla_int_t power) {
  if (phase >= 0 && phase < MAX_PHASES) {
    if (emValue.m[0].power_active[phase] != power) {
      valueChanged = true;
    }
    emValue.m[0].power_active[phase] = power;
    emValue.measured_values |= EM_VAR_POWER_ACTIVE;
  }
}

// power in 0.00001 var
void Supla::Sensor::ElectricityMeter::setPowerReactive(int phase,
                                                       _supla_int_t power) {
  if (phase >= 0 && phase < MAX_PHASES) {
    if (emValue.m[0].power_reactive[phase] != power) {
      valueChanged = true;
    }
    emValue.m[0].power_reactive[phase] = power;
    emValue.measured_values |= EM_VAR_POWER_REACTIVE;
  }
}

// power in 0.00001 VA
void Supla::Sensor::ElectricityMeter::setPowerApparent(int phase,
                                                       _supla_int_t power) {
  if (phase >= 0 && phase < MAX_PHASES) {
    if (emValue.m[0].power_apparent[phase] != power) {
      valueChanged = true;
    }
    emValue.m[0].power_apparent[phase] = power;
    emValue.measured_values |= EM_VAR_POWER_APPARENT;
  }
}

// power in 0.001
void Supla::Sensor::ElectricityMeter::setPowerFactor(int phase,
                                                     _supla_int_t powerFactor) {
  if (phase >= 0 && phase < MAX_PHASES) {
    if (emValue.m[0].power_factor[phase] != powerFactor) {
      valueChanged = true;
    }
    emValue.m[0].power_factor[phase] = powerFactor;
    emValue.measured_values |= EM_VAR_POWER_FACTOR;
  }
}

// phase angle in 0.1 degree
void Supla::Sensor::ElectricityMeter::setPhaseAngle(int phase,
                                                    _supla_int_t phaseAngle) {
  if (phase >= 0 && phase < MAX_PHASES) {
    if (emValue.m[0].phase_angle[phase] != phaseAngle) {
      valueChanged = true;
    }
    emValue.m[0].phase_angle[phase] = phaseAngle;
    emValue.measured_values |= EM_VAR_PHASE_ANGLE;
  }
}

void Supla::Sensor::ElectricityMeter::resetReadParameters() {
  if (emValue.measured_values != 0) {
    // we keep only energy counting registers/flags
    emValue.measured_values &= EM_VAR_ALL_ENERGY_REGISTERS;

    memset(&emValue.m[0], 0, sizeof(TElectricityMeter_Measurement));
    memset(&rawCurrent, 0, sizeof(rawCurrent));
    currentMeasurementAvailable = false;

    valueChanged = true;
  }
}

// Please implement this class for reading value from elecricity meter device.
// It will be called every 5 s. Use set methods defined above in order to
// set values on channel. Don't use any other method to modify channel values.
void Supla::Sensor::ElectricityMeter::readValuesFromDevice() {
}

// Put here initialization code for electricity meter device.
// It will be called within SuplaDevce.begin method.
// It should also read first data set, so at the end it should call those two
// methods:
// readValuesFromDevice();
// updateChannelValues();
void Supla::Sensor::ElectricityMeter::onInit() {
}

void Supla::Sensor::ElectricityMeter::iterateAlways() {
  if (millis() - lastReadTime > refreshRateSec * 1000) {
    lastReadTime = millis();
    readValuesFromDevice();
    updateChannelValues();
  }
}

// Implement this method to reset stored energy value (i.e. to set energy
// counter back to 0 kWh
void Supla::Sensor::ElectricityMeter::resetStorage() {
  SUPLA_LOG_DEBUG("EM: reset storage called, but implementation is missing");
}

Supla::Channel *Supla::Sensor::ElectricityMeter::getChannel() {
  return &extChannel;
}

void Supla::Sensor::ElectricityMeter::setRefreshRate(unsigned int sec) {
  refreshRateSec = sec;
  if (refreshRateSec == 0) {
    refreshRateSec = 1;
  }
}

// energy 1 == 0.00001 kWh
unsigned _supla_int64_t
Supla::Sensor::ElectricityMeter::getFwdActEnergy(int phase) {
  if (phase >= 0 && phase < MAX_PHASES) {
    return emValue.total_forward_active_energy[phase];
  }
  return 0;
}

// energy 1 == 0.00001 kWh
unsigned _supla_int64_t
Supla::Sensor::ElectricityMeter::getRvrActEnergy(int phase) {
  if (phase >= 0 && phase < MAX_PHASES) {
    return emValue.total_reverse_active_energy[phase];
  }
  return 0;
}

// energy 1 == 0.00001 kWh
unsigned _supla_int64_t
Supla::Sensor::ElectricityMeter::getFwdReactEnergy(int phase) {
  if (phase >= 0 && phase < MAX_PHASES) {
    return emValue.total_forward_reactive_energy[phase];
  }
  return 0;
}

// energy 1 == 0.00001 kWh
unsigned _supla_int64_t
Supla::Sensor::ElectricityMeter::getRvrReactEnergy(int phase) {
  if (phase >= 0 && phase < MAX_PHASES) {
    return emValue.total_reverse_reactive_energy[phase];
  }
  return 0;
}

// voltage 1 == 0.01 V
unsigned _supla_int16_t Supla::Sensor::ElectricityMeter::getVoltage(int phase) {
  if (phase >= 0 && phase < MAX_PHASES) {
    return emValue.m[0].voltage[phase];
  }
  return 0;
}

// current 1 == 0.001 A
unsigned _supla_int_t Supla::Sensor::ElectricityMeter::getCurrent(int phase) {
  if (phase >= 0 && phase < MAX_PHASES) {
    return rawCurrent[phase];
  }
  return 0;
}

// Frequency 1 == 0.01 Hz
unsigned _supla_int16_t Supla::Sensor::ElectricityMeter::getFreq() {
  return emValue.m[0].freq;
}

// power 1 == 0.00001 W
_supla_int_t Supla::Sensor::ElectricityMeter::getPowerActive(int phase) {
  if (phase >= 0 && phase < MAX_PHASES) {
    return emValue.m[0].power_active[phase];
  }
  return 0;
}

// power 1 == 0.00001 var
_supla_int_t Supla::Sensor::ElectricityMeter::getPowerReactive(int phase) {
  if (phase >= 0 && phase < MAX_PHASES) {
    return emValue.m[0].power_reactive[phase];
  }
  return 0;
}

// power 1 == 0.00001 VA
_supla_int_t Supla::Sensor::ElectricityMeter::getPowerApparent(int phase) {
  if (phase >= 0 && phase < MAX_PHASES) {
    return emValue.m[0].power_apparent[phase];
  }
  return 0;
}

// power 1 == 0.001
_supla_int_t Supla::Sensor::ElectricityMeter::getPowerFactor(int phase) {
  if (phase >= 0 && phase < MAX_PHASES) {
    return emValue.m[0].power_factor[phase];
  }
  return 0;
}

// phase angle 1 == 0.1 degree
_supla_int_t Supla::Sensor::ElectricityMeter::getPhaseAngle(int phase) {
  if (phase >= 0 && phase < MAX_PHASES) {
    return emValue.m[0].phase_angle[phase];
  }
  return 0;
}

int Supla::Sensor::ElectricityMeter::handleCalcfgFromServer(
    TSD_DeviceCalCfgRequest *request) {
  if (request) {
    if (request->Command == SUPLA_CALCFG_CMD_RESET_COUNTERS) {
      if (!request->SuperUserAuthorized) {
        return SUPLA_CALCFG_RESULT_UNAUTHORIZED;
      }
      resetStorage();
      return SUPLA_CALCFG_RESULT_DONE;
    }
  }
  return SUPLA_CALCFG_RESULT_FALSE;
}

void Supla::Sensor::ElectricityMeter::handleAction(int event, int action) {
  (void)(event);
  switch (action) {
    case RESET: {
      resetStorage();
      break;
    }
  }
}

// energy 1 == 0.00001 kWh
unsigned _supla_int64_t Supla::Sensor::ElectricityMeter::getFwdActEnergy(
    const TElectricityMeter_ExtendedValue_V2 &emValue, int phase) {
  if (phase >= 0 && phase < MAX_PHASES) {
    return emValue.total_forward_active_energy[phase];
  }
  return 0;
}

// energy 1 == 0.00001 kWh
unsigned _supla_int64_t Supla::Sensor::ElectricityMeter::getTotalFwdActEnergy(
    const TElectricityMeter_ExtendedValue_V2 &emValue) {
  uint64_t sum = 0;
  for (int i = 0; i < MAX_PHASES; i++) {
    sum += getFwdActEnergy(emValue, i);
  }

  return sum;
}

// energy 1 == 0.00001 kWh
unsigned _supla_int64_t Supla::Sensor::ElectricityMeter::getRvrActEnergy(
    const TElectricityMeter_ExtendedValue_V2 &emValue, int phase) {
  if (phase >= 0 && phase < MAX_PHASES) {
    return emValue.total_reverse_active_energy[phase];
  }
  return 0;
}

// energy 1 == 0.00001 kWh
unsigned _supla_int64_t Supla::Sensor::ElectricityMeter::getTotalRvrActEnergy(
    const TElectricityMeter_ExtendedValue_V2 &emValue) {
  uint64_t sum = 0;
  for (int i = 0; i < MAX_PHASES; i++) {
    sum += getRvrActEnergy(emValue, i);
  }

  return sum;
}

// energy 1 == 0.00001 kWh
unsigned _supla_int64_t Supla::Sensor::ElectricityMeter::getFwdReactEnergy(
    const TElectricityMeter_ExtendedValue_V2 &emValue, int phase) {
  if (phase >= 0 && phase < MAX_PHASES) {
    return emValue.total_forward_reactive_energy[phase];
  }
  return 0;
}

// energy 1 == 0.00001 kWh
unsigned _supla_int64_t Supla::Sensor::ElectricityMeter::getRvrReactEnergy(
    const TElectricityMeter_ExtendedValue_V2 &emValue, int phase) {
  if (phase >= 0 && phase < MAX_PHASES) {
    return emValue.total_reverse_reactive_energy[phase];
  }
  return 0;
}

// voltage 1 == 0.01 V
unsigned _supla_int16_t Supla::Sensor::ElectricityMeter::getVoltage(
    const TElectricityMeter_ExtendedValue_V2 &emValue, int phase) {
  if (phase >= 0 && phase < MAX_PHASES) {
    return emValue.m[0].voltage[phase];
  }
  return 0;
}

// current 1 == 0.001 A
unsigned _supla_int_t Supla::Sensor::ElectricityMeter::getCurrent(
    const TElectricityMeter_ExtendedValue_V2 &emValue, int phase) {
  if (phase >= 0 && phase < MAX_PHASES) {
    if (emValue.measured_values & EM_VAR_CURRENT_OVER_65A) {
      return emValue.m[0].current[phase] * 10;
    } else {
      return emValue.m[0].current[phase];
    }
  }
  return 0;
}

// Frequency 1 == 0.01 Hz
unsigned _supla_int16_t Supla::Sensor::ElectricityMeter::getFreq(
    const TElectricityMeter_ExtendedValue_V2 &emValue) {
  return emValue.m[0].freq;
}

// power 1 == 0.00001 W
_supla_int_t Supla::Sensor::ElectricityMeter::getPowerActive(
    const TElectricityMeter_ExtendedValue_V2 &emValue, int phase) {
  if (phase >= 0 && phase < MAX_PHASES) {
    return emValue.m[0].power_active[phase];
  }
  return 0;
}

// power 1 == 0.00001 var
_supla_int_t Supla::Sensor::ElectricityMeter::getPowerReactive(
    const TElectricityMeter_ExtendedValue_V2 &emValue, int phase) {
  if (phase >= 0 && phase < MAX_PHASES) {
    return emValue.m[0].power_reactive[phase];
  }
  return 0;
}

// power 1 == 0.00001 VA
_supla_int_t Supla::Sensor::ElectricityMeter::getPowerApparent(
    const TElectricityMeter_ExtendedValue_V2 &emValue, int phase) {
  if (phase >= 0 && phase < MAX_PHASES) {
    return emValue.m[0].power_apparent[phase];
  }
  return 0;
}

// power 1 == 0.001
_supla_int_t Supla::Sensor::ElectricityMeter::getPowerFactor(
    const TElectricityMeter_ExtendedValue_V2 &emValue, int phase) {
  if (phase >= 0 && phase < MAX_PHASES) {
    return emValue.m[0].power_factor[phase];
  }
  return 0;
}

// phase angle 1 == 0.1 degree
_supla_int_t Supla::Sensor::ElectricityMeter::getPhaseAngle(
    const TElectricityMeter_ExtendedValue_V2 &emValue, int phase) {
  if (phase >= 0 && phase < MAX_PHASES) {
    return emValue.m[0].phase_angle[phase];
  }
  return 0;
}


bool Supla::Sensor::ElectricityMeter::isFwdActEnergyUsed(
    const TElectricityMeter_ExtendedValue_V2 &emValue) {
  return emValue.measured_values & EM_VAR_FORWARD_ACTIVE_ENERGY;
}

bool Supla::Sensor::ElectricityMeter::isRvrActEnergyUsed(
    const TElectricityMeter_ExtendedValue_V2 &emValue) {
  return emValue.measured_values & EM_VAR_REVERSE_ACTIVE_ENERGY;
}

bool Supla::Sensor::ElectricityMeter::isFwdReactEnergyUsed(
    const TElectricityMeter_ExtendedValue_V2 &emValue) {
  return emValue.measured_values & EM_VAR_FORWARD_REACTIVE_ENERGY;
}

bool Supla::Sensor::ElectricityMeter::isRvrReactEnergyUsed(
    const TElectricityMeter_ExtendedValue_V2 &emValue) {
  return emValue.measured_values & EM_VAR_REVERSE_REACTIVE_ENERGY;
}

bool Supla::Sensor::ElectricityMeter::isVoltageUsed(
    const TElectricityMeter_ExtendedValue_V2 &emValue) {
  return emValue.measured_values & EM_VAR_VOLTAGE;
}

bool Supla::Sensor::ElectricityMeter::isCurrentUsed(
    const TElectricityMeter_ExtendedValue_V2 &emValue) {
  return emValue.measured_values & EM_VAR_CURRENT ||
    emValue.measured_values & EM_VAR_CURRENT_OVER_65A;
}

bool Supla::Sensor::ElectricityMeter::isFreqUsed(
    const TElectricityMeter_ExtendedValue_V2 &emValue) {
  return emValue.measured_values & EM_VAR_FREQ;
}

bool Supla::Sensor::ElectricityMeter::isPowerActiveUsed(
    const TElectricityMeter_ExtendedValue_V2 &emValue) {
  return emValue.measured_values & EM_VAR_POWER_ACTIVE;
}

bool Supla::Sensor::ElectricityMeter::isPowerReactiveUsed(
    const TElectricityMeter_ExtendedValue_V2 &emValue) {
  return emValue.measured_values & EM_VAR_POWER_REACTIVE;
}

bool Supla::Sensor::ElectricityMeter::isPowerApparentUsed(
    const TElectricityMeter_ExtendedValue_V2 &emValue) {
  return emValue.measured_values & EM_VAR_POWER_APPARENT;
}

bool Supla::Sensor::ElectricityMeter::isPowerFactorUsed(
    const TElectricityMeter_ExtendedValue_V2 &emValue) {
  return emValue.measured_values & EM_VAR_POWER_FACTOR;
}

bool Supla::Sensor::ElectricityMeter::isPhaseAngleUsed(
    const TElectricityMeter_ExtendedValue_V2 &emValue) {
  return emValue.measured_values & EM_VAR_PHASE_ANGLE;
}


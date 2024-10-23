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
#include <supla/tools.h>
#include <supla/storage/config.h>
#include <supla/storage/config_tags.h>

#include "../condition.h"
#include "../events.h"
#include "electricity_meter.h"

Supla::Sensor::ElectricityMeter::ElectricityMeter() {
  extChannel.setType(SUPLA_CHANNELTYPE_ELECTRICITY_METER);
  extChannel.setDefault(SUPLA_CHANNELFNC_ELECTRICITY_METER);
  extChannel.setFlag(SUPLA_CHANNEL_FLAG_CALCFG_RESET_COUNTERS);

  emValue.period = 5;
}

void Supla::Sensor::ElectricityMeter::updateChannelValues() {
  if (!valueChanged) {
    return;
  }
  valueChanged = false;

  // Update current messurement precision based on last updates
  bool over65A = false;
  bool activePowerInKW = false;
  bool reactivePowerInKvar = false;
  bool apparentPowerInKVA = false;
  for (int i = 0; i < MAX_PHASES; i++) {
    if (rawCurrent[i] > UINT16_MAX - 1) {
      over65A = true;
    }
    if (rawActivePower[i] > INT32_MAX ||
        rawActivePower[i] < INT32_MIN) {
      activePowerInKW = true;
    }
    if (rawReactivePower[i] > INT32_MAX ||
        rawReactivePower[i] < INT32_MIN) {
      reactivePowerInKvar = true;
    }
    if (rawApparentPower[i] > INT32_MAX ||
        rawApparentPower[i] < INT32_MIN) {
      apparentPowerInKVA = true;
    }
  }

  for (int i = 0; i < MAX_PHASES; i++) {
    if (over65A) {
      emValue.m[0].current[i] = rawCurrent[i] / 10;
    } else {
      emValue.m[0].current[i] = rawCurrent[i];
    }
    if (activePowerInKW) {
      emValue.m[0].power_active[i] = rawActivePower[i] / 1000;
    } else {
      emValue.m[0].power_active[i] = rawActivePower[i];
    }
    if (reactivePowerInKvar) {
      emValue.m[0].power_reactive[i] = rawReactivePower[i] / 1000;
    } else {
      emValue.m[0].power_reactive[i] = rawReactivePower[i];
    }
    if (apparentPowerInKVA) {
      emValue.m[0].power_apparent[i] = rawApparentPower[i] / 1000;
    } else {
      emValue.m[0].power_apparent[i] = rawApparentPower[i];
    }
  }

  if (currentMeasurementAvailable) {
    if (over65A) {
      emValue.measured_values &= (~EM_VAR_CURRENT);
      emValue.measured_values |= EM_VAR_CURRENT_OVER_65A;
    } else {
      emValue.measured_values &= (~EM_VAR_CURRENT_OVER_65A);
      emValue.measured_values |= EM_VAR_CURRENT;
    }
  }
  if (powerActiveMeasurementAvailable) {
    if (activePowerInKW) {
      emValue.measured_values &= (~EM_VAR_POWER_ACTIVE);
      emValue.measured_values |= EM_VAR_POWER_ACTIVE_KW;
    } else {
      emValue.measured_values &= (~EM_VAR_POWER_ACTIVE_KW);
      emValue.measured_values |= EM_VAR_POWER_ACTIVE;
    }
  }
  if (powerReactiveMeasurementAvailable) {
    if (reactivePowerInKvar) {
      emValue.measured_values &= (~EM_VAR_POWER_REACTIVE);
      emValue.measured_values |= EM_VAR_POWER_REACTIVE_KVAR;
    } else {
      emValue.measured_values &= (~EM_VAR_POWER_REACTIVE_KVAR);
      emValue.measured_values |= EM_VAR_POWER_REACTIVE;
    }
  }
  if (powerApparentMeasurementAvailable) {
    if (apparentPowerInKVA) {
      emValue.measured_values &= (~EM_VAR_POWER_APPARENT);
      emValue.measured_values |= EM_VAR_POWER_APPARENT_KVA;
    } else {
      emValue.measured_values &= (~EM_VAR_POWER_APPARENT_KVA);
      emValue.measured_values |= EM_VAR_POWER_APPARENT;
    }
  }

  // Prepare extended channel value
  if (lastChannelUpdateTime == 0 ||
      millis() - lastChannelUpdateTime >= refreshRateSec * 1000) {
    lastChannelUpdateTime = millis();
    srpc_evtool_v2_emextended2extended(&emValue, extChannel.getExtValue());
    extChannel.setNewValue(emValue);
  }
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

// energy in 0.00001 kWh
void Supla::Sensor::ElectricityMeter::setFwdBalancedEnergy(uint64_t energy) {
  if (emValue.total_forward_active_energy_balanced != energy) {
    valueChanged = true;
  }
  emValue.total_forward_active_energy_balanced = energy;
  emValue.measured_values |= EM_VAR_FORWARD_ACTIVE_ENERGY_BALANCED;
}

// energy in 0.00001 kWh
void Supla::Sensor::ElectricityMeter::setRvrBalancedEnergy(uint64_t energy) {
  if (emValue.total_reverse_active_energy_balanced != energy) {
    valueChanged = true;
  }
  emValue.total_reverse_active_energy_balanced = energy;
  emValue.measured_values |= EM_VAR_REVERSE_ACTIVE_ENERGY_BALANCED;
}

// voltage in 0.01 V
void Supla::Sensor::ElectricityMeter::setVoltage(
    int phase, unsigned _supla_int16_t voltage) {
  if (phase >= 0 && phase < MAX_PHASES) {
    if (emValue.m[0].voltage[phase] != voltage) {
      valueChanged = true;
    }
    emValue.m[0].voltage[phase] = voltage;
    emValue.m_count = 1;
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
    emValue.m_count = 1;
    currentMeasurementAvailable = true;
  }
}

// Frequency in 0.01 Hz
void Supla::Sensor::ElectricityMeter::setFreq(unsigned _supla_int16_t freq) {
  if (emValue.m[0].freq != freq) {
    valueChanged = true;
  }
  emValue.m[0].freq = freq;
  emValue.m_count = 1;
  emValue.measured_values |= EM_VAR_FREQ;
}

// power in 0.00001 W
void Supla::Sensor::ElectricityMeter::setPowerActive(int phase, int64_t power) {
  if (phase >= 0 && phase < MAX_PHASES) {
    if (rawActivePower[phase] != power) {
      valueChanged = true;
      rawActivePower[phase] = power;
    }
    emValue.m_count = 1;
    powerActiveMeasurementAvailable = true;
  }
}

// power in 0.00001 var
void Supla::Sensor::ElectricityMeter::setPowerReactive(int phase,
                                                       int64_t power) {
  if (phase >= 0 && phase < MAX_PHASES) {
    if (rawReactivePower[phase] != power) {
      valueChanged = true;
      rawReactivePower[phase] = power;
    }
    emValue.m_count = 1;
    powerReactiveMeasurementAvailable = true;
  }
}

// power in 0.00001 VA
void Supla::Sensor::ElectricityMeter::setPowerApparent(int phase,
                                                       int64_t power) {
  if (phase >= 0 && phase < MAX_PHASES) {
    if (rawApparentPower[phase] != power) {
      valueChanged = true;
      rawApparentPower[phase] = power;
    }
    emValue.m_count = 1;
    powerApparentMeasurementAvailable = true;
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
    emValue.m_count = 1;
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
    emValue.m_count = 1;
    emValue.measured_values |= EM_VAR_PHASE_ANGLE;
  }
}

void Supla::Sensor::ElectricityMeter::resetReadParameters() {
  emValue.m_count = 0;
  if (emValue.measured_values != 0) {
    memset(&emValue.m[0], 0, sizeof(TElectricityMeter_Measurement));
    memset(&rawCurrent, 0, sizeof(rawCurrent));
    memset(&rawActivePower, 0, sizeof(rawActivePower));
    memset(&rawReactivePower, 0, sizeof(rawReactivePower));
    memset(&rawApparentPower, 0, sizeof(rawApparentPower));
    currentMeasurementAvailable = false;
    powerActiveMeasurementAvailable = false;
    powerReactiveMeasurementAvailable = false;
    powerApparentMeasurementAvailable = false;

    valueChanged = true;
  }
}

// Please implement this class for reading value from elecricity meter device.
// It will be called every 5 s. Use set methods defined above in order to
// set values on channel. Don't use any other method to modify channel values.
void Supla::Sensor::ElectricityMeter::readValuesFromDevice() {
}

void Supla::Sensor::ElectricityMeter::onLoadConfig(SuplaDeviceClass *) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg && (availablePhaseLedTypes || availableCtTypes)) {
    // load config changed offline flags
    loadConfigChangeFlag();

    int32_t value32 = 0;
    char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    generateKey(key, Supla::ConfigTag::EmCtTypeTag);
    if (cfg->getInt32(key, &value32)) {
      if (value32 > 63) {
        value32 = 63;
      }
      usedCtType = value32;
    }
    SUPLA_LOG_INFO("EM: CT type is %d", usedCtType);

    int8_t value = 0;
    generateKey(key, Supla::ConfigTag::EmPhaseLedTag);
    if (cfg->getInt8(key, &value)) {
      if (value > 63) {
        value = 63;
      }
      usedPhaseLedType = value;
    }
    SUPLA_LOG_INFO("EM: Phase LED type is %d", usedPhaseLedType);

    value32 = 0;
    generateKey(key, Supla::ConfigTag::EmPhaseLedVoltageLowTag);
    if (cfg->getInt32(key, &value32)) {
      ledVoltageLow = value32;
    }

    generateKey(key, Supla::ConfigTag::EmPhaseLedVoltageHighTag);
    if (cfg->getInt32(key, &value32)) {
      ledVoltageHigh = value32;
    }

    generateKey(key, Supla::ConfigTag::EmPhaseLedPowerLowTag);
    if (cfg->getInt32(key, &value32)) {
      ledPowerLow = value32;
    }

    generateKey(key, Supla::ConfigTag::EmPhaseLedPowerHighTag);
    if (cfg->getInt32(key, &value32)) {
      ledPowerHigh = value32;
    }

    bool configChange = false;

    const int32_t MinPowerValue = -2000000000;
    const int32_t MaxPowerValue = 2000000000;
    const int32_t MinVoltageValue = 15000;
    const int32_t MaxVoltageValue = 100000;
    const int32_t MinValuesDistance = 1000;

    generateKey(key, Supla::ConfigTag::EmPhaseLedPowerLowTag);
    cfg->getInt32(key, &ledPowerLow);
    if (ledPowerLow < MinPowerValue) {
      ledPowerLow = MinPowerValue;
      cfg->setInt32(key, ledPowerLow);
      configChange = true;
    }
    if (ledPowerLow > MaxPowerValue - MinValuesDistance) {
      ledPowerLow = MaxPowerValue - MinValuesDistance;
      cfg->setInt32(key, ledPowerLow);
      configChange = true;
    }

    generateKey(key, Supla::ConfigTag::EmPhaseLedPowerHighTag);
    cfg->getInt32(key, &ledPowerHigh);
    if (ledPowerHigh > MaxPowerValue) {
      ledPowerHigh = MaxPowerValue;
      cfg->setInt32(key, ledPowerHigh);
      configChange = true;
    }
    if (ledPowerHigh < ledPowerLow) {
      ledPowerHigh = ledPowerLow + MinValuesDistance;
      cfg->setInt32(key, ledPowerHigh);
      configChange = true;
    }

    generateKey(key, Supla::ConfigTag::EmPhaseLedVoltageLowTag);
    cfg->getInt32(key, &ledVoltageLow);
    if (ledVoltageLow < MinVoltageValue) {
      ledVoltageLow = MinVoltageValue;
      cfg->setInt32(key, ledVoltageLow);
      configChange = true;
    }
    if (ledVoltageLow > MaxVoltageValue - MinValuesDistance) {
      ledVoltageLow = MaxVoltageValue - MinValuesDistance;
      cfg->setInt32(key, ledVoltageLow);
      configChange = true;
    }

    generateKey(key, Supla::ConfigTag::EmPhaseLedVoltageHighTag);
    cfg->getInt32(key, &ledVoltageHigh);
    if (ledVoltageHigh > MaxVoltageValue) {
      ledVoltageHigh = MaxVoltageValue;
      cfg->setInt32(key, ledVoltageHigh);
      configChange = true;
    }
    if (ledVoltageHigh < ledVoltageLow) {
      ledVoltageHigh = ledVoltageLow + MinValuesDistance;
      cfg->setInt32(key, ledVoltageHigh);
      configChange = true;
    }

    if (configChange) {
      channelConfigState = Supla::ChannelConfigState::LocalChangePending;
      saveConfigChangeFlag();
    }

    SUPLA_LOG_INFO(
        "EM: Voltage low: %d, Voltage high: %d, Power low: %d, Power high: %d",
        ledVoltageLow,
        ledVoltageHigh,
        ledPowerLow,
        ledPowerHigh);
  }
}

uint8_t Supla::Sensor::ElectricityMeter::applyChannelConfig(
    TSD_ChannelConfig *config, bool) {
  if (config == nullptr) {
    return SUPLA_CONFIG_RESULT_DATA_ERROR;
  }
  if (config->ConfigType != SUPLA_CONFIG_TYPE_DEFAULT) {
    return SUPLA_CONFIG_RESULT_DATA_ERROR;
  }
  auto cfg = Supla::Storage::ConfigInstance();
  if (!cfg) {
    return SUPLA_CONFIG_RESULT_TRUE;
  }

  if (config->ConfigSize == 0) {
    return SUPLA_CONFIG_RESULT_TRUE;
  }

  if (config->Func == SUPLA_CHANNELFNC_ELECTRICITY_METER) {
    if (config->ConfigSize < sizeof(TChannelConfig_ElectricityMeter)) {
      return SUPLA_CONFIG_RESULT_DATA_ERROR;
    }
    auto configFromServer =
        reinterpret_cast<TChannelConfig_ElectricityMeter *>(config->Config);

    bool configChanged = false;
    bool configValid = true;

    int8_t bitNumberCtTypeInNewConfig =
        Supla::getBitNumber(configFromServer->UsedCTType);
    if (usedCtType != bitNumberCtTypeInNewConfig) {
      if (!isCtTypeSupported(configFromServer->UsedCTType)) {
        SUPLA_LOG_DEBUG("CT type %d not supported",
                        configFromServer->UsedCTType);
        configValid = false;
      } else {
        usedCtType = bitNumberCtTypeInNewConfig;
        char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
        generateKey(key, Supla::ConfigTag::EmCtTypeTag);
        cfg->setInt32(key, usedCtType);
        configChanged = true;
      }
    }

    int8_t bitNumberPhaseLedTypeInNewConfig =
        Supla::getBitNumber(configFromServer->UsedPhaseLedType);
    if (usedPhaseLedType != bitNumberPhaseLedTypeInNewConfig) {
      if (!isPhaseLedTypeSupported(configFromServer->UsedPhaseLedType)) {
        SUPLA_LOG_DEBUG("Phase LED type %d not supported",
                        configFromServer->UsedPhaseLedType);
        configValid = false;
      } else {
        usedPhaseLedType = bitNumberPhaseLedTypeInNewConfig;
        char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
        generateKey(key, Supla::ConfigTag::EmPhaseLedTag);
        cfg->setInt8(key, usedPhaseLedType);
        configChanged = true;
      }
    }

    if (usedPhaseLedType == 3) {
      if (ledVoltageLow != configFromServer->PhaseLedParam1) {
        ledVoltageLow = configFromServer->PhaseLedParam1;
        char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
        generateKey(key, Supla::ConfigTag::EmPhaseLedVoltageLowTag);
        cfg->setInt32(key, ledVoltageLow);
        configChanged = true;
      }
      if (ledVoltageHigh != configFromServer->PhaseLedParam2) {
        ledVoltageHigh = configFromServer->PhaseLedParam2;
        char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
        generateKey(key, Supla::ConfigTag::EmPhaseLedVoltageHighTag);
        cfg->setInt32(key, ledVoltageHigh);
        configChanged = true;
      }
    }

    if (usedPhaseLedType == 4) {
      if (ledPowerLow != configFromServer->PhaseLedParam1) {
        ledPowerLow = configFromServer->PhaseLedParam1;
        char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
        generateKey(key, Supla::ConfigTag::EmPhaseLedPowerLowTag);
        cfg->setInt32(key, ledPowerLow);
        configChanged = true;
      }
      if (ledPowerHigh != configFromServer->PhaseLedParam2) {
        ledPowerHigh = configFromServer->PhaseLedParam2;
        char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
        generateKey(key, Supla::ConfigTag::EmPhaseLedPowerHighTag);
        cfg->setInt32(key, ledPowerHigh);
        configChanged = true;
      }
    }

    if (configChanged) {
      cfg->commit();
    }

    if (configFromServer->AvailablePhaseLedTypes != availablePhaseLedTypes ||
        configFromServer->AvailableCTTypes != availableCtTypes ||
        !configValid) {
      SUPLA_LOG_WARNING(
          "Invalid config received from server %llu %llu %llu %llu, "
          "configValid %d",
          configFromServer->AvailablePhaseLedTypes,
          configFromServer->AvailableCTTypes,
          availablePhaseLedTypes,
          availableCtTypes,
          configValid);
      channelConfigState = Supla::ChannelConfigState::LocalChangePending;
      saveConfigChangeFlag();
    }
    return SUPLA_CONFIG_RESULT_TRUE;
  }
  return SUPLA_CONFIG_RESULT_TRUE;
}

void Supla::Sensor::ElectricityMeter::fillChannelConfig(void *channelConfig,
                                                        int *size) {
  if (size) {
    *size = 0;
  } else {
    return;
  }
  if (channelConfig == nullptr) {
    return;
  }

  auto config = reinterpret_cast<TChannelConfig_ElectricityMeter *>(
      channelConfig);
  *size = sizeof(TChannelConfig_ElectricityMeter);

  config->AvailableCTTypes = availableCtTypes;
  if (usedCtType >= 0 && usedCtType <= 63) {
    config->UsedCTType = (1ULL << usedCtType);
  }

  config->AvailablePhaseLedTypes = availablePhaseLedTypes;
  if (usedPhaseLedType >= 0 && usedPhaseLedType <= 63) {
    config->UsedPhaseLedType = (1ULL << usedPhaseLedType);
  }

  if (usedPhaseLedType == 3) {
    config->PhaseLedParam1 = ledVoltageLow;
    config->PhaseLedParam2 = ledVoltageHigh;
  } else if (usedPhaseLedType == 4) {
    config->PhaseLedParam1 = ledPowerLow;
    config->PhaseLedParam2 = ledPowerHigh;
  }
  SUPLA_LOG_DEBUG(
      "EM: fillChannelConfig: usedCtType=%d, usedPhaseLedType=%d"
      ", availableCtTypes=%d, availablePhaseLedTypes=%d"
      ", param1=%d, param2=%d",
      static_cast<uint32_t>(config->UsedCTType),
      static_cast<uint32_t>(config->UsedPhaseLedType),
      static_cast<uint32_t>(config->AvailableCTTypes),
      static_cast<uint32_t>(config->AvailablePhaseLedTypes),
      config->PhaseLedParam1,
      config->PhaseLedParam2);
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

const Supla::Channel *Supla::Sensor::ElectricityMeter::getChannel() const {
  return &extChannel;
}

void Supla::Sensor::ElectricityMeter::setRefreshRate(unsigned int sec) {
  SUPLA_LOG_INFO("EM: setRefreshRate: %d", sec);
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
uint64_t Supla::Sensor::ElectricityMeter::getFwdBalancedActEnergy() {
  return emValue.total_forward_active_energy_balanced;
}

// energy 1 == 0.00001 kWh
uint64_t Supla::Sensor::ElectricityMeter::getRvrBalancedActEnergy() {
  return emValue.total_reverse_active_energy_balanced;
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
int64_t Supla::Sensor::ElectricityMeter::getPowerActive(int phase) {
  if (phase >= 0 && phase < MAX_PHASES) {
    return rawActivePower[phase];
  }
  return 0;
}

// power 1 == 0.00001 var
int64_t Supla::Sensor::ElectricityMeter::getPowerReactive(int phase) {
  if (phase >= 0 && phase < MAX_PHASES) {
    return rawReactivePower[phase];
  }
  return 0;
}

// power 1 == 0.00001 VA
int64_t Supla::Sensor::ElectricityMeter::getPowerApparent(int phase) {
  if (phase >= 0 && phase < MAX_PHASES) {
    return rawApparentPower[phase];
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
  return SUPLA_CALCFG_RESULT_NOT_SUPPORTED;
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
uint64_t Supla::Sensor::ElectricityMeter::getFwdBalancedActEnergy(
    const TElectricityMeter_ExtendedValue_V2 &emValue) {
  return emValue.total_forward_active_energy_balanced;
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
uint64_t Supla::Sensor::ElectricityMeter::getRvrBalancedActEnergy(
    const TElectricityMeter_ExtendedValue_V2 &emValue) {
  return emValue.total_reverse_active_energy_balanced;
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
int64_t Supla::Sensor::ElectricityMeter::getPowerActive(
    const TElectricityMeter_ExtendedValue_V2 &emValue, int phase) {
  if (phase >= 0 && phase < MAX_PHASES) {
    if (emValue.measured_values & EM_VAR_POWER_ACTIVE_KW) {
      return emValue.m[0].power_active[phase] * 1000;
    } else {
      return emValue.m[0].power_active[phase];
    }
  }
  return 0;
}

// power 1 == 0.00001 var
int64_t Supla::Sensor::ElectricityMeter::getPowerReactive(
    const TElectricityMeter_ExtendedValue_V2 &emValue, int phase) {
  if (phase >= 0 && phase < MAX_PHASES) {
    if (emValue.measured_values & EM_VAR_POWER_REACTIVE_KVAR) {
      return emValue.m[0].power_reactive[phase] * 1000;
    }  else {
      return emValue.m[0].power_reactive[phase];
    }
  }
  return 0;
}

// power 1 == 0.00001 VA
int64_t Supla::Sensor::ElectricityMeter::getPowerApparent(
    const TElectricityMeter_ExtendedValue_V2 &emValue, int phase) {
  if (phase >= 0 && phase < MAX_PHASES) {
    if (emValue.measured_values & EM_VAR_POWER_APPARENT_KVA) {
      return emValue.m[0].power_apparent[phase] * 1000;
    } else {
      return emValue.m[0].power_apparent[phase];
    }
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

bool Supla::Sensor::ElectricityMeter::isFwdBalancedActEnergyUsed(
    const TElectricityMeter_ExtendedValue_V2 &emValue) {
  return emValue.measured_values & EM_VAR_FORWARD_ACTIVE_ENERGY_BALANCED;
}
bool Supla::Sensor::ElectricityMeter::isRvrBalancedActEnergyUsed(
    const TElectricityMeter_ExtendedValue_V2 &emValue) {
  return emValue.measured_values & EM_VAR_REVERSE_ACTIVE_ENERGY_BALANCED;
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


void Supla::Sensor::ElectricityMeter::addPhaseLedType(uint64_t phaseLedType) {
  if (availablePhaseLedTypes == 0 && phaseLedType != 0 &&
      usedPhaseLedType == -1) {
    int bitNumber = Supla::getBitNumber(phaseLedType);
    if (bitNumber >= 0) {
      usedPhaseLedType = bitNumber;
    }
  }
  availablePhaseLedTypes |= phaseLedType;
  enableChannelConfig();
}

void Supla::Sensor::ElectricityMeter::enableChannelConfig() {
  extChannel.setFlag(SUPLA_CHANNEL_FLAG_RUNTIME_CHANNEL_CONFIG_UPDATE);
}

void Supla::Sensor::ElectricityMeter::addCtType(uint64_t ctType) {
  if (availableCtTypes == 0 && ctType != 0 && usedCtType == -1) {
    int bitNumber = Supla::getBitNumber(ctType);
    if (bitNumber >= 0) {
      usedCtType = bitNumber;
    }
  }
  availableCtTypes |= ctType;
  enableChannelConfig();
}

int8_t Supla::Sensor::ElectricityMeter::getPhaseLedType() const {
  return usedPhaseLedType;
}

int32_t Supla::Sensor::ElectricityMeter::getLedVoltageLow() const {
  return ledVoltageLow;
}

int32_t Supla::Sensor::ElectricityMeter::getLedVoltageHigh() const {
  return ledVoltageHigh;
}

int32_t Supla::Sensor::ElectricityMeter::getLedPowerLow() const {
  return ledPowerLow;
}

int32_t Supla::Sensor::ElectricityMeter::getLedPowerHigh() const {
  return ledPowerHigh;
}

bool Supla::Sensor::ElectricityMeter::isCtTypeSupported(uint64_t ctType) const {
  return (availableCtTypes & ctType) != 0;
}

bool Supla::Sensor::ElectricityMeter::isPhaseLedTypeSupported(
    uint64_t ledType) const {
  return (availablePhaseLedTypes & ledType) != 0;
}

void Supla::Sensor::ElectricityMeter::onRegistered(
    Supla::Protocol::SuplaSrpc *suplaSrpc) {
  lastChannelUpdateTime = 0;
  Supla::ElementWithChannelActions::onRegistered(suplaSrpc);
}

void Supla::Sensor::ElectricityMeter::purgeConfig() {
  Supla::ElementWithChannelActions::purgeConfig();
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    generateKey(key, Supla::ConfigTag::EmCtTypeTag);
    cfg->eraseKey(key);
    generateKey(key, Supla::ConfigTag::EmPhaseLedTag);
    cfg->eraseKey(key);
    generateKey(key, Supla::ConfigTag::EmPhaseLedVoltageLowTag);
    cfg->eraseKey(key);
    generateKey(key, Supla::ConfigTag::EmPhaseLedVoltageHighTag);
    cfg->eraseKey(key);
    generateKey(key, Supla::ConfigTag::EmPhaseLedPowerLowTag);
    cfg->eraseKey(key);
    generateKey(key, Supla::ConfigTag::EmPhaseLedPowerHighTag);
    cfg->eraseKey(key);
  }
}

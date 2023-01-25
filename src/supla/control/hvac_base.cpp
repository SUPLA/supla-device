/*
 * Copyright (C) AC SOFTWARE SP. Z O.O
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "hvac_base.h"

#include <string.h>

#include <supla/storage/storage.h>
#include <supla/storage/config.h>
#include <supla/log_wrapper.h>

using Supla::Control::HvacBase;

HvacBase::HvacBase() {
  channel.setType(SUPLA_CHANNELTYPE_HVAC);
  channel.setFlag(SUPLA_CHANNEL_FLAG_WEEKLY_SCHEDULE);
  channel.setFlag(SUPLA_CHANNEL_FLAG_RUNTIME_CHANNEL_CONFIG_UPDATE);

  // default modes of HVAC channel
  channel.setFuncList(SUPLA_HVAC_CAP_FLAG_MODE_HEAT |
                      SUPLA_HVAC_CAP_FLAG_MODE_ONOFF);
  // default function is set in onInit based on supported modes or loaded from
  // config
}

HvacBase::~HvacBase() {
}

bool HvacBase::iterateConnected() {
  auto result = Element::iterateConnected();
  return result;
}

void HvacBase::onLoadConfig() {
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};

    // Read last set channel function
    loadFunctionFromConfig();

    // Generic HVAC configuration
    generateKey(key, "hvac_cfg");
    if (cfg->getBlob(key,
                     reinterpret_cast<char *>(&config),
                     sizeof(TSD_ChannelConfig_HVAC))) {
      SUPLA_LOG_INFO("HVAC config loaded successfully");
    } else {
      SUPLA_LOG_INFO("HVAC config missing. Using SW defaults");
    }

    // Weekly schedule configuration
    generateKey(key, "hvac_weekly");
    if (cfg->getBlob(key,
                     reinterpret_cast<char *>(&weeklySchedule),
                     sizeof(TSD_ChannelConfig_WeeklySchedule))) {
      SUPLA_LOG_INFO("HVAC weekly schedule loaded successfully");
      isWeeklyScheduleConfigured = true;
    } else {
      SUPLA_LOG_INFO("HVAC weekly schedule missing. Using SW defaults");
      isWeeklyScheduleConfigured = false;
    }
  } else {
    SUPLA_LOG_ERROR("HVAC can't work without config storage");
  }
}

void HvacBase::onLoadState() {
  if (getChannel() && getChannelNumber() >= 0) {
    auto hvacValue = getChannel()->getValueHvac();
    Supla::Storage::ReadState(reinterpret_cast<unsigned char *>(hvacValue),
                            sizeof(THVACValue));
  }
}

void HvacBase::onSaveState() {
  if (getChannel() && getChannelNumber() >= 0) {
    auto hvacValue = getChannel()->getValueHvac();
    Supla::Storage::WriteState(
        reinterpret_cast<const unsigned char *>(hvacValue), sizeof(THVACValue));
  }
}

void HvacBase::onInit() {
  // init default channel function when it wasn't read from config or
  // set by user
  if (getChannel()->getDefaultFunction() == 0) {
    // set default to auto when both heat and cool are supported
    if (isHeatingSupported() && isCoolingSupported() && isAutoSupported()) {
      setAndSaveFunction(SUPLA_CHANNELFNC_HVAC_THERMOSTAT_AUTO);
    } else if (isHeatingSupported()) {
      setAndSaveFunction(SUPLA_CHANNELFNC_HVAC_THERMOSTAT_HEAT);
    } else if (isCoolingSupported()) {
      setAndSaveFunction(SUPLA_CHANNELFNC_HVAC_THERMOSTAT_COOL);
    } else if (isDrySupported()) {
      setAndSaveFunction(SUPLA_CHANNELFNC_HVAC_DRYER);
    } else if (isFanSupported()) {
      setAndSaveFunction(SUPLA_CHANNELFNC_HVAC_FAN);
    }
  }
}


void HvacBase::onRegistered(Supla::Protocol::SuplaSrpc *suplaSrpc) {
  Supla::Element::onRegistered(suplaSrpc);
}

void HvacBase::iterateAlways() {
}

void HvacBase::setHeatingSupported(bool supported) {
  if (supported) {
    channel.setFuncList(channel.getFuncList() | SUPLA_HVAC_CAP_FLAG_MODE_HEAT);
  } else {
    channel.setFuncList(channel.getFuncList() & ~SUPLA_HVAC_CAP_FLAG_MODE_HEAT);
  }
}

void HvacBase::setCoolingSupported(bool supported) {
  if (supported) {
    channel.setFuncList(channel.getFuncList() | SUPLA_HVAC_CAP_FLAG_MODE_COOL);
  } else {
    channel.setFuncList(channel.getFuncList() & ~SUPLA_HVAC_CAP_FLAG_MODE_COOL);
  }
}

void HvacBase::setAutoSupported(bool supported) {
  if (supported) {
    channel.setFuncList(channel.getFuncList() | SUPLA_HVAC_CAP_FLAG_MODE_AUTO);
    setHeatingSupported(true);
    setCoolingSupported(true);
  } else {
    channel.setFuncList(channel.getFuncList() & ~SUPLA_HVAC_CAP_FLAG_MODE_AUTO);
  }
}

void HvacBase::setFanSupported(bool supported) {
  if (supported) {
    channel.setFuncList(channel.getFuncList() | SUPLA_HVAC_CAP_FLAG_MODE_FAN);
  } else {
    channel.setFuncList(channel.getFuncList() & ~SUPLA_HVAC_CAP_FLAG_MODE_FAN);
  }
}

void HvacBase::setDrySupported(bool supported) {
  if (supported) {
    channel.setFuncList(channel.getFuncList() | SUPLA_HVAC_CAP_FLAG_MODE_DRY);
  } else {
    channel.setFuncList(channel.getFuncList() & ~SUPLA_HVAC_CAP_FLAG_MODE_DRY);
  }
}

void HvacBase::setOnOffSupported(bool supported) {
  if (supported) {
    channel.setFuncList(channel.getFuncList() | SUPLA_HVAC_CAP_FLAG_MODE_ONOFF);
  } else {
    channel.setFuncList(channel.getFuncList() &
                        ~SUPLA_HVAC_CAP_FLAG_MODE_ONOFF);
  }
}

bool HvacBase::isOnOffSupported() {
  return channel.getFuncList() & SUPLA_HVAC_CAP_FLAG_MODE_ONOFF;
}

bool HvacBase::isHeatingSupported() {
  return channel.getFuncList() & SUPLA_HVAC_CAP_FLAG_MODE_HEAT;
}

bool HvacBase::isCoolingSupported() {
  return channel.getFuncList() & SUPLA_HVAC_CAP_FLAG_MODE_COOL;
}

bool HvacBase::isAutoSupported() {
  return channel.getFuncList() & SUPLA_HVAC_CAP_FLAG_MODE_AUTO;
}

bool HvacBase::isFanSupported() {
  return channel.getFuncList() & SUPLA_HVAC_CAP_FLAG_MODE_FAN;
}

bool HvacBase::isDrySupported() {
  return channel.getFuncList() & SUPLA_HVAC_CAP_FLAG_MODE_DRY;
}

uint8_t HvacBase::handleChannelConfig(TSD_ChannelConfig *newConfig) {
  if (newConfig == nullptr) {
    return SUPLA_CONFIG_RESULT_DATA_ERROR;
  }

  if (newConfig->ConfigType != SUPLA_CONFIG_TYPE_DEFAULT) {
    return SUPLA_CONFIG_RESULT_TYPE_NOT_SUPPORTED;
  }

  auto channelFunction = newConfig->Func;
  if (!isFunctionSupported(channelFunction)) {
    return SUPLA_CONFIG_RESULT_FUNCTION_NOT_SUPPORTED;
  }

  if (newConfig->ConfigSize < sizeof(TSD_ChannelConfig_HVAC)) {
    return SUPLA_CONFIG_RESULT_DATA_ERROR;
  }

  auto hvacConfig =
      reinterpret_cast<TSD_ChannelConfig_HVAC *>(newConfig->Config);

  if (!isConfigValid(hvacConfig)) {
    return SUPLA_CONFIG_RESULT_DATA_ERROR;
  }

  TSD_ChannelConfig_HVAC configCopy;
  memcpy(&configCopy, &config, sizeof(TSD_ChannelConfig_HVAC));

  // Received config looks ok, so we apply it to channel
  setAndSaveFunction(channelFunction);

  // We don't use setters here, because they run validation againsted current
  // configuration, which may fail. However new config was already validated
  // so we assign them directly.
  config.MainThermometerChannelNo = hvacConfig->MainThermometerChannelNo;
  config.HeaterCoolerThermometerChannelNo =
      hvacConfig->HeaterCoolerThermometerChannelNo;
  config.HeaterCoolerThermometerType = hvacConfig->HeaterCoolerThermometerType;
  config.EnableAntiFreezeAndOverheatProtection =
      hvacConfig->EnableAntiFreezeAndOverheatProtection;
  config.UsedAlgorithm = hvacConfig->UsedAlgorithm;
  config.MinOnTimeS = hvacConfig->MinOnTimeS;
  config.MinOffTimeS = hvacConfig->MinOffTimeS;

  if (isTemperatureSetInStruct(&hvacConfig->Temperatures, TEMPERATURE_ECO)) {
    setTemperatureInStruct(&config.Temperatures,
                           TEMPERATURE_ECO,
                           getTemperatureEco(&hvacConfig->Temperatures));
  }

  if (isTemperatureSetInStruct(&hvacConfig->Temperatures,
                               TEMPERATURE_COMFORT)) {
    setTemperatureInStruct(&config.Temperatures,
                           TEMPERATURE_COMFORT,
                           getTemperatureComfort(&hvacConfig->Temperatures));
  }

  if (isTemperatureSetInStruct(&hvacConfig->Temperatures, TEMPERATURE_BOOST)) {
    setTemperatureInStruct(&config.Temperatures,
                           TEMPERATURE_BOOST,
                           getTemperatureBoost(&hvacConfig->Temperatures));
  }

  if (isTemperatureSetInStruct(&hvacConfig->Temperatures,
                               TEMPERATURE_FREEZE_PROTECTION)) {
    setTemperatureInStruct(
        &config.Temperatures,
        TEMPERATURE_FREEZE_PROTECTION,
        getTemperatureFreezeProtection(&hvacConfig->Temperatures));
  }

  if (isTemperatureSetInStruct(&hvacConfig->Temperatures,
                               TEMPERATURE_HEAT_PROTECTION)) {
    setTemperatureInStruct(
        &config.Temperatures,
        TEMPERATURE_HEAT_PROTECTION,
        getTemperatureHeatProtection(&hvacConfig->Temperatures));
  }

  if (isTemperatureSetInStruct(&hvacConfig->Temperatures,
                               TEMPERATURE_HISTERESIS)) {
    setTemperatureInStruct(&config.Temperatures,
                           TEMPERATURE_HISTERESIS,
                           getTemperatureHisteresis(&hvacConfig->Temperatures));
  }

  if (isTemperatureSetInStruct(&hvacConfig->Temperatures,
                               TEMPERATURE_BELOW_ALARM)) {
    setTemperatureInStruct(&config.Temperatures,
                           TEMPERATURE_BELOW_ALARM,
                           getTemperatureBelowAlarm(&hvacConfig->Temperatures));
  }

  if (isTemperatureSetInStruct(&hvacConfig->Temperatures,
                               TEMPERATURE_ABOVE_ALARM)) {
    setTemperatureInStruct(&config.Temperatures,
                           TEMPERATURE_ABOVE_ALARM,
                           getTemperatureAboveAlarm(&hvacConfig->Temperatures));
  }

  if (isTemperatureSetInStruct(&hvacConfig->Temperatures,
                               TEMPERATURE_HEATER_COOLER_MIN_SETPOINT)) {
    setTemperatureInStruct(
        &config.Temperatures,
        TEMPERATURE_HEATER_COOLER_MIN_SETPOINT,
        getTemperatureHeaterCoolerMinSetpoint(&hvacConfig->Temperatures));
  }

  if (isTemperatureSetInStruct(&hvacConfig->Temperatures,
                               TEMPERATURE_HEATER_COOLER_MAX_SETPOINT)) {
    setTemperatureInStruct(
        &config.Temperatures,
        TEMPERATURE_HEATER_COOLER_MAX_SETPOINT,
        getTemperatureHeaterCoolerMaxSetpoint(&hvacConfig->Temperatures));
  }

  if (isTemperatureSetInStruct(&hvacConfig->Temperatures,
                               TEMPERATURE_AUTO_OFFSET)) {
    setTemperatureInStruct(&config.Temperatures,
                           TEMPERATURE_AUTO_OFFSET,
                           getTemperatureAutoOffset(&hvacConfig->Temperatures));
  }

  if (memcmp(&config, &configCopy, sizeof(TSD_ChannelConfig_HVAC)) != 0) {
    saveConfig();
  }

  return SUPLA_CONFIG_RESULT_TRUE;
}

bool HvacBase::isConfigValid(TSD_ChannelConfig_HVAC *newConfig) {
  if (newConfig == nullptr) {
    return false;
  }

  // main thermometer is mandatory and has to be set to a local thermometer
  if (!isChannelThermometer(newConfig->MainThermometerChannelNo)) {
    return false;
  }

  // heater cooler thermometer is optional, but if set, it has to be set to a
  // local thermometer
  if (newConfig->HeaterCoolerThermometerType !=
      SUPLA_HVAC_HEATER_COOLER_THERMOMETER_TYPE_NOT_SET) {
    if (!isChannelThermometer(newConfig->HeaterCoolerThermometerChannelNo)) {
      return false;
    }
    if (newConfig->HeaterCoolerThermometerChannelNo ==
        newConfig->MainThermometerChannelNo) {
      return false;
    }
  }

  switch (newConfig->HeaterCoolerThermometerType) {
    case SUPLA_HVAC_HEATER_COOLER_THERMOMETER_TYPE_NOT_SET:
    case SUPLA_HVAC_HEATER_COOLER_THERMOMETER_TYPE_DISALBED:
    case SUPLA_HVAC_HEATER_COOLER_THERMOMETER_TYPE_FLOOR:
    case SUPLA_HVAC_HEATER_COOLER_THERMOMETER_TYPE_WATER:
    case SUPLA_HVAC_HEATER_COOLER_THERMOMETER_TYPE_GENERIC_COOLER:
    case SUPLA_HVAC_HEATER_COOLER_THERMOMETER_TYPE_GENERIC_HEATER:
      break;
    default:
      return false;
  }

  if (!isAlgorithmValid(newConfig->UsedAlgorithm)) {
    SUPLA_LOG_WARNING("HVAC: invalid algorithm %d", newConfig->UsedAlgorithm);
    return false;
  }

  if (!isMinOnOffTimeValid(newConfig->MinOnTimeS)) {
    SUPLA_LOG_WARNING("HVAC: invalid min on time %d", newConfig->MinOnTimeS);
    return false;
  }

  if (!isMinOnOffTimeValid(newConfig->MinOffTimeS)) {
    SUPLA_LOG_WARNING("HVAC: invalid min off time %d", newConfig->MinOffTimeS);
    return false;
  }

  if (!areTemperaturesValid(&newConfig->Temperatures)) {
    SUPLA_LOG_WARNING("HVAC: invalid temperatures");
    return false;
  }

  return true;
}

bool HvacBase::areTemperaturesValid(THVACTemperatureCfg *temperatures) {
  if (temperatures == nullptr) {
    return false;
  }

  if (isTemperatureSetInStruct(temperatures, TEMPERATURE_FREEZE_PROTECTION)) {
    if (!isTemperatureFreezeProtectionValid(temperatures)) {
      SUPLA_LOG_WARNING("HVAC: invalid freeze protection temperature");
      return false;
    }
  }

  if (isTemperatureSetInStruct(temperatures, TEMPERATURE_HEAT_PROTECTION)) {
    if (!isTemperatureHeatProtectionValid(temperatures)) {
      SUPLA_LOG_WARNING("HVAC: invalid heat protection temperature");
      return false;
    }
  }

  if (isTemperatureSetInStruct(temperatures, TEMPERATURE_ECO)) {
    if (!isTemperatureEcoValid(temperatures)) {
      SUPLA_LOG_WARNING("HVAC: invalid eco temperature");
      return false;
    }
  }

  if (isTemperatureSetInStruct(temperatures, TEMPERATURE_COMFORT)) {
    if (!isTemperatureComfortValid(temperatures)) {
      SUPLA_LOG_WARNING("HVAC: invalid comfort temperature");
      return false;
    }
  }

  if (isTemperatureSetInStruct(temperatures, TEMPERATURE_BOOST)) {
    if (!isTemperatureBoostValid(temperatures)) {
      SUPLA_LOG_WARNING("HVAC: invalid boost temperature");
      return false;
    }
  }

  if (isTemperatureSetInStruct(temperatures, TEMPERATURE_HISTERESIS)) {
    if (!isTemperatureHisteresisValid(temperatures)) {
      SUPLA_LOG_WARNING("HVAC: invalid histeresis value");
      return false;
    }
  }

  if (isTemperatureSetInStruct(temperatures, TEMPERATURE_AUTO_OFFSET)) {
    if (!isTemperatureAutoOffsetValid(temperatures)) {
      SUPLA_LOG_WARNING("HVAC: invalid auto offset value");
      return false;
    }
  }

  if (isTemperatureSetInStruct(temperatures, TEMPERATURE_BELOW_ALARM)) {
    if (!isTemperatureBelowAlarmValid(temperatures)) {
      SUPLA_LOG_WARNING("HVAC: invalid below alarm temperature");
      return false;
    }
  }

  if (isTemperatureSetInStruct(temperatures, TEMPERATURE_ABOVE_ALARM)) {
    if (!isTemperatureAboveAlarmValid(temperatures)) {
      SUPLA_LOG_WARNING("HVAC: invalid above alarm temperature");
      return false;
    }
  }

  if (isTemperatureSetInStruct(temperatures,
                               TEMPERATURE_HEATER_COOLER_MIN_SETPOINT)) {
    if (!isTemperatureHeaterCoolerMinSetpointValid(temperatures)) {
      SUPLA_LOG_WARNING("HVAC: invalid heater cooler min setpoint");
      return false;
    }
  }

  if (isTemperatureSetInStruct(temperatures,
                               TEMPERATURE_HEATER_COOLER_MAX_SETPOINT)) {
    if (!isTemperatureHeaterCoolerMaxSetpointValid(temperatures)) {
      SUPLA_LOG_WARNING("HVAC: invalid heater cooler max setpoint");
      return false;
    }
  }

  return true;
}

bool HvacBase::isTemperatureSetInStruct(THVACTemperatureCfg *temperatures,
                                        unsigned _supla_int_t index) {
  if (temperatures == nullptr) {
    return false;
  }

  if (index == 0) {
    return false;
  }

  return (temperatures->Index & index) == index;
}

_supla_int16_t HvacBase::getTemperatureFromStruct(
    THVACTemperatureCfg *temperatures, unsigned _supla_int_t index) {
  if (!isTemperatureSetInStruct(temperatures, index)) {
    return SUPLA_TEMPERATURE_INVALID_INT16;
  }

  // convert index bit number to array index
  int arrayIndex = 0;
  for (; arrayIndex < 24; arrayIndex++) {
    if (index & (1 << arrayIndex)) {
      break;
    }
  }

  return temperatures->Temperature[arrayIndex];
}

bool HvacBase::isTemperatureInRoomConstrain(_supla_int16_t temperature) {
  if (temperature == SUPLA_TEMPERATURE_INVALID_INT16) {
    return false;
  }

  auto tMin =
      getTemperatureFromStruct(&config.Temperatures, TEMPERATURE_ROOM_MIN);
  auto tMax =
      getTemperatureFromStruct(&config.Temperatures, TEMPERATURE_ROOM_MAX);

  return temperature >= tMin && temperature <= tMax;
}

bool HvacBase::isTemperatureInHeaterCoolerConstrain(
    _supla_int16_t temperature) {
  if (temperature == SUPLA_TEMPERATURE_INVALID_INT16) {
    return false;
  }

  auto tMin = getTemperatureFromStruct(&config.Temperatures,
                                       TEMPERATURE_HEATER_COOLER_MIN);
  auto tMax = getTemperatureFromStruct(&config.Temperatures,
                                       TEMPERATURE_HEATER_COOLER_MAX);

  return temperature >= tMin && temperature <= tMax;
}

bool HvacBase::isTemperatureFreezeProtectionValid(_supla_int16_t temperature) {
  return isTemperatureInRoomConstrain(temperature);
}

bool HvacBase::isTemperatureFreezeProtectionValid(
    THVACTemperatureCfg *temperatures) {
  auto t =
      getTemperatureFromStruct(temperatures, TEMPERATURE_FREEZE_PROTECTION);
  return isTemperatureFreezeProtectionValid(t);
}

bool HvacBase::isTemperatureHeatProtectionValid(_supla_int16_t temperature) {
  return isTemperatureInRoomConstrain(temperature);
}

bool HvacBase::isTemperatureHeatProtectionValid(
    THVACTemperatureCfg *temperatures) {
  auto  t = getTemperatureFromStruct(temperatures, TEMPERATURE_HEAT_PROTECTION);
  return isTemperatureInRoomConstrain(t);
}

bool HvacBase::isTemperatureEcoValid(_supla_int16_t temperature) {
  return isTemperatureInRoomConstrain(temperature);
}

bool HvacBase::isTemperatureEcoValid(THVACTemperatureCfg *temperatures) {
  auto t = getTemperatureFromStruct(temperatures, TEMPERATURE_ECO);
  return isTemperatureInRoomConstrain(t);
}

bool HvacBase::isTemperatureComfortValid(_supla_int16_t temperature) {
  return isTemperatureInRoomConstrain(temperature);
}

bool HvacBase::isTemperatureComfortValid(THVACTemperatureCfg *temperatures) {
  auto t = getTemperatureFromStruct(temperatures, TEMPERATURE_COMFORT);
  return isTemperatureInRoomConstrain(t);
}

bool HvacBase::isTemperatureBoostValid(_supla_int16_t temperature) {
  return isTemperatureInRoomConstrain(temperature);
}

bool HvacBase::isTemperatureBoostValid(THVACTemperatureCfg *temperatures) {
  auto t = getTemperatureFromStruct(temperatures, TEMPERATURE_BOOST);
  return isTemperatureInRoomConstrain(t);
}

bool HvacBase::isTemperatureHisteresisValid(_supla_int16_t hist) {
  if (hist == SUPLA_TEMPERATURE_INVALID_INT16) {
    return false;
  }

  auto histMin = getTemperatureFromStruct(&config.Temperatures,
                                          TEMPERATURE_HISTERESIS_MIN);
  auto histMax = getTemperatureFromStruct(&config.Temperatures,
                                          TEMPERATURE_HISTERESIS_MAX);

  return hist >= histMin && hist <= histMax;
}

bool HvacBase::isTemperatureHisteresisValid(THVACTemperatureCfg *temperatures) {
  auto hist = getTemperatureFromStruct(temperatures, TEMPERATURE_HISTERESIS);
  return isTemperatureHisteresisValid(hist);
}

bool HvacBase::isTemperatureAutoOffsetValid(_supla_int16_t temperature) {
  if (temperature == SUPLA_TEMPERATURE_INVALID_INT16) {
    return false;
  }

  auto tMin = getTemperatureFromStruct(&config.Temperatures,
                                       TEMPERATURE_AUTO_OFFSET_MIN);
  auto tMax = getTemperatureFromStruct(&config.Temperatures,
                                       TEMPERATURE_AUTO_OFFSET_MAX);

  return temperature >= tMin && temperature <= tMax;
}

bool HvacBase::isTemperatureAutoOffsetValid(THVACTemperatureCfg *temperatures) {
  auto t = getTemperatureFromStruct(temperatures, TEMPERATURE_AUTO_OFFSET);
  return isTemperatureAutoOffsetValid(t);
}

bool HvacBase::isTemperatureHeaterCoolerMinSetpointValid(
    _supla_int16_t temperature) {
  if (temperature == SUPLA_TEMPERATURE_INVALID_INT16) {
    return false;
  }

  auto tMin = getTemperatureHeaterCoolerMin();
  auto tMax = getTemperatureHeaterCoolerMax();

  auto tMaxSetpoint = getTemperatureHeaterCoolerMaxSetpoint();

  return (temperature < tMaxSetpoint ||
          tMaxSetpoint == SUPLA_TEMPERATURE_INVALID_INT16) &&
         temperature >= tMin && temperature <= tMax;
}

bool HvacBase::isTemperatureHeaterCoolerMinSetpointValid(
    THVACTemperatureCfg *temperatures) {
  auto t = getTemperatureHeaterCoolerMinSetpoint(temperatures);
  if (isTemperatureSetInStruct(temperatures,
                               TEMPERATURE_HEATER_COOLER_MAX_SETPOINT)) {
    auto tMaxSetpoint = getTemperatureHeaterCoolerMaxSetpoint(temperatures);
    if (t >= tMaxSetpoint) {
      return false;
    }
  }

  auto tMin = getTemperatureHeaterCoolerMin();
  auto tMax = getTemperatureHeaterCoolerMax();

  return t >= tMin && t <= tMax;
}

bool HvacBase::isTemperatureHeaterCoolerMaxSetpointValid(
    _supla_int16_t temperature) {
  if (temperature == SUPLA_TEMPERATURE_INVALID_INT16) {
    return false;
  }

  // min and max values are taken from device config
  auto tMin = getTemperatureHeaterCoolerMin();
  auto tMax = getTemperatureHeaterCoolerMax();

  // current min setpoint is taken from current device config
  auto tMinSetpoint = getTemperatureHeaterCoolerMinSetpoint();

  return (temperature > tMinSetpoint ||
          tMinSetpoint == SUPLA_TEMPERATURE_INVALID_INT16) &&
         temperature >= tMin && temperature <= tMax;
}

bool HvacBase::isTemperatureHeaterCoolerMaxSetpointValid(
    THVACTemperatureCfg *temperatures) {
  auto t = getTemperatureHeaterCoolerMaxSetpoint(temperatures);

  // min setpoint is taken from configuration in temperatures struct
  // i.e. new values send from server
  if (isTemperatureSetInStruct(temperatures,
                               TEMPERATURE_HEATER_COOLER_MIN_SETPOINT)) {
    auto tMinSetpoint = getTemperatureHeaterCoolerMinSetpoint(temperatures);
    if (t <= tMinSetpoint) {
      return false;
    }
  }

  // min and max range is validated against device config (readonly values)
  auto tMin = getTemperatureHeaterCoolerMin();
  auto tMax = getTemperatureHeaterCoolerMax();

  return t >= tMin && t <= tMax;
}

bool HvacBase::isTemperatureBelowAlarmValid(_supla_int16_t temperature) {
  if (temperature == SUPLA_TEMPERATURE_INVALID_INT16) {
    return false;
  }

  auto tMin =
      getTemperatureFromStruct(&config.Temperatures, TEMPERATURE_ROOM_MIN);
  auto tMax =
      getTemperatureFromStruct(&config.Temperatures, TEMPERATURE_ROOM_MAX);

  return temperature >= tMin && temperature <= tMax;
}

bool HvacBase::isTemperatureBelowAlarmValid(THVACTemperatureCfg *temperatures) {
  auto t = getTemperatureFromStruct(temperatures, TEMPERATURE_BELOW_ALARM);
  return isTemperatureBelowAlarmValid(t);
}

bool HvacBase::isTemperatureAboveAlarmValid(_supla_int16_t temperature) {
  if (temperature == SUPLA_TEMPERATURE_INVALID_INT16) {
    return false;
  }
  auto tMin =
      getTemperatureFromStruct(&config.Temperatures, TEMPERATURE_ROOM_MIN);
  auto tMax =
      getTemperatureFromStruct(&config.Temperatures, TEMPERATURE_ROOM_MAX);

  return temperature >= tMin && temperature <= tMax;
}

bool HvacBase::isTemperatureAboveAlarmValid(THVACTemperatureCfg *temperatures) {
  auto t = getTemperatureFromStruct(temperatures, TEMPERATURE_ABOVE_ALARM);
  return isTemperatureAboveAlarmValid(t);
}

bool HvacBase::isChannelThermometer(uint8_t channelNo) {
  auto element = Supla::Element::getElementByChannelNumber(channelNo);
  if (element == nullptr) {
    SUPLA_LOG_WARNING("HVAC: thermometer not found for channel %d", channelNo);
    return false;
  }
  auto elementType = element->getChannel()->getChannelType();
  switch (elementType) {
    case SUPLA_CHANNELTYPE_HUMIDITYANDTEMPSENSOR:
    case SUPLA_CHANNELTYPE_THERMOMETER:
      break;
    default:
      SUPLA_LOG_WARNING("HVAC: thermometer channel %d has invalid type %d",
          channelNo, elementType);
      return false;
  }
  return true;
}

bool HvacBase::isAlgorithmValid(unsigned _supla_int16_t algorithm) {
  return (config.AlgorithmCaps & algorithm) == algorithm;
}

// handleWeeklySchedule
uint8_t HvacBase::handleWeeklySchedule(TSD_ChannelConfig *config) {
  if (config == nullptr) {
    return SUPLA_CONFIG_RESULT_DATA_ERROR;
  }

  saveWeeklySchedule();

  return SUPLA_RESULT_TRUE;
}

bool HvacBase::isFunctionSupported(_supla_int_t channelFunction) {
  switch (channelFunction) {
    case SUPLA_CHANNELFNC_HVAC_THERMOSTAT_HEAT:
      return isHeatingSupported();
    case SUPLA_CHANNELFNC_HVAC_THERMOSTAT_COOL:
      return isCoolingSupported();
    case SUPLA_CHANNELFNC_HVAC_THERMOSTAT_AUTO:
      return isAutoSupported();
    case SUPLA_CHANNELFNC_HVAC_FAN:
      return isFanSupported();
    case SUPLA_CHANNELFNC_HVAC_DRYER:
      return isDrySupported();
  }
  return false;
}

void HvacBase::addAlgorithmCap(unsigned _supla_int16_t algorithm) {
  config.AlgorithmCaps |= algorithm;
}

void HvacBase::setTemperatureInStruct(THVACTemperatureCfg *temperatures,
                                      unsigned _supla_int_t index,
                                      _supla_int16_t temperature) {
  if (temperatures == nullptr) {
    return;
  }

  // convert index bit number to array index
  int arrayIndex = 0;
  for (; arrayIndex < 24; arrayIndex++) {
    if (index & (1 << arrayIndex)) {
      break;
    }
  }

  if (arrayIndex >= 24) {
    return;
  }

  temperatures->Index |= index;
  temperatures->Temperature[arrayIndex] = temperature;
}

void HvacBase::setTemperatureRoomMin(_supla_int16_t temperature) {
  setTemperatureInStruct(
      &config.Temperatures, TEMPERATURE_ROOM_MIN, temperature);
}

void HvacBase::setTemperatureRoomMax(_supla_int16_t temperature) {
  setTemperatureInStruct(
      &config.Temperatures, TEMPERATURE_ROOM_MAX, temperature);
}

void HvacBase::setTemperatureHeaterCoolerMin(_supla_int16_t temperature) {
  setTemperatureInStruct(
      &config.Temperatures, TEMPERATURE_HEATER_COOLER_MIN, temperature);
}

void HvacBase::setTemperatureHeaterCoolerMax(_supla_int16_t temperature) {
  setTemperatureInStruct(
      &config.Temperatures, TEMPERATURE_HEATER_COOLER_MAX, temperature);
}

void HvacBase::setTemperatureHisteresisMin(_supla_int16_t temperature) {
  setTemperatureInStruct(
      &config.Temperatures, TEMPERATURE_HISTERESIS_MIN, temperature);
}

void HvacBase::setTemperatureHisteresisMax(_supla_int16_t temperature) {
  setTemperatureInStruct(
      &config.Temperatures, TEMPERATURE_HISTERESIS_MAX, temperature);
}

void HvacBase::setTemperatureAutoOffsetMin(_supla_int16_t temperature) {
  setTemperatureInStruct(
      &config.Temperatures, TEMPERATURE_AUTO_OFFSET_MIN, temperature);
}

void HvacBase::setTemperatureAutoOffsetMax(_supla_int16_t temperature) {
  setTemperatureInStruct(
      &config.Temperatures, TEMPERATURE_AUTO_OFFSET_MAX, temperature);
}


bool HvacBase::setTemperatureFreezeProtection(_supla_int16_t temperature) {
  if (!isTemperatureFreezeProtectionValid(temperature)) {
    return false;
  }
  setTemperatureInStruct(
      &config.Temperatures, TEMPERATURE_FREEZE_PROTECTION, temperature);
  return true;
}

bool HvacBase::setTemperatureHeatProtection(_supla_int16_t temperature) {
  if (!isTemperatureHeatProtectionValid(temperature)) {
    return false;
  }
  setTemperatureInStruct(
      &config.Temperatures, TEMPERATURE_HEAT_PROTECTION, temperature);
  return true;
}

bool HvacBase::setTemperatureEco(_supla_int16_t temperature) {
  if (!isTemperatureEcoValid(temperature)) {
    return false;
  }
  setTemperatureInStruct(
      &config.Temperatures, TEMPERATURE_ECO, temperature);
  return true;
}

bool HvacBase::setTemperatureComfort(_supla_int16_t temperature) {
  if (!isTemperatureComfortValid(temperature)) {
    return false;
  }
  setTemperatureInStruct(
      &config.Temperatures, TEMPERATURE_COMFORT, temperature);
  return true;
}

bool HvacBase::setTemperatureBoost(_supla_int16_t temperature) {
  if (!isTemperatureBoostValid(temperature)) {
    return false;
  }
  setTemperatureInStruct(
      &config.Temperatures, TEMPERATURE_BOOST, temperature);
  return true;
}

bool HvacBase::setTemperatureHisteresis(_supla_int16_t temperature) {
  if (!isTemperatureHisteresisValid(temperature)) {
    return false;
  }

  setTemperatureInStruct(
      &config.Temperatures, TEMPERATURE_HISTERESIS, temperature);
  return true;
}

bool HvacBase::setTemperatureAutoOffset(_supla_int16_t temperature) {
  if (!isTemperatureAutoOffsetValid(temperature)) {
    return false;
  }

  setTemperatureInStruct(
      &config.Temperatures, TEMPERATURE_AUTO_OFFSET, temperature);
  return true;
}

bool HvacBase::setTemperatureBelowAlarm(_supla_int16_t temperature) {
  if (!isTemperatureBelowAlarmValid(temperature)) {
    return false;
  }

  setTemperatureInStruct(
      &config.Temperatures, TEMPERATURE_BELOW_ALARM, temperature);
  return true;
}

bool HvacBase::setTemperatureAboveAlarm(_supla_int16_t temperature) {
  if (!isTemperatureAboveAlarmValid(temperature)) {
    return false;
  }

  setTemperatureInStruct(
      &config.Temperatures, TEMPERATURE_ABOVE_ALARM, temperature);
  return true;
}

bool HvacBase::setTemperatureHeaterCoolerMinSetpoint(
    _supla_int16_t temperature) {
  if (!isTemperatureHeaterCoolerMinSetpointValid(temperature)) {
    return false;
  }
  setTemperatureInStruct(&config.Temperatures,
                         TEMPERATURE_HEATER_COOLER_MIN_SETPOINT,
                         temperature);
  return true;
}

bool HvacBase::setTemperatureHeaterCoolerMaxSetpoint(
    _supla_int16_t temperature) {
  if (!isTemperatureHeaterCoolerMaxSetpointValid(temperature)) {
    return false;
  }
  setTemperatureInStruct(&config.Temperatures,
                         TEMPERATURE_HEATER_COOLER_MAX_SETPOINT,
                         temperature);
  return true;
}

_supla_int16_t HvacBase::getTemperatureRoomMin(
    THVACTemperatureCfg *temperatures) {
  return getTemperatureFromStruct(temperatures, TEMPERATURE_ROOM_MIN);
}

_supla_int16_t HvacBase::getTemperatureRoomMax(
    THVACTemperatureCfg *temperatures) {
  return getTemperatureFromStruct(temperatures, TEMPERATURE_ROOM_MAX);
}

_supla_int16_t HvacBase::getTemperatureHeaterCoolerMin(
    THVACTemperatureCfg *temperatures) {
  return getTemperatureFromStruct(temperatures, TEMPERATURE_HEATER_COOLER_MIN);
}

_supla_int16_t HvacBase::getTemperatureHeaterCoolerMax(
    THVACTemperatureCfg *temperatures) {
  return getTemperatureFromStruct(temperatures, TEMPERATURE_HEATER_COOLER_MAX);
}
_supla_int16_t HvacBase::getTemperatureHisteresisMin(
    THVACTemperatureCfg *temperatures) {
  return getTemperatureFromStruct(temperatures, TEMPERATURE_HISTERESIS_MIN);
}
_supla_int16_t HvacBase::getTemperatureHisteresisMax(
    THVACTemperatureCfg *temperatures) {
  return getTemperatureFromStruct(temperatures, TEMPERATURE_HISTERESIS_MAX);
}
_supla_int16_t HvacBase::getTemperatureAutoOffsetMin(
    THVACTemperatureCfg *temperatures) {
  return getTemperatureFromStruct(temperatures, TEMPERATURE_AUTO_OFFSET_MIN);
}
_supla_int16_t HvacBase::getTemperatureAutoOffsetMax(
    THVACTemperatureCfg *temperatures) {
  return getTemperatureFromStruct(temperatures, TEMPERATURE_AUTO_OFFSET_MAX);
}

_supla_int16_t HvacBase::getTemperatureRoomMin() {
  return getTemperatureRoomMin(&config.Temperatures);
}

_supla_int16_t HvacBase::getTemperatureRoomMax() {
  return getTemperatureRoomMax(&config.Temperatures);
}

_supla_int16_t HvacBase::getTemperatureHeaterCoolerMin() {
  return getTemperatureHeaterCoolerMin(&config.Temperatures);
}

_supla_int16_t HvacBase::getTemperatureHeaterCoolerMax() {
  return getTemperatureHeaterCoolerMax(&config.Temperatures);
}

_supla_int16_t HvacBase::getTemperatureHisteresisMin() {
  return getTemperatureHisteresisMin(&config.Temperatures);
}

_supla_int16_t HvacBase::getTemperatureHisteresisMax() {
  return getTemperatureHisteresisMax(&config.Temperatures);
}

_supla_int16_t HvacBase::getTemperatureAutoOffsetMin() {
  return getTemperatureAutoOffsetMin(&config.Temperatures);
}

_supla_int16_t HvacBase::getTemperatureAutoOffsetMax() {
  return getTemperatureAutoOffsetMax(&config.Temperatures);
}

_supla_int16_t HvacBase::getTemperatureFreezeProtection(
    THVACTemperatureCfg *temperatures) {
  return getTemperatureFromStruct(temperatures, TEMPERATURE_FREEZE_PROTECTION);
}

_supla_int16_t HvacBase::getTemperatureHeatProtection(
    THVACTemperatureCfg *temperatures) {
  return getTemperatureFromStruct(temperatures, TEMPERATURE_HEAT_PROTECTION);
}

_supla_int16_t HvacBase::getTemperatureEco(THVACTemperatureCfg *temperatures) {
  return getTemperatureFromStruct(temperatures, TEMPERATURE_ECO);
}

_supla_int16_t HvacBase::getTemperatureComfort(
    THVACTemperatureCfg *temperatures) {
  return getTemperatureFromStruct(temperatures, TEMPERATURE_COMFORT);
}

_supla_int16_t HvacBase::getTemperatureBoost(
    THVACTemperatureCfg *temperatures) {
  return getTemperatureFromStruct(temperatures, TEMPERATURE_BOOST);
}

_supla_int16_t HvacBase::getTemperatureHisteresis(
    THVACTemperatureCfg *temperatures) {
  return getTemperatureFromStruct(temperatures, TEMPERATURE_HISTERESIS);
}

_supla_int16_t HvacBase::getTemperatureAutoOffset(
    THVACTemperatureCfg *temperatures) {
  return getTemperatureFromStruct(temperatures, TEMPERATURE_AUTO_OFFSET);
}

_supla_int16_t HvacBase::getTemperatureBelowAlarm(
    THVACTemperatureCfg *temperatures) {
  return getTemperatureFromStruct(temperatures, TEMPERATURE_BELOW_ALARM);
}

_supla_int16_t HvacBase::getTemperatureAboveAlarm(
    THVACTemperatureCfg *temperatures) {
  return getTemperatureFromStruct(temperatures, TEMPERATURE_ABOVE_ALARM);
}

_supla_int16_t HvacBase::getTemperatureHeaterCoolerMinSetpoint(
    THVACTemperatureCfg *temperatures) {
  return getTemperatureFromStruct(temperatures,
                                  TEMPERATURE_HEATER_COOLER_MIN_SETPOINT);
}

_supla_int16_t HvacBase::getTemperatureHeaterCoolerMaxSetpoint(
    THVACTemperatureCfg *temperatures) {
  return getTemperatureFromStruct(temperatures,
                                  TEMPERATURE_HEATER_COOLER_MAX_SETPOINT);
}

_supla_int16_t HvacBase::getTemperatureFreezeProtection() {
  return getTemperatureFreezeProtection(&config.Temperatures);
}

_supla_int16_t HvacBase::getTemperatureHeatProtection() {
  return getTemperatureHeatProtection(&config.Temperatures);
}

_supla_int16_t HvacBase::getTemperatureEco() {
  return getTemperatureEco(&config.Temperatures);
}

_supla_int16_t HvacBase::getTemperatureComfort() {
  return getTemperatureComfort(&config.Temperatures);
}

_supla_int16_t HvacBase::getTemperatureBoost() {
  return getTemperatureBoost(&config.Temperatures);
}

_supla_int16_t HvacBase::getTemperatureHisteresis() {
  return getTemperatureHisteresis(&config.Temperatures);
}

_supla_int16_t HvacBase::getTemperatureAutoOffset() {
  return getTemperatureAutoOffset(&config.Temperatures);
}

_supla_int16_t HvacBase::getTemperatureBelowAlarm() {
  return getTemperatureBelowAlarm(&config.Temperatures);
}

_supla_int16_t HvacBase::getTemperatureAboveAlarm() {
  return getTemperatureAboveAlarm(&config.Temperatures);
}

_supla_int16_t HvacBase::getTemperatureHeaterCoolerMinSetpoint() {
  return getTemperatureHeaterCoolerMinSetpoint(&config.Temperatures);
}

_supla_int16_t HvacBase::getTemperatureHeaterCoolerMaxSetpoint() {
  return getTemperatureHeaterCoolerMaxSetpoint(&config.Temperatures);
}

bool HvacBase::setUsedAlgorithm(unsigned _supla_int16_t newAlgorithm) {
  if (isAlgorithmValid(newAlgorithm)) {
    config.UsedAlgorithm = newAlgorithm;
    return true;
  }
  return false;
}

unsigned _supla_int16_t HvacBase::getUsedAlgorithm() const {
  return config.UsedAlgorithm;
}

bool HvacBase::setMainThermometerChannelNo(uint8_t channelNo) {
  if (isChannelThermometer(channelNo)) {
    if (getHeaterCoolerThermometerType() !=
        SUPLA_HVAC_HEATER_COOLER_THERMOMETER_TYPE_NOT_SET) {
      if (channelNo == getHeaterCoolerThermometerChannelNo()) {
        return false;
      }
    }
    config.MainThermometerChannelNo = channelNo;
    return true;
  }
  return false;
}

uint8_t HvacBase::getMainThermometerChannelNo() const {
  return config.MainThermometerChannelNo;
}

bool HvacBase::setHeaterCoolerThermometerChannelNo(uint8_t channelNo) {
  if (isChannelThermometer(channelNo)) {
    if (getMainThermometerChannelNo() == channelNo) {
      return false;
    }
    config.HeaterCoolerThermometerChannelNo = channelNo;
    if (getHeaterCoolerThermometerType() ==
        SUPLA_HVAC_HEATER_COOLER_THERMOMETER_TYPE_NOT_SET) {
      setHeaterCoolerThermometerType(
          SUPLA_HVAC_HEATER_COOLER_THERMOMETER_TYPE_DISALBED);
    }
    return true;
  }
  if (getChannelNumber() == channelNo) {
    config.HeaterCoolerThermometerChannelNo = channelNo;
    setHeaterCoolerThermometerType(
        SUPLA_HVAC_HEATER_COOLER_THERMOMETER_TYPE_NOT_SET);
    return true;
  }
  return false;
}

uint8_t HvacBase::getHeaterCoolerThermometerChannelNo() const {
  return config.HeaterCoolerThermometerChannelNo;
}

void HvacBase::setHeaterCoolerThermometerType(uint8_t type) {
  config.HeaterCoolerThermometerType = type;
}

uint8_t HvacBase::getHeaterCoolerThermometerType() const {
  return config.HeaterCoolerThermometerType;
}

void HvacBase::setAntiFreezeAndHeatProtectionEnabled(bool enabled) {
  config.EnableAntiFreezeAndOverheatProtection = enabled;
}

bool HvacBase::isAntiFreezeAndHeatProtectionEnabled() const {
  return config.EnableAntiFreezeAndOverheatProtection;
}

bool HvacBase::isMinOnOffTimeValid(uint16_t seconds) const {
  return seconds <= 600;  // TODO(klew): is this range ok? from 1 s to 10 min
                          // 0 - disabled
}

bool HvacBase::setMinOnTimeS(uint16_t seconds) {
  if (isMinOnOffTimeValid(seconds)) {
    config.MinOnTimeS = seconds;
    return true;
  }
  return false;
}

uint16_t HvacBase::getMinOnTimeS() const {
  return config.MinOnTimeS;
}

bool HvacBase::setMinOffTimeS(uint16_t seconds) {
  if (isMinOnOffTimeValid(seconds)) {
    config.MinOffTimeS = seconds;
    return true;
  }
  return false;
}

uint16_t HvacBase::getMinOffTimeS() const {
  return config.MinOffTimeS;
}

void HvacBase::saveConfig() {
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    // Generic HVAC configuration
    char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    generateKey(key, "hvac_cfg");
    if (cfg->setBlob(key,
                     reinterpret_cast<char *>(&config),
                     sizeof(TSD_ChannelConfig_HVAC))) {
      SUPLA_LOG_INFO("HVAC config saved successfully");
      cfg->saveWithDelay(1000);
    } else {
      SUPLA_LOG_INFO("HVAC failed to save config");
    }
  }
}

void HvacBase::saveWeeklySchedule() {
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    // Weekly schedule configuration
    generateKey(key, "hvac_weekly");
    if (cfg->setBlob(key,
                     reinterpret_cast<char *>(&weeklySchedule),
                     sizeof(TSD_ChannelConfig_WeeklySchedule))) {
      SUPLA_LOG_INFO("HVAC weekly schedule saved successfully");
      cfg->saveWithDelay(1000);
    } else {
      SUPLA_LOG_INFO("HVAC failed to save weekly schedule");
    }
  }
}
